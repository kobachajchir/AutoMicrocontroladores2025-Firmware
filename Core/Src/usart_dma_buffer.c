#include <string.h>
#include <stdbool.h>
#include "types/usart_buffer_type.h"
#include "usart_dma_buffer.h"
#include "stm32f1xx_hal.h"

/* Reconstrucción de flags internos en bufPtr->flags.byte:
 *  bit0: TX_BUSY   → hay un envío en curso
 *  bit1: TX_FULL   → cadena > 64 bytes o intentó encolar con TX_BUSY=1
 *  bit2: RX_FULL   → overflow en RX circular
 */
#define USART_FLAG_TX_BUSY   (1U << 0)
#define USART_FLAG_TX_FULL   (1U << 1)
#define USART_FLAG_RX_FULL   (1U << 2)

static USART1_TxDMA_Callback txDmaCallback = NULL;
static USART1_RxDMA_Callback rxDmaCallback = NULL;
static USART1_RxHandler       rxHandler     = NULL;
static UART_HandleTypeDef    *uartHandle    = NULL;
static USART_Buffer_t        *bufPtr        = NULL;

// Número de bytes que la última llamada solicitó al DMA (≤64)
static uint16_t lastTxLen = 0;

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
 * @brief  Asigna los callbacks de arranque DMA TX (Normal) y RX (Circular).
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
 * @brief  Inicializa el buffer interno y arranca recepción RX circular (64 bytes).
 */
HAL_StatusTypeDef USART1_DMA_Init(USART_Buffer_t *buf)
{
    if (buf == NULL || rxDmaCallback == NULL) {
        return HAL_ERROR;
    }
    bufPtr = buf;

    // Inicializar índices y flags (solo usamos flags.byte para TX_BUSY y RX_FULL)
    bufPtr->headTx = 0;
    bufPtr->tailTx = 0;
    bufPtr->headRx = 0;
    bufPtr->tailRx = 0;
    bufPtr->flags.byte = 0;

    // Limpiar buffers
    memset(bufPtr->txBuffer, 0, USART1_BUFFER_SIZE);
    memset(bufPtr->rxBuffer, 0, USART1_BUFFER_SIZE);

    // Iniciar recepción RX circular (64 bytes)
    return rxDmaCallback(bufPtr->rxBuffer, USART1_BUFFER_SIZE);
}

/**
 * @brief  Encola un C-string (hasta 64 bytes) en el buffer lineal TX.
 */
HAL_StatusTypeDef USART1_PushTxString(USART_Buffer_t *buf, const char *str)
{
    if (buf == NULL || txDmaCallback == NULL) {
        return HAL_ERROR;
    }

    size_t len = strlen(str);
    if (len == 0) {
        return HAL_OK;  // nada que hacer
    }

    // 1) Si ya hay transmisión en curso, rechazamos inmediatamente (no circula)
    if (buf->flags.byte & USART_FLAG_TX_BUSY) {
        return HAL_BUSY;
    }

    // 2) Si la cadena excede 64 bytes, rechazamos
    if (len > USART1_BUFFER_SIZE) {
        buf->flags.byte |= USART_FLAG_TX_FULL;
        return HAL_BUSY;
    }

    // 3) Buffer vacío: copiamos directamente de txBuffer[0..len-1]
    memcpy(buf->txBuffer, str, len);

    // 4) Marcar “transmisión en curso”
    buf->flags.byte |= USART_FLAG_TX_BUSY;

    // 5) Guardar longitud para el callback
    lastTxLen = (uint16_t)len;

    // 6) Arrancar el DMA normal para enviar len bytes desde txBuffer[0]
    if (txDmaCallback(buf->txBuffer, lastTxLen) != HAL_OK) {
        // Si falla arrancar el DMA, despejar TX_BUSY y error
        buf->flags.byte &= ~USART_FLAG_TX_BUSY;
        return HAL_ERROR;
    }

    // 7) Habilitar UART_IT_TC para que, al sacar el último bit de la UART,
    //    salte la interrupción de “Transmit Complete” (TC) y llame a nuestro callback.
    __HAL_UART_ENABLE_IT(uartHandle, UART_IT_TC);

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
 *         Extrae cada byte de rxBuffer (llenado por DMA Circular) y llama a rxHandler().
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

/**
 * @brief  Callback que la HAL invoca cuando el UART genera “Transmit Complete” (TC).
 *         Debe enlazarse en stm32f1xx_it.c:
 *           void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
 *               USART1_DMA_TxCpltHandler(huart);
 *           }
 *
 *  Aquí simplemente despejamos TX_BUSY y deshabilitamos TCIE.
 */
void USART1_DMA_TxCpltHandler(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1 || bufPtr == NULL) {
        return;
    }

    // 1) Limpiar TX_BUSY
    bufPtr->flags.byte &= ~USART_FLAG_TX_BUSY;

    // 2) Deshabilitar UART_IT_TC para que no se siga interrumpiendo “en vacío”
    __HAL_UART_DISABLE_IT(uartHandle, UART_IT_TC);

    // No hay más datos pendientes (en este esquema lineal).
    // El DMA Normal ya se detuvo solo cuando terminó de copiar los 'lastTxLen' bytes.
}

/* ---------------------- CALLBACKS HAL “weak” por defecto ----------------------------- */
/**
 * @brief  HAL por defecto: overrideable.
 */
__weak void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    USART1_DMA_TxCpltHandler(huart);
}

/**
 * @brief  Cuando el DMA RX circular llenó 32 bytes.
 */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1 && bufPtr != NULL) {
        bufPtr->headRx = (uint8_t)((bufPtr->headRx + (USART1_BUFFER_SIZE/2)) % USART1_BUFFER_SIZE);
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
        bufPtr->headRx = (uint8_t)((bufPtr->headRx + (USART1_BUFFER_SIZE/2)) % USART1_BUFFER_SIZE);
        if (bufPtr->headRx == bufPtr->tailRx) {
            bufPtr->flags.byte |= USART_FLAG_RX_FULL;
        }
        if (rxDmaCallback != NULL) {
            rxDmaCallback(bufPtr->rxBuffer, USART1_BUFFER_SIZE);
        }
    }
}

