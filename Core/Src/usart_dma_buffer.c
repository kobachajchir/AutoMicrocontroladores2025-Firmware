/**
 * @file    usart_dma_buffer.c
 * @brief   Librería USART1 + DMA circular (64B) con HT/TC para enviar exactamente lo que haya en cola.
 */

#include <string.h>
#include <stdbool.h>
#include "types/usart_buffer_type.h"
#include "usart_dma_buffer.h"
#include "stm32f1xx_hal.h"

/* Reconstruimos los flags internos:
 *  bit0: TX_BUSY   → hay un envío en curso
 *  bit1: TX_FULL   → no cabe más data en la cola
 *  bit2: RX_FULL   → cola RX rebasada (overflow)
 */
#define USART_FLAG_TX_BUSY   (1U << 0)  // bit0
#define USART_FLAG_TX_FULL   (1U << 1)  // bit1
#define USART_FLAG_RX_FULL   (1U << 2)  // bit2

// Callback pointers
static USART1_TxDMA_Callback txDmaCallback = NULL;
static USART1_RxDMA_Callback rxDmaCallback = NULL;
static USART1_RxHandler       rxHandler     = NULL;
static UART_HandleTypeDef    *uartHandle    = NULL;
static USART_Buffer_t        *bufPtr        = NULL;

/*
 * Variables auxiliares HT/TC:
 *  pendingBytes = número real de bytes encolados (≤64)
 *  sentBytes    = cuántos bytes ya mandó el DMA hasta HT/TC
 *  firstRound   = true durante la primera ronda, luego se despeja
 */
static uint16_t pendingBytes = 0;
static uint16_t sentBytes    = 0;
static bool     firstRound   = false;

/**
 * @brief  Registra el handle UART1 (debe llamarse tras MX_USART1_UART_Init).
 */
HAL_StatusTypeDef USART1_RegisterHandle(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        return HAL_ERROR;
    }
    uartHandle = huart;
    return HAL_OK;
}

/**
 * @brief  Asigna los callbacks de arranque DMA (TX circular 64 / RX circular 64).
 */
HAL_StatusTypeDef USART1_SetDMACallbacks(USART1_TxDMA_Callback txCb,
                                         USART1_RxDMA_Callback rxCb)
{
    if (txCb == NULL || rxCb == NULL) {
        return HAL_ERROR;
    }
    txDmaCallback = txCb;
    rxDmaCallback = rxCb;
    return HAL_OK;
}

/**
 * @brief  Inicializa índices/flags e inicia recepción RX circular (64 bytes).
 */
HAL_StatusTypeDef USART1_DMA_Init(USART_Buffer_t *buf)
{
    if (buf == NULL || rxDmaCallback == NULL) {
        return HAL_ERROR;
    }
    bufPtr = buf;

    bufPtr->headTx = 0;
    bufPtr->tailTx = 0;
    bufPtr->headRx = 0;
    bufPtr->tailRx = 0;
    bufPtr->txCount = 0;
    bufPtr->flags.byte = 0;

    /* Limpiar ambos buffers para evitar datos basura */
    memset(bufPtr->txBuffer, 0, USART1_BUFFER_SIZE);
    memset(bufPtr->rxBuffer, 0, USART1_BUFFER_SIZE);

    /* Iniciar recepción RX circular de 64 bytes */
    return rxDmaCallback(bufPtr->rxBuffer, USART1_BUFFER_SIZE);
}

/**
 * @brief  Calcula cuántos bytes hay en cola TX (entre tailTx y headTx).
 */
static uint16_t USART1_TxQueued(const USART_Buffer_t *buf)
{
    if (buf->headTx >= buf->tailTx) {
        return (uint16_t)(buf->headTx - buf->tailTx);
    } else {
        return (uint16_t)(USART1_BUFFER_SIZE - (buf->tailTx - buf->headTx));
    }
}

/**
 * @brief  Calcula cuánto espacio libre queda en la cola TX (64 - encolados).
 */
static uint16_t USART1_FreeSpaceTx(const USART_Buffer_t *buf)
{
    return (uint16_t)(USART1_BUFFER_SIZE - USART1_TxQueued(buf));
}

/**
 * @brief  Encola un C-string (sin '\0') en el buffer circular TX.
 *         Si cabe (≤64 bytes en total), copia byte a byte sin importar si TX_BUSY=1.
 *         Sólo arranca el DMA si TX_BUSY==0.
 * @retval HAL_OK   si encoló (y arrancó DMA si estaba libre).
 *         HAL_BUSY si no hay espacio suficiente (buffer lleno).
 *         HAL_ERROR en caso de error fatal.
 */
