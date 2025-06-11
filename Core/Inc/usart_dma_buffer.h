/**
 * @file    usart_dma_buffer.h
 * @brief   Librería de transmisión y recepción UART1 usando DMA + buffer circular (64 bytes).
 *
 * Esta librería proporciona una API totalmente no bloqueante para:
 *   - Transmitir cadenas (“C-strings”) vía USART1 usando un buffer circular de 64 bytes y DMA en modo “normal”.
 *   - Recibir bytes vía USART1 + DMA en modo Circular (64 B), entregándolos mediante un callback en el bucle principal.
 *
 * ## 1. Requisitos previos
 *   - El proyecto debe tener configurado USART1 (PA9=TX, PA10=RX) con HAL_UART_Init(&huart1).
 *   - DMA TX (ej. DMA1_Channel4) **en modo NORMAL** (no circular):
 *       • DIR: Memory → Peripheral
 *       • PeriphInc: DISABLE
 *       • MemInc: ENABLE
 *       • Align: BYTE–BYTE
 *       • CircularMode: DISABLE
 *   - DMA RX (ej. DMA1_Channel5) **en modo CIRCULAR**:
 *       • DIR: Peripheral → Memory
 *       • PeriphInc: DISABLE
 *       • MemInc: ENABLE
 *       • Align: BYTE–BYTE
 *       • CircularMode: ENABLE
 *   - NVIC habilitado para interrupciones de DMA1_Channel4 (TX) y DMA1_Channel5 (RX).
 *
 * ## 2. Integración paso a paso
 *   1. Copiar estos dos archivos (`usart_dma_buffer.h` y `usart_dma_buffer.c`) en tu carpeta de librerías.
 *   2. Incluir en tu proyecto el archivo fuente `usart_dma_buffer.c` para compilarlo como parte de la librería estática.
 *   3. En tu proyecto, tras inicializar UART1 y DMA, hacer:
 *
 *      ```c
 *      #include "usart_dma_buffer.h"
 *
 *      // 1) Declarar un buffer global de 64 bytes:
 *      USART_Buffer_t usart1Buf;
 *
 *      // 2) Wrappers para arrancar DMA TX/RX:
 *      static HAL_StatusTypeDef HAL_UART1_TxDMA_Wrapper(uint8_t *pData, uint16_t size)
 *      {
 *          // Llama a HAL_UART_Transmit_DMA con la longitud “size” exacta
 *          return HAL_UART_Transmit_DMA(&huart1, pData, size);
 *      }
 *      static HAL_StatusTypeDef HAL_UART1_RxDMA_Wrapper(uint8_t *pData, uint16_t size)
 *      {
 *          // Llama a HAL_UART_Receive_DMA en modo CIRCULAR 64 bytes
 *          return HAL_UART_Receive_DMA(&huart1, pData, size);
 *      }
 *
 *      // 3) Callback que procesa cada byte recibido:
 *      static void MyRxHandler(uint8_t byte)
 *      {
 *          // Aquí recibes cada "byte" que llegó por UART1.
 *      }
 *
 *      int main(void)
 *      {
 *          HAL_Init();
 *          SystemClock_Config();
 *          MX_GPIO_Init();
 *          MX_DMA_Init();         // Asegúrate que DMA1_Ch4 (TX) está en Modo NORMAL, DMA1_Ch5 (RX) en Modo CIRCULAR
 *          MX_USART1_UART_Init(); // USART1 8N1, BAUD adecuado, etc.
 *
 *          // 1) Registrar handle UART1
 *          if (USART1_RegisterHandle(&huart1) != HAL_OK) Error_Handler();
 *
 *          // 2) Asignar callbacks DMA TX/RX
 *          if (USART1_SetDMACallbacks(HAL_UART1_TxDMA_Wrapper,
 *                                     HAL_UART1_RxDMA_Wrapper) != HAL_OK) Error_Handler();
 *
 *          // 3) Inicializar buffer (arranca RX en circular 64 bytes)
 *          if (USART1_DMA_Init(&usart1Buf) != HAL_OK) Error_Handler();
 *
 *          // 4) Registrar handler RX
 *          if (USART1_SetRxHandler(MyRxHandler) != HAL_OK) Error_Handler();
 *
 *          // 5) Enviar un saludo inicial (no bloqueante)
 *          USART1_PushTxString(&usart1Buf, "Bienvenido. UART1 + DMA listo.\r\n");
 *
 *          while (1)
 *          {
 *              // A) Procesar bytes RX pendientes
 *              USART1_Update();
 *
 *              // B) Encolar TX en cualquier momento:
 *              //    if (USART1_PushTxString(&usart1Buf, "Otra línea...\r\n") == HAL_BUSY)
 *              //    {
 *              //        // No hubo espacio (TX_FULL): descartar o reintentar más tarde
 *              //    }
 *          }
 *      }
 *      ```
 *
 *   4. **IMPORTANTE**: En tu archivo `stm32f1xx_it.c` (o donde definas los IRQHandlers),
 *      debes **insertar** dentro de `HAL_UART_TxCpltCallback` la llamada a `USART1_DMA_TxCpltHandler(&huart1)`. Por ejemplo:
 *
 *      ```c
 *      void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
 *      {
 *          USART1_DMA_TxCpltHandler(huart);
 *      }
 *      ```
 *
 *      Sin esta llamada, la librería no sabrá cuándo terminaron de enviarse los bytes y jamás liberará el flag “TX_BUSY”.
 *
 * ## 3. API pública
 *   - `HAL_StatusTypeDef USART1_RegisterHandle(UART_HandleTypeDef *huart);`
 *   - `HAL_StatusTypeDef USART1_SetDMACallbacks(USART1_TxDMA_Callback txCb, USART1_RxDMA_Callback rxCb);`
 *   - `HAL_StatusTypeDef USART1_DMA_Init(USART_Buffer_t *buf);`
 *   - `HAL_StatusTypeDef USART1_PushTxString(USART_Buffer_t *buf, const char *str);`
 *   - `HAL_StatusTypeDef USART1_SetRxHandler(USART1_RxHandler handler);`
 *   - `void               USART1_Update(void);`
 *   - `void               USART1_DMA_TxCpltHandler(UART_HandleTypeDef *huart);`
 *
 * ## 4. Cómo funciona internamente (TX en Modo Normal)
 *   1. `PushTxString` encola la cadena completa (long ≤ 64 bytes) en el buffer circular, sin importar si `TX_BUSY == 1`.
 *   2. Si `TX_BUSY == 0`, toma nota de cuántos bytes están encolados (hasta el wrap) y arranca **una sola** llamada a `HAL_UART_Transmit_DMA(&huart1, &txBuffer[tailTx], length)` donde “length” es el tramo contiguo inicial (desde tailTx hasta headTx, o hasta el final del buffer). Pone `TX_BUSY = 1`.
 *   3. Cuando el DMA termine esa primera transferencia, el HAL llamará a `HAL_UART_TxCpltCallback` → tu `USART1_DMA_TxCpltHandler`. Allí:
 *       - Avanza `tailTx += lastTxLen` (la misma cantidad de bytes que se transmitieron).
 *       - Comprueba si aún queda data (es decir, `headTx != tailTx`).
 *         • Si hay más, calcula la longitud siguiente (hasta el wrap, o desde tailTx hasta headTx) y vuelve a llamar a `HAL_UART_Transmit_DMA` con ese nuevo “length”.
 *         • Si no hay nada (`headTx == tailTx`), detiene el DMA y pone `TX_BUSY = 0`.
 *
 *   4. De ese modo, **cada bloque que envía el DMA es exactamente la longitud real solicitada**. No hay asunciones de “64 siempre” ni “división en mitades de 32”.
 *
 * ## 5. Cómo funciona internamente (RX en Circular de 64 B)
 *   1. Al llamar `USART1_DMA_Init`, se lanza `HAL_UART_Receive_DMA(&huart1, rxBuffer, 64)` con `CircularMode = ENABLE`.
 *   2. Cada 32 bytes recibidos, el HAL dispara `HAL_UART_RxHalfCpltCallback(&huart1)`:
 *       - La librería avanza `headRx += 32`. Si `headRx == tailRx`, marca `RX_FULL`.
 *   3. Al completar los 64 bytes, el HAL dispara `HAL_UART_RxCpltCallback(&huart1)`:
 *       - La librería avanza `headRx += 32`. Si `headRx == tailRx`, marca `RX_FULL`.
 *       - Vuelve a llamar a `HAL_UART_Receive_DMA(&huart1, rxBuffer, 64)` para rearmar el circular.
 *   4. Mientras tanto, en tu bucle principal llamas a `USART1_Update()`, que extrae cada byte entre `tailRx` y `headRx` y llama a tu `RxHandler(byte)`.
 *
 * ## 6. Notas finales
 *   - Con esta estrategia, **nunca asumes que el usuario envía 64 bytes**: cada vez que inicias el DMA pides exactamente los bytes pendientes en la cola TX.
 *   - Ni siquiera necesitas el “Half Complete” para TX; basta con el “Full Complete” (TC) para avanzar. El HT para TX se omite.
 *   - El buffer circular TX jamás se “tragará” datos: si no cabe un string completo, `PushTxString` devolverá `HAL_BUSY` y no encolará nada.
 *   - Cuando el DMA termine de transmitir, `USART1_DMA_TxCpltHandler` se encargará de avanzar punteros y volver a arrancar solo la siguiente porción restante.
 *
 * @author  kobac
 * @date    Jun 4, 2025
 */

