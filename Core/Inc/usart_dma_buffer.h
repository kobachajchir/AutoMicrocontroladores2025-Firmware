/**
 * @file    usart_dma_buffer.h
 * @brief   Librería de transmisión y recepción UART1 usando DMA + buffer circular (64 bytes).
 *
 * Esta librería proporciona una API totalmente no bloqueante para:
 *   - Transmitir cadenas (“C-strings”) vía USART1 usando un buffer circular de 64 bytes y DMA en modo Circular.
 *   - Recibir bytes vía USART1 + DMA en modo Circular, entregándolos mediante un callback en el bucle principal.
 *
 * ## 1. Requisitos previos
 *   - El proyecto debe tener configurado USART1 (PA9=TX, PA10=RX) con HAL_UART_Init(&huart1).
 *   - DMA TX (ej. DMA1_Channel4) **en modo CIRCULAR**:
 *       • DIR: Memory → Peripheral
 *       • PeriphInc: DISABLE
 *       • MemInc: ENABLE
 *       • Align: BYTE–BYTE
 *       • CircularMode: ENABLE
 *   - DMA RX (ej. DMA1_Channel5) **en modo CIRCULAR**:
 *       • DIR: Peripheral → Memory
 *       • PeriphInc: DISABLE
 *       • MemInc: ENABLE
 *       • Align: BYTE–BYTE
 *       • CircularMode: ENABLE
 *   - NVIC habilitado para interrupciones de DMA1_Channel4 (TX) y DMA1_Channel5 (RX).
 *
 * ## 2. Paso a paso para integrar
 *   1. Copiar estos dos archivos (`usart_dma_buffer.h` y `usart_dma_buffer.c`) en tu carpeta de librerías.
 *   2. Incluir en tu proyecto el archivo fuente `usart_dma_buffer.c` para compilarlo como parte de la librería estática.
 *   3. En tu proyecto, tras inicializar UART1 y DMA, hacer:
 *
 *      ```c
 *      #include "usart_dma_buffer.h"
 *
 *      // Buffer global (debe ser visible para la librería):
 *      USART_Buffer_t usart1Buf;
 *
 *      // Wrappers para arrancar DMA TX/RX:
 *      static HAL_StatusTypeDef HAL_UART1_TxDMA_Wrapper(uint8_t *pData, uint16_t size)
 *      {
 *          return HAL_UART_Transmit_DMA(&huart1, pData, size);
 *      }
 *      static HAL_StatusTypeDef HAL_UART1_RxDMA_Wrapper(uint8_t *pData, uint16_t size)
 *      {
 *          return HAL_UART_Receive_DMA(&huart1, pData, size);
 *      }
 *
 *      // Callback que procesa cada byte recibido:
 *      static void MyRxHandler(uint8_t byte)
 *      {
 *          // Procesar “byte” como desees (p.ej. guardarlo en un ring, filtrarlo, etc.)
 *      }
 *
 *      int main(void)
 *      {
 *          HAL_Init();
 *          SystemClock_Config();
 *          MX_GPIO_Init();
 *          MX_DMA_Init();         // Asegúrate que DMA1_Ch4 (TX) y DMA1_Ch5 (RX) estén en modo CIRCULAR
 *          MX_USART1_UART_Init(); // USART1 (PA9, PA10) 8N1, baud deseado, etc.
 *
 *          // 1) Registrar handle UART1
 *          if (USART1_RegisterHandle(&huart1) != HAL_OK) Error_Handler();
 *
 *          // 2) Asignar callbacks DMA TX/RX
 *          if (USART1_SetDMACallbacks(HAL_UART1_TxDMA_Wrapper,
 *                                     HAL_UART1_RxDMA_Wrapper) != HAL_OK) Error_Handler();
 *
 *          // 3) Inicializar buffer (arranca RX circular)
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
 *              // B) Cualquier otro momento, encolar cadenas TX:
 *              //    if (USART1_PushTxString(&usart1Buf, "Otra línea...\r\n") == HAL_BUSY)
 *              //    {
 *              //        // no hubo espacio, descartar o reintentar más tarde
 *              //    }
 *
 *              // ... resto de la lógica de la aplicación ...
 *          }
 *      }
 *      ```
 *
 *   4. **Importante**: El proyecto final **debe** notificar a la librería los eventos de Tx Complete del DMA.
 *      - Si usas CubeMX y la opción “Initialize UART with DMA” te genera un `HAL_UART_TxCpltCallback` en `stm32f1xx_it.c` o similar,
 *        debes **insertar** dentro de ese callback una llamada a `USART1_DMA_TxCpltHandler(huart1_ptr)`.
 *      - Si no implementas ese paso, la librería nunca sabrá que finalizó el bloque y “seguirá intentando enviar basura”.
 *
 * ## 3. API pública resumida
 *   - `HAL_StatusTypeDef USART1_RegisterHandle(UART_HandleTypeDef *huart);`
 *   - `HAL_StatusTypeDef USART1_SetDMACallbacks(USART1_TxDMA_Callback txCb, USART1_RxDMA_Callback rxCb);`
 *   - `HAL_StatusTypeDef USART1_DMA_Init(USART_Buffer_t *buf);`
 *   - `HAL_StatusTypeDef USART1_PushTxString(USART_Buffer_t *buf, const char *str);`
 *   - `HAL_StatusTypeDef USART1_SetRxHandler(USART1_RxHandler handler);`
 *   - `void USART1_Update(void);`
 *   - `void USART1_DMA_TxCpltHandler(UART_HandleTypeDef *huart);`  ← Debe llamarse desde el `HAL_UART_TxCpltCallback` del proyecto.
 *
 * ## 4. Modo de operación Tx
 *   1. `PushTxString` encola hasta 64 bytes en el buffer circular (si cabe).
 *   2. Si no hay transmisión en curso (`sending == false`), toma un “snapshot” del índice
 *      `headSentUntil = headTx` y lanza `HAL_UART_Transmit_DMA(&huart1, &txBuffer[tailTx_old], chunkLen)`.
 *   3. DMA envía `chunkLen` bytes. Cuando la transferencia se completa, `HAL_UART_TxCpltCallback` dispara:
 *       - Llama a `USART1_DMA_TxCpltHandler(&huart1)` que hace:
 *         • `tailTx = headSentUntil; sending = false; HAL_UART_DMAStop(&huart1)`
 *         • Si `headTx != tailTx`, captura nuevo `headSentUntil = headTx`, calcula `chunkLen` y reinicia DMA.
 *         • Si `headTx == tailTx`, no hay más datos y la librería “se queda quieta” hasta la próxima llamada a `PushTxString`.
 *
 *   4. Si entre tanto (mientras DMA transmitía) se llama a `PushTxString` y se encola más data,
 *      cuando finalice el bloque actual, `DMA_TxCpltHandler` se percatara de esa data nueva y lanzará el siguiente tramo.
 *
 * ## 5. Modo de operación Rx
 *   1. `USART1_DMA_Init` arranca automáticamente `HAL_UART_Receive_DMA(&huart1, rxBuffer, 64)` en Circular.
 *   2. Cada 32 bytes que reciba, se dispara `HAL_UART_RxHalfCpltCallback`:
 *       - En ese callback interno, la librería hace `headRx += 32; if (headRx == tailRx) mark overflow`.
 *   3. Cuando llega a 64 bytes, se dispara `HAL_UART_RxCpltCallback`:
 *       - Librería hace `headRx += 32; if (headRx == tailRx) mark overflow; rearmar DMA`.
 *   4. En tu bucle principal, llamas a `USART1_Update()`, que saca cada byte entre `tailRx` y `headRx` y los pasa a tu `RxHandler`.
 *
 * ## 6. Notas finales
 *   - Si deseas cambiar a otro UART (p. ej. USART2), basta con replicar los wrappers y adaptar nombres.
 *   - La librería **no se bloquea jamás**: `PushTxString` devuelve inmediatamente con `HAL_BUSY` si no hay espacio, o `HAL_OK` si encoló.
 *   - Al terminar cada bloque de transmisión, el DMA se detiene. Si quedan datos pendientes en el buffer, la librería vuelve a arrancarlo.
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

// Redefinimos flags internos:
#define USART_FLAG_TX_BUSY   (1U << 0)  // bit0
#define USART_FLAG_TX_FULL   (1U << 1)  // bit1
#define USART_FLAG_RX_FULL   (1U << 2)  // bit2

/*
 * Callbacks que el usuario debe proporcionar:
 *  - Tx: HAL_UART_Transmit_DMA(&huart1, pData, size);
 *  - Rx: HAL_UART_Receive_DMA(&huart1, pData, size);
 */
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
 * @brief  Encola un C-string para transmisión no bloqueante por USART1 + DMA (TX en Circular).
 * @param  buf  Puntero al USART_Buffer_t correspondiente a USART1.
 * @param  str  Cadena C-terminada en '\0' que se quiere enviar (sin incluir '\0').
 * @retval HAL_OK   si se encoló (y arrancó DMA si hacía falta).
 *         HAL_BUSY si no hay suficiente espacio libre (TX_FULL).
 *         HAL_ERROR en caso de error fatal (p.ej. txDmaCallback == NULL).
 */