HAL_StatusTypeDef USART1_PushTxString(USART_Buffer_t *buf, const char *str)
{
    if (buf == NULL || txDmaCallback == NULL) {
        return HAL_ERROR;
    }

    size_t len = strlen(str);
    if (len == 0) {
        return HAL_OK;
    }

    // Si el string no cabe completo en el buffer circular, rechazamos:
    if (len > USART1_FreeSpaceTx(buf)) {
        buf->flags.byte |= USART_FLAG_TX_FULL;
        return HAL_BUSY;
    }

    // 1) Copiar en modo circular, sin fijarnos en TX_BUSY:
    for (size_t i = 0; i < len; i++) {
        buf->txBuffer[ buf->headTx ] = (uint8_t)str[i];
        buf->headTx = (uint8_t)((buf->headTx + 1) % USART1_BUFFER_SIZE);
    }
    buf->flags.byte &= ~USART_FLAG_TX_FULL;  // despejamos TX_FULL porque ahora sí cabe

    // 2) Actualizar cuántos bytes pendientes en total (entre tailTx y headTx):
    //    (esto no es estrictamente necesario si usas HT/TC para vaciar, pero puede servir para debug).
    buf->txCount = (uint8_t)USART1_TxQueued(buf);

    // 3) Preparar variables HT/TC (sólo si era un envío “nuevo”):
    pendingBytes = (uint16_t)buf->txCount;
    sentBytes    = 0;
    firstRound   = true;

    // 4) Si NO había envío en curso, arrancamos el DMA CIRCULAR 64:
    if ((buf->flags.byte & USART_FLAG_TX_BUSY) == 0) {
        buf->flags.byte |= USART_FLAG_TX_BUSY;
        if (txDmaCallback(buf->txBuffer, USART1_BUFFER_SIZE) != HAL_OK) {
            // si falla al arrancar el DMA, liberamos la marca TX_BUSY
            buf->flags.byte &= ~USART_FLAG_TX_BUSY;
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

/**
 * @brief  Registra el callback que procesará cada byte recibido (RX).
 */
HAL_StatusTypeDef USART1_SetRxHandler(USART1_RxHandler handler)
{
    if (handler == NULL) {
        return HAL_ERROR;
    }
    rxHandler = handler;
    return HAL_OK;
}

/**
 * @brief  Debe llamarse periódicamente en el bucle principal.
 *         Extrae cada byte de rxBuffer y llama a rxHandler(byte).
 */
void USART1_Update(void)
{
    if (bufPtr == NULL || rxHandler == NULL) {
        return;
    }
    uint8_t c;
    while (bufPtr->headRx != bufPtr->tailRx) {
        c = bufPtr->rxBuffer[ bufPtr->tailRx ];
        bufPtr->tailRx = (uint8_t)((bufPtr->tailRx + 1) % USART1_BUFFER_SIZE);
        rxHandler(c);
    }
}

/* ---------------------- CALLBACK: HALF TRANSFER COMPLETE ----------------------------- */
/**
 * @brief  Handler que se llama en HAL_UART_TxHalfCpltCallback (32 bytes enviados).
 *         - Reduce pendingBytes en 32, avanza tailTx en 32.
 *         - Si luego pendingBytes > 0, relanza DMA (segunda mitad).
 *         - Si pendingBytes == 0, detiene completamente el DMA y libera TX_BUSY.
 */
void USART1_DMA_TxHalfCpltHandler(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1 || bufPtr == NULL) {
        return;
    }

    // 1) Ya se enviaron 32 bytes:
    sentBytes += (USART1_BUFFER_SIZE / 2);  // 32
    if (pendingBytes >= (USART1_BUFFER_SIZE / 2)) {
        pendingBytes -= (USART1_BUFFER_SIZE / 2);
    } else {
        pendingBytes = 0;
    }

    // 2) Avanzar tailTx en 32 (liberamos esos bytes):
    for (int i = 0; i < (USART1_BUFFER_SIZE/2); i++) {
        bufPtr->tailTx = (uint8_t)((bufPtr->tailTx + 1) % USART1_BUFFER_SIZE);
    }

    // 3) ¿Quedan bytes por enviar?
    if (pendingBytes > 0) {
        // Aún hay más data: relanzamos el DMA para los próximos 32→64
        // (En circular, la segunda mitad corresponde a las siguientes direcciones).
        if (txDmaCallback(&bufPtr->txBuffer[ bufPtr->tailTx ],
                          (uint16_t)(USART1_BUFFER_SIZE / 2)) != HAL_OK)
        {
            // Si falla al relanzar, marcamos error y liberamos TX_BUSY:
            bufPtr->flags.byte &= ~USART_FLAG_TX_BUSY;
        }
        // Atención: no limpiamos TX_BUSY hasta que se complete todo (HT + TC).
    }
    else {
        // Ya no queda nada en cola → detenemos el DMA y limpiamos TX_BUSY:
        bufPtr->flags.byte &= ~USART_FLAG_TX_BUSY;

        if (uartHandle != NULL) {
            CLEAR_BIT(uartHandle->Instance->CR3, USART_CR3_DMAT);
            HAL_UART_DMAStop(uartHandle);
            // Restaurar estado HAL:
            uartHandle->gState = HAL_UART_STATE_READY;
            __HAL_UNLOCK(uartHandle);
        }
    }
}

/**
 * @brief  Handler que se llama en HAL_UART_TxCpltCallback (64 bytes enviados).
 *         - Reduce pendingBytes en 32, avanza tailTx en 32 (restantes).
 *         - Si luego pendingBytes > 0 (mensaje >64), relanza con la mitad siguiente.
 *         - Si pendingBytes == 0, detiene el DMA y libera TX_BUSY.
 */
void USART1_DMA_TxCpltHandler(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1 || bufPtr == NULL) {
        return;
    }

    // 1) Se enviaron otros 32 bytes (segunda mitad de los 64):
    sentBytes += (USART1_BUFFER_SIZE / 2);  // otro +32
    if (pendingBytes >= (USART1_BUFFER_SIZE / 2)) {
        pendingBytes -= (USART1_BUFFER_SIZE / 2);
    } else {
        pendingBytes = 0;
    }

    // 2) Avanzar tailTx en 32 más:
    for (int i = 0; i < (USART1_BUFFER_SIZE/2); i++) {
        bufPtr->tailTx = (uint8_t)((bufPtr->tailTx + 1) % USART1_BUFFER_SIZE);
    }

    // 3) ¿Queda aún data tras los primeros 64? (caso futuro >64 bytes)
    if (pendingBytes > 0) {
        // Relanzar el DMA para la siguiente “mitad” de 64 (en modo circular):
        if (txDmaCallback(&bufPtr->txBuffer[ bufPtr->tailTx ],
                          (uint16_t)(USART1_BUFFER_SIZE / 2)) != HAL_OK)
        {
            bufPtr->flags.byte &= ~USART_FLAG_TX_BUSY;
        }
        // Quedará pendiente el TC final posterior.
    }
    else {
        // Ya no quedan bytes por enviar → terminamos:
        bufPtr->flags.byte &= ~USART_FLAG_TX_BUSY;

        if (uartHandle != NULL) {
            CLEAR_BIT(uartHandle->Instance->CR3, USART_CR3_DMAT);
            HAL_UART_DMAStop(uartHandle);
            // Restaurar estado HAL:
            uartHandle->gState = HAL_UART_STATE_READY;
            __HAL_UNLOCK(uartHandle);
        }
    }
}


/* ---------------------- CALLBACKS HAL "weak" ----------------------------- */
/**
 * @brief  Callback HAL por defecto (half transfer). Invoca nuestro handler.
 */
__weak void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    USART1_DMA_TxHalfCpltHandler(huart);
}

