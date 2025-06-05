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
 * @brief  Encola un C-string (sin '\0') en la cola circular TX.
 *         El string debe tener longitud ≤ 64. Luego arranca el DMA CIRCULAR (64 bytes).
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
    if (len > USART1_FreeSpaceTx(buf)) {
        buf->flags.byte |= USART_FLAG_TX_FULL;
        return HAL_BUSY;
    }

    /* 1) Copiar en modo circular */
    for (size_t i = 0; i < len; i++) {
        buf->txBuffer[ buf->headTx ] = (uint8_t)str[i];
        buf->headTx = (uint8_t)((buf->headTx + 1) % USART1_BUFFER_SIZE);
    }
    buf->flags.byte &= ~USART_FLAG_TX_FULL;

    /* 2) Preparar HT/TC y marcar BUSY */
    pendingBytes = (uint16_t)len;
    sentBytes    = 0;
    firstRound   = true;

    if ((buf->flags.byte & USART_FLAG_TX_BUSY) == 0) {
        buf->flags.byte |= USART_FLAG_TX_BUSY;
        /* 3) LLamada EXACTA a HAL_UART_Transmit_DMA, para reactivar DMAT */
        if (txDmaCallback(buf->txBuffer, USART1_BUFFER_SIZE) != HAL_OK) {
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
 * @brief  LLamado desde HAL_UART_TxHalfCpltCallback:
 *         - Se ejecuta cuando el DMA ha mandado 32 bytes (la mitad).
 *         - Si ya completamos el string (pendingBytes ≤ 32), detenemos el DMA.
 */
void USART1_DMA_TxHalfCpltHandler(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1 || bufPtr == NULL) {
        return;
    }

    sentBytes += (USART1_BUFFER_SIZE / 2);  // +32

    if (firstRound && (sentBytes >= pendingBytes)) {
        // Hemos enviado todo el string (pendingBytes ≤ 32).
        firstRound = false;

        // 1) Avanzar tailTx = headTx para marcar buffer como vacío
        bufPtr->tailTx = bufPtr->headTx;

        // 2) Limpiar flags TX_BUSY y TX_FULL
        bufPtr->flags.byte &= ~(USART_FLAG_TX_BUSY | USART_FLAG_TX_FULL);

        // 3) Detener el DMA Circular
        if (uartHandle != NULL) {
            // Detener DMA:
            CLEAR_BIT(uartHandle->Instance->CR3, USART_CR3_DMAT);
            HAL_UART_DMAStop(uartHandle);
            // --------------------------------------------------------------------------------
            //  AÑADIDO: restaurar el estado interno para permitir nuevos HAL_UART_Transmit_DMA:
            uartHandle->gState = HAL_UART_STATE_READY;
            __HAL_UNLOCK(uartHandle);
            // --------------------------------------------------------------------------------
        }
    }
}

/* ---------------------- CALLBACK: TRANSFER COMPLETE ----------------------------- */
/**
 * @brief  LLamado desde HAL_UART_TxCpltCallback:
 *         - Se ejecuta cuando el DMA ha mandado 64 bytes (buffer completo).
 *         - Si el string (pendingBytes) estaba entre 33 y 64, detenemos el DMA.
 */
void USART1_DMA_TxCpltHandler(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1 || bufPtr == NULL) {
        return;
    }

    sentBytes += (USART1_BUFFER_SIZE / 2);  // +32 => total +64

    if (firstRound && (sentBytes >= pendingBytes)) {
        // El string tenía entre 33 y 64 bytes, completamos envío justo al llegar a 64.
        firstRound = false;

        // 1) Avanzar tailTx = headTx para vaciar buffer
        bufPtr->tailTx = bufPtr->headTx;

        // 2) Limpiar flags TX_BUSY y TX_FULL
        bufPtr->flags.byte &= ~(USART_FLAG_TX_BUSY | USART_FLAG_TX_FULL);

        // 3) Detener el DMA Circular
        if (uartHandle != NULL) {
            // Detener DMA:
            CLEAR_BIT(uartHandle->Instance->CR3, USART_CR3_DMAT);
            HAL_UART_DMAStop(uartHandle);
            // --------------------------------------------------------------------------------
            //  AÑADIDO: restaurar el estado interno para permitir nuevos HAL_UART_Transmit_DMA:
            uartHandle->gState = HAL_UART_STATE_READY;
            __HAL_UNLOCK(uartHandle);
            // --------------------------------------------------------------------------------
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