HAL_StatusTypeDef USART1_PushTxString(USART_Buffer_t *buf, const char *str);

/**
 * @brief  Registra el callback que procesará cada byte recibido por DMA.
 * @param  handler  Función que recibirá cada byte (p.ej. convertirlo, guardarlo, etc.).
 * @retval HAL_OK si handler != NULL; HAL_ERROR en otro caso.
 */
HAL_StatusTypeDef USART1_SetRxHandler(USART1_RxHandler handler);

/**
 * @brief  Debe llamarse periódicamente (p.ej. en el loop principal).
 *         Extrae todos los bytes pendientes en el buffer RX y, para cada uno,
 *         invoca al callback registrado con USART1_SetRxHandler().
 */
void USART1_Update(void);

/**
 * @brief  Debe llamarse desde el callback `HAL_UART_TxCpltCallback` del proyecto.
 *         Maneja internamente la lógica de “avanzar índices, detener DMA y relanzar
 *         otro bloque si hay datos nuevos”. Sin esta llamada, la librería se quedará
 *         enviando datos indefinidamente (o no sabrá cuándo detener el DMA).
 *
 * @param huart  Puntero al UART_HandleTypeDef que causó el callback (p.ej. &huart1).
 */
void USART1_DMA_TxCpltHandler(UART_HandleTypeDef *huart);

/**
 * @brief  Se debe llamar desde HAL_UART_TxHalfCpltCallback( &huart1 )
 *         ─ Si al enviar 32 bytes ya completamos el mensaje (≤32), lo detenemos.
 *         ─ Si todavía hay más de 32 bytes, no detenemos; dejamos que siga.
 */
void USART1_DMA_TxHalfCpltHandler(UART_HandleTypeDef *huart);

#endif /* INC_USART_DMA_BUFFER_H_ */