#ifndef INC_USART_DMA_BUFFER_H_
#define INC_USART_DMA_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>
#include "types/usart_buffer_type.h"
#include "stm32f1xx_hal.h"

// Flags internos (coinciden con Byte_Flag_Struct.bitmap)
#define USART_FLAG_TX_BUSY   (1U << 0)  // bit0
#define USART_FLAG_TX_FULL   (1U << 1)  // bit1
#define USART_FLAG_RX_FULL   (1U << 2)  // bit2

// Prototipos de los callbacks que el usuario debe proporcionar:
typedef HAL_StatusTypeDef (*USART1_TxDMA_Callback)(uint8_t *pData, uint16_t size);
typedef HAL_StatusTypeDef (*USART1_RxDMA_Callback)(uint8_t *pData, uint16_t size);
typedef void (*USART1_RxHandler)(uint8_t byte);

/**
 * @brief  Registra el puntero al UART_HandleTypeDef que usará la librería para TX/RX.
 * @param  huart  Puntero al handle del USART (p.ej. &huart1).
 * @retval HAL_OK si se asignó correctamente; HAL_ERROR si huart == NULL.
 */
HAL_StatusTypeDef USART1_RegisterHandle(UART_HandleTypeDef *huart);

/**
 * @brief  Asigna las funciones de arranque DMA TX/RX que la librería utilizará internamente.
 * @param  txCb  Función que arranca el DMA TX: HAL_UART_Transmit_DMA.
 * @param  rxCb  Función que arranca el DMA RX: HAL_UART_Receive_DMA.
 * @retval HAL_OK si ambas no son NULL; HAL_ERROR en otro caso.
 */
