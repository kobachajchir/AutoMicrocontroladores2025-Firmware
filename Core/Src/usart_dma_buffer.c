/**
 * @file    usart_dma_buffer.c
 * @brief   Implementación de la librería USART1 + DMA + buffer circular de 64 bytes.
 */

#include <string.h>
#include <stdbool.h>
#include "types/usart_buffer_type.h"  // Define: Byte_Flag_Struct, USART1_BUFFER_SIZE = 64
#include "usart_dma_buffer.h"
#include "stm32f1xx_hal.h"

/* Definición de flags dentro de 'flags.byte' para estado interno */
#define USART_FLAG_TX_FULL   (1U << 0)
#define USART_FLAG_RX_FULL   (1U << 1)
#define USART_FLAG_TX_BUSY   (1U << 2)

/* Punteros a callbacks y estado interno */
static USART1_TxDMA_Callback    txDmaCallback = NULL;
static USART1_RxDMA_Callback    rxDmaCallback = NULL;
static USART1_RxHandler         rxHandler     = NULL;
static USART_Buffer_t          *bufPtr        = NULL;
static UART_HandleTypeDef      *uartHandle    = NULL;

/* Variables para gestionar la cola circular TX: */
static uint8_t  headSentUntil = 0;   // “snapshot” de headTx al iniciar un bloque DMA
static uint16_t lastTxChunkLen = 0;  // longitud del bloque que el DMA está enviando
static bool     sending       = false; // true = DMA TX en curso

/**
 * @brief  Registra el puntero al UART_HandleTypeDef que usará la librería para TX/RX.
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
 * @brief  Asigna las funciones de arranque DMA TX/RX que la librería utilizará internamente.
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
 * @brief  Inicializa índices/flags e invoca al callback RX para arrancar recepción circular.
 */
HAL_StatusTypeDef USART1_DMA_Init(USART_Buffer_t *buf)
{
    if (buf == NULL || rxDmaCallback == NULL) {
        return HAL_ERROR;
    }
    bufPtr = buf;

    /* Inicializar índices y flags de TX y RX */
    bufPtr->headTx  = 0;
    bufPtr->tailTx  = 0;
    bufPtr->headRx  = 0;
    bufPtr->tailRx  = 0;
    bufPtr->flags.byte = 0;

    /* Limpiar buffers (TX y RX) */
    memset(bufPtr->txBuffer, 0, USART1_BUFFER_SIZE);
    memset(bufPtr->rxBuffer, 0, USART1_BUFFER_SIZE);

    /* Arrancar RX DMA en modo CIRCULAR (64 bytes) */
    return rxDmaCallback(bufPtr->rxBuffer, USART1_BUFFER_SIZE);
}

/**
 * @brief  Calcula cuántos bytes libres hay en el buffer TX circular de 64 bytes.
 */
static uint16_t USART1_FreeSpaceTx(const USART_Buffer_t *buf)
{
    uint16_t used;
    if (buf->headTx >= buf->tailTx) {
        used = buf->headTx - buf->tailTx;
    } else {
        used = (uint16_t)USART1_BUFFER_SIZE - (buf->tailTx - buf->headTx);
    }
    return (uint16_t)(USART1_BUFFER_SIZE - used);
}

/**
 * @brief  Encola un C-string para transmisión no bloqueante por USART1 + DMA (modo CIRCULAR).
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
    if (len > USART1_BUFFER_SIZE) {
        // Nunca cabrá más de 64 bytes
        buf->flags.byte |= USART_FLAG_TX_FULL;
        return HAL_BUSY;
    }

    /* 1) Verificar si cabe entero en el espacio libre circular */
    uint16_t freeSpace = USART1_FreeSpaceTx(buf);
    if ((uint16_t)len > freeSpace) {
        buf->flags.byte |= USART_FLAG_TX_FULL;
        return HAL_BUSY;
    }

    /* 2) Copiar la cadena a txBuffer[] en modo circular desde tailTx */
    uint8_t oldTailIdx = buf->tailTx;
    if ((uint16_t)(oldTailIdx + len) <= USART1_BUFFER_SIZE) {
        // Bloque contiguo: [oldTailIdx .. oldTailIdx+len-1]
        memcpy(&buf->txBuffer[oldTailIdx], str, len);
        buf->tailTx = (uint8_t)(oldTailIdx + len);
    } else {
        // Wrap: copiar hasta el final (64), luego el resto al inicio
        uint16_t firstPart  = USART1_BUFFER_SIZE - oldTailIdx;
        memcpy(&buf->txBuffer[oldTailIdx], str, firstPart);
        uint16_t secondPart = (uint16_t)len - firstPart;
        memcpy(&buf->txBuffer[0], &str[firstPart], secondPart);
        buf->tailTx = (uint8_t)secondPart;
    }

    /* 3) Si no hay transmisión en curso, arrancamos el DMA TX */
    if (!sending) {
        headSentUntil = buf->tailTx;

        // Calcular cuántos bytes lineales hay desde oldTailIdx hasta headSentUntil
        uint16_t chunkLen;
        if (oldTailIdx < headSentUntil) {
            chunkLen = (uint16_t)(headSentUntil - oldTailIdx);
        } else {
            chunkLen = (uint16_t)(USART1_BUFFER_SIZE - oldTailIdx);
        }
        lastTxChunkLen = chunkLen;

        // Arrancar el DMA (canal CIRCULAR) enviando chunkLen bytes
        if (txDmaCallback(&buf->txBuffer[oldTailIdx], chunkLen) != HAL_OK) {
            // Si falla, revertir tailTx
            buf->tailTx = oldTailIdx;
            buf->flags.byte &= ~USART_FLAG_TX_FULL;
            return HAL_ERROR;
        }
        sending = true;
        buf->flags.byte &= ~USART_FLAG_TX_FULL;
    }

    return HAL_OK;
}

