/* usart_dma_buffer.c - VERSIÓN CORREGIDA */

#include <string.h>
#include <stdbool.h>
#include "types/usart_buffer_type.h"    // Define: Byte_Flag_Struct, USART1_BUFFER_SIZE = 64
#include "usart_dma_buffer.h"
#include "stm32f1xx_hal.h"

/* Definición de flags dentro de 'flags.byte' para estado interno */
#define USART_FLAG_TX_FULL   (1U << 0)
#define USART_FLAG_RX_FULL   (1U << 1)
#define USART_FLAG_TX_BUSY   (1U << 2)

/* Punteros internos a callbacks y al buffer y al handle UART */
static USART1_TxDMA_Callback    txDmaCallback = NULL;
static USART1_RxDMA_Callback    rxDmaCallback = NULL;
static USART1_RxHandler         rxHandler     = NULL;
static USART_Buffer_t          *bufPtr        = NULL;
static UART_HandleTypeDef      *uartHandle    = NULL;
static uint16_t                 lastTxChunkLen;

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
    /* TX circular: headTx = índice de lectura (dequeue), tailTx = índice de escritura (enqueue) */
    bufPtr->headTx  = 0;
    bufPtr->tailTx  = 0;
    bufPtr->txCount = 0;    // no hay bytes pendientes
    /* RX circular: headRx = índice de escritura DMA, tailRx = índice lectura (dequeue en Update) */
    bufPtr->headRx  = 0;
    bufPtr->tailRx  = 0;
    bufPtr->flags.byte = 0;

    /* Limpiar buffer TX para evitar datos basura iniciales */
    memset(bufPtr->txBuffer, 0, USART1_BUFFER_SIZE);

    /* Arrancar RX DMA circular (64 bytes) */
    return rxDmaCallback(bufPtr->rxBuffer, USART1_BUFFER_SIZE);
}

/**
 * @brief  Encola un C-string para transmisión no bloqueante por USART1 + DMA.
 *         CORRECCIÓN: Si txCount == 0, SIEMPRE reinicia headTx=tailTx=0 para evitar residuos.
 *         Luego encola en modo secuencial desde posición 0, evitando fragmentación circular.
 *
 * @param  buf  Puntero al USART_Buffer_t correspondiente a USART1.
 * @param  str  Cadena C-terminada en '\0' que se quiere enviar (sin incluir '\0').
 * @retval HAL_OK    si pudo encolar y arrancar DMA.
 *         HAL_BUSY  si txCount != 0 (transmisión en curso) o no hay espacio suficiente.
 *         HAL_ERROR en error fatal al arrancar el DMA.
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
    if (len > USART1_BUFFER_SIZE) {
        // El mensaje excede 64 bytes → jamás cabrá
        buf->flags.byte |= USART_FLAG_TX_FULL;
        return HAL_BUSY;
    }

    /* CORRECCIÓN PRINCIPAL: Si hay transmisión en curso, rechazar */
    if (buf->txCount != 0) {
        return HAL_BUSY;  // Ya hay datos pendientes de envío
    }

    /* CORRECCIÓN: Buffer vacío - SIEMPRE reiniciar a posición 0 */
    buf->headTx = 0;
    buf->tailTx = 0;

    /* Limpiar el área que vamos a usar para evitar datos residuales */
    memset(buf->txBuffer, 0, (uint16_t)len);

    /* Copiar la cadena directamente desde posición 0 */
    memcpy(buf->txBuffer, str, len);
    buf->tailTx = (uint8_t)len;
    buf->txCount = (uint8_t)len;
    buf->flags.byte |= USART_FLAG_TX_BUSY;

    /* Arrancar transmisión DMA del mensaje completo desde posición 0 */
    lastTxChunkLen = (uint16_t)len;
    if (txDmaCallback(buf->txBuffer, (uint16_t)len) != HAL_OK) {
        buf->flags.byte &= ~USART_FLAG_TX_BUSY;
        buf->txCount = 0;  // Limpiar estado en caso de error
        return HAL_ERROR;
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
 * @brief  Debe llamarse periódicamente (p.ej. en el while principal).
 *         Extrae todos los bytes pendientes del buffer RX y, para cada uno,
 *         invoca al callback registrado con USART1_SetRxHandler().
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

/* ---- Callbacks HAL para encadenar DMA ---- */

/**
 * @brief  Callback HAL: se invoca cuando el DMA finaliza la transmisión completa.
 *         CORRECCIÓN: Simplificado para manejar mensajes completos sin fragmentación.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1 && bufPtr != NULL) {

        /* CORRECCIÓN: Marcar transmisión como completada */
        bufPtr->txCount = 0;
        bufPtr->flags.byte &= ~USART_FLAG_TX_BUSY;

        /* CORRECCIÓN: Resetear índices a 0 para próxima transmisión limpia */
        bufPtr->headTx = 0;
        bufPtr->tailTx = 0;

        /* CORRECCIÓN: Desactivar DMAT para que USART no pida más datos DMA */
        if (uartHandle != NULL) {
            CLEAR_BIT(uartHandle->Instance->CR3, USART_CR3_DMAT);
        }

        /* CORRECCIÓN: Detener el canal DMA (limpia flags internos) */
        if (uartHandle != NULL) {
            HAL_UART_DMAStop(uartHandle);
        }

        /* CORRECCIÓN: Informar a HAL que el periférico quedó listo para nuevo TX */
        if (uartHandle != NULL) {
            uartHandle->gState = HAL_UART_STATE_READY;
            __HAL_UNLOCK(uartHandle);
        }
    }
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
        /* Re-armar recepción circular */
        if (rxDmaCallback != NULL) {
            rxDmaCallback(bufPtr->rxBuffer, USART1_BUFFER_SIZE);
        }
    }
}