HAL_StatusTypeDef USART1_SetDMACallbacks(USART1_TxDMA_Callback txCb,
                                         USART1_RxDMA_Callback rxCb);

/**
 * @brief  Inicializa índices y flags, y arranca recepción DMA circular para RX.
 * @param  buf   Puntero al USART_Buffer_t que usará USART1.
 * @retval HAL_OK si buf != NULL y rxDmaCallback está asignado; HAL_ERROR en otro caso.
 */
HAL_StatusTypeDef USART1_DMA_Init(USART_Buffer_t *buf);

/**
 * @brief  Encola un C-string para transmisión no bloqueante por USART1 + DMA.
 *         Solo acepta el encolado si el buffer TX tiene al menos len bytes libres.
 *         Si TX_BUSY==0, arranca un HAL_UART_Transmit_DMA exactly con la longitud necesaria.
 * @param  buf  Puntero al USART_Buffer_t correspondiente a USART1.
 * @param  str  Cadena C-terminada en '\0' que se quiere enviar (sin incluir '\0').
 * @retval HAL_OK   si encoló (y arrancó DMA si hacía falta);
 *         HAL_BUSY si no hay suficiente espacio libre (TX_FULL);
 *         HAL_ERROR en caso de error fatal (p.ej. txDmaCallback == NULL).
 */
HAL_StatusTypeDef USART1_PushTxString(USART_Buffer_t *buf, const char *str);

/**
 * @brief  Registra el callback que procesará cada byte recibido.
 * @param  handler  Función que recibirá cada byte (p.ej. convertirlo, guardarlo, etc.).
 * @retval HAL_OK si handler != NULL; HAL_ERROR en otro caso.
 */
HAL_StatusTypeDef USART1_SetRxHandler(USART1_RxHandler handler);

/**
 * @brief  Debe llamarse periódicamente (p.ej. en el loop principal).
 *         Extrae todos los bytes pendientes de recepción (entre tailRx y headRx)
 *         e invoca el callback registrado con USART1_SetRxHandler().
 */
void USART1_Update(void);

/**
 * @brief  Debe llamarse desde el callback `HAL_UART_TxCpltCallback` del proyecto.
 *         Maneja internamente la lógica de “avanzar índices, detener DMA y relanzar
 *         otro bloque si hay datos nuevos”.
 * @param  huart  Puntero al UART_HandleTypeDef que causó el callback (p.ej. &huart1).
 */
void USART1_DMA_TxCpltHandler(UART_HandleTypeDef *huart);

void USART1_DMA_TxDMACpltHandler(DMA_HandleTypeDef *hdma);

/**
 * @brief  Encola un array de valores de 16 bits sin signo para transmitirlos vía USART1+DMA.
 *         Formatea cada valor en base decimal sin signo, separado por comas y un espacio.
 *
 * @param  buf      Puntero al USART_Buffer_t (txBuffer[64]).
 * @param  values   Puntero al array de uint16_t con los valores a imprimir.
 * @param  length   Número de elementos en el array `values` (≤64 en total).
 * @retval HAL_OK    Si pudo formatear y encolar todos los valores.
 * @retval HAL_BUSY  Si TX_BUSY==1 o la cadena resultante excede 64 bytes.
 * @retval HAL_ERROR En caso de error fatal (p.ej. buf==NULL).
 */
HAL_StatusTypeDef USART1_PushTxU16Values(USART_Buffer_t *buf,
                                         const uint16_t *values,
                                         uint16_t length);



#endif /* INC_USART_DMA_BUFFER_H_ */


