#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include "globals.h"
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
 * @brief   Transmite una cadena formateada (printf‐style) de forma bloqueante por USART1.
 * @param   fmt     Cadena de formato (igual que printf).
 * @param   ...     Argumentos variables según fmt.
 * @retval  HAL_OK       Si todo salió bien.
 * @retval  HAL_ERROR    Si hubo error al formatear o transmitIR.
 * @retval  HAL_BUSY     Si UART1 estaba ocupado.
 * @retval  HAL_TIMEOUT  Si se agotó el timeout (usamos HAL_MAX_DELAY).
 *
 * @note    Usa un buffer interno de 128 bytes. Si el formato genera más de 127 caracteres,
 *          se truncará.
 */
HAL_StatusTypeDef USART1_PrintfBlocking(const char *fmt, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len < 0) {
        return HAL_ERROR;              // fallo interno de vsnprintf
    }
    if (len > (int)(sizeof(buffer)-1)) {
        len = sizeof(buffer)-1;        // truncar
    }
    // Bloqueante: transmite y espera a que termine incluyendo el stop bit
    return HAL_UART_Transmit(uartHandle,
                             (uint8_t*)buffer,
                             (uint16_t)len,
                             HAL_MAX_DELAY);
}

/**
 * @brief   Transmite un valor entero en un formato numérico específico.
 * @param   value   Valor a imprimir (32 bits sin signo).
 * @param   base    Base para la representación:
 *                   - 2  → binario
 *                   - 10 → decimal
 *                   - 16 → hexadecimal (letras mayúsculas)
 * @retval  HAL_OK       Si la impresión y transmisión fueron exitosas.
 * @retval  HAL_ERROR    Si base no es 2/10/16 o falla al formatear.
 * @retval  HAL_BUSY     Si UART1 está ocupado.
 * @retval  HAL_TIMEOUT  Si se agotó el timeout.
 *
 * @note    El output siempre termina con "\r\n".
 */

HAL_StatusTypeDef USART1_PrintValue(uint32_t value, uint8_t base)
{
    char buffer[40];
    int pos = 0;

    if (base == 2) {
        // Convertir a binario (sin ceros líderes)
        bool started = false;
        for (int b = 31; b >= 0; b--) {
            bool bit = (value >> b) & 1;
            if (bit) started = true;
            if (started) {
                buffer[pos++] = bit ? '1' : '0';
                if (pos >= (int)sizeof(buffer)-3) break;
            }
        }
        if (!started) {
            buffer[pos++] = '0';
        }
    }
    else if (base == 10) {
        pos = snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)value);
    }
    else if (base == 16) {
        pos = snprintf(buffer, sizeof(buffer), "%lX", (unsigned long)value);
    }
    else {
        return HAL_ERROR;  // base inválida
    }

    if (pos < 0 || pos >= (int)sizeof(buffer)-2) {
        return HAL_ERROR;  // formateo falló o buffer insuficiente
    }

    // añadir CRLF
    buffer[pos++] = '\r';
    buffer[pos++] = '\n';

    // bloquear hasta enviar todo
    return HAL_UART_Transmit(uartHandle,
                             (uint8_t*)buffer,
                             (uint16_t)pos,
                             HAL_MAX_DELAY);
}

/**
 * @brief  Transmite de forma bloqueante una cadena por el USART1 previamente registrado.
 * @param  str  Cadena C-terminada en '\0' que se desea enviar.
 * @retval HAL_OK       Si la transmisión finalizó correctamente.
 * @retval HAL_ERROR    Si hubo error (por ejemplo, no se registró el handle).
 * @retval HAL_BUSY     Si el periférico UART está ocupado.
 * @retval HAL_TIMEOUT  Si se superó el tiempo máximo de espera.
 *
 * @note   Usa HAL_MAX_DELAY como timeout, por lo que bloqueará hasta terminar.
 */
HAL_StatusTypeDef USART1_PrintBlocking(const char *str)
{
    if (str == NULL || uartHandle == NULL) {
        return HAL_ERROR;
    }
    uint16_t len = (uint16_t)strlen(str);
    return HAL_UART_Transmit(uartHandle,
                             (uint8_t*)str,
                             len,
                             HAL_MAX_DELAY);
}

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

HAL_StatusTypeDef USART1_PushTxU16Values(USART_Buffer_t *buf,
                                         const uint16_t *values,
                                         uint16_t length)
{
    if (buf == NULL || txDmaCallback == NULL || values == NULL) {
        return HAL_ERROR;
    }
    if (length == 0) {
        return HAL_OK;
    }
    // Construimos la cadena entera aquí:
    // Máximo 64 bytes de datos + 2 bytes de "\r\n" + '\0'
    char tmp[USART1_BUFFER_SIZE+3];
    int  pos = 0;

    for (uint16_t i = 0; i < length; i++)
    {
        // 1) Formatear values[i]
        int n = snprintf(tmp + pos,
                         sizeof(tmp) - pos,
                         "%u",
                         (unsigned)values[i]);
        if (n < 0 || n >= (int)(sizeof(tmp) - pos)) {
            return HAL_BUSY;
        }
        pos += n;

        // 2) Añadir ", " si no es el último
        if (i + 1 < length) {
            if (pos + 2 >= (int)sizeof(tmp)) {
                return HAL_BUSY;
            }
            tmp[pos++] = ',';
            tmp[pos++] = ' ';
        }
    }

    // 3) Agregar salto de línea "\r\n"
    if (pos + 2 >= (int)sizeof(tmp)) {
        return HAL_BUSY;
    }
    tmp[pos++] = '\r';
    tmp[pos++] = '\n';

    // 4) Cerrar string
    tmp[pos] = '\0';

    // 5) Encolar todo de una vez
    return USART1_PushTxString(buf, tmp);
}