/**
 * @brief  Registra el callback que procesará cada byte recibido por DMA.
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
 *         Extrae bytes pendientes de rxBuffer[] y los pasa a RxHandler.
 */
void USART1_Update(void)
{
    if (bufPtr == NULL || rxHandler == NULL) {
        return;
    }
    uint8_t c;
    while (bufPtr->headRx != bufPtr->tailRx) {
        c = bufPtr->rxBuffer[bufPtr->tailRx];
        bufPtr->tailRx = (uint8_t)((bufPtr->tailRx + 1) % USART1_BUFFER_SIZE);
        rxHandler(c);
    }
}

/**
 * @brief  Maneja el evento “DMA TX Transfer Complete” para USART1:
 *   - Avanza los índices en el buffer circular TX hasta headSentUntil.
 *   - Detiene el DMA (desactiva DMAT + HAL_UART_DMAStop).
 *   - Si quedaron bytes pendientes, arranca un nuevo bloque.
 */
void USART1_DMA_TxCpltHandler(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1 && bufPtr != NULL && sending) {
        /* 1) Avanzar tailTx hasta headSentUntil */
        bufPtr->tailTx = headSentUntil;

        /* 2) Detener el DMA Circular porque ese bloque ya se completó */
        sending = false;
        if (uartHandle != NULL) {
            CLEAR_BIT(uartHandle->Instance->CR3, USART_CR3_DMAT);
            HAL_UART_DMAStop(uartHandle);
            uartHandle->gState = HAL_UART_STATE_READY;
            __HAL_UNLOCK(uartHandle);
        }

        /* 3) Si en el ínterin llegaron bytes nuevos (headTx != tailTx), arrancar otro bloque */
        if (bufPtr->headTx != bufPtr->tailTx) {
            headSentUntil = bufPtr->headTx;
            uint16_t chunkLen;
            uint8_t  tailIdx = bufPtr->tailTx;

            if (tailIdx < headSentUntil) {
                chunkLen = (uint16_t)(headSentUntil - tailIdx);
            } else {
                chunkLen = (uint16_t)(USART1_BUFFER_SIZE - tailIdx);
            }
            lastTxChunkLen = chunkLen;

            if (txDmaCallback(&bufPtr->txBuffer[tailIdx], chunkLen) == HAL_OK) {
                sending = true;
            }
            // Si falla, dejamos el contenido en buffer esperando reintento en la próxima llamada a PushTxString
        }
        // Si no quedaron bytes (headTx == tailTx), nos quedamos quietos
    }
}

/**
 * @brief  Callback HAL débil para “Tx Complete”. Si tu proyecto no define otro, la librería lo usa.
 */
__weak void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    USART1_DMA_TxCpltHandler(huart);
}

/**
 * @brief  Callback HAL: se invoca cuando el DMA llena la mitad inferior de rxBuffer (32 bytes).
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
 * @brief  Callback HAL: se invoca cuando el DMA llena todo rxBuffer (64 bytes).
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1 && bufPtr != NULL) {
        bufPtr->headRx = (uint8_t)((bufPtr->headRx + (USART1_BUFFER_SIZE / 2)) % USART1_BUFFER_SIZE);
        if (bufPtr->headRx == bufPtr->tailRx) {
            bufPtr->flags.byte |= USART_FLAG_RX_FULL;
        }
        /* Re-armar recepción Circular inmediatamente */
        if (rxDmaCallback != NULL) {
            rxDmaCallback(bufPtr->rxBuffer, USART1_BUFFER_SIZE);
        }
    }
}