/**
 * @brief  Callback HAL por defecto (full transfer). Invoca nuestro handler.
 */
__weak void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    USART1_DMA_TxCpltHandler(huart);
}

/* ---------------------- CALLBACKS RX (sin cambios) ----------------------------- */
/**
 * @brief  Cuando el DMA RX circular llenó 32 bytes.
 */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1 && bufPtr != NULL) {
        bufPtr->headRx = (uint8_t)((bufPtr->headRx + (USART1_BUFFER_SIZE / 2)) % USART1_BUFFER_SIZE);
        if (bufPtr->headRx == bufPtr->tailRx) {
            bufPtr->flags.byte |= USART_FLAG_RX_FULL;
        }
    }
}

/**
 * @brief  Cuando el DMA RX circular llenó 64 bytes.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1 && bufPtr != NULL) {
        bufPtr->headRx = (uint8_t)((bufPtr->headRx + (USART1_BUFFER_SIZE / 2)) % USART1_BUFFER_SIZE);
        if (bufPtr->headRx == bufPtr->tailRx) {
            bufPtr->flags.byte |= USART_FLAG_RX_FULL;
        }
        if (rxDmaCallback != NULL) {
            rxDmaCallback(bufPtr->rxBuffer, USART1_BUFFER_SIZE);
        }
    }
}

