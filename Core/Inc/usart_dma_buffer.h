/*
 * usart_dma_buffer.h
 *
 *  Created on: Jun 4, 2025
 *      Author: kobac
 */

#ifndef INC_USART_DMA_BUFFER_H_
#define INC_USART_DMA_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>
#include "types/usart_buffer_type.h"
#include "stm32f1xx_hal.h"  // para HAL_StatusTypeDef

/* Flags internos para TX_FULL, RX_FULL, TX_BUSY */
#define USART_FLAG_TX_FULL   (1U << 0)
#define USART_FLAG_RX_FULL   (1U << 1)
#define USART_FLAG_TX_BUSY   (1U << 2)

// Prototipos de los callbacks que el usuario debe proporcionar:
typedef HAL_StatusTypeDef (*USART1_TxDMA_Callback)(uint8_t *pData, uint16_t size);
typedef HAL_StatusTypeDef (*USART1_RxDMA_Callback)(uint8_t *pData, uint16_t size);
typedef void (*USART1_RxHandler)(uint8_t byte);

/**
 * @brief  Registra el puntero al UART_HandleTypeDef que usará la librería para TX/RX.
 * @param  huart  Puntero al handle del USART (por ejemplo, &huart1).
 * @retval HAL_OK si se asignó correctamente; HAL_ERROR si huart == NULL.
 */
HAL_StatusTypeDef USART1_RegisterHandle(UART_HandleTypeDef *huart);

/**
 * @brief  Asigna las funciones de arranque DMA TX/RX que la librería utilizará internamente.
 * @param  txCb  Función que arranca el DMA TX: debe llamar internamente a HAL_UART_Transmit_DMA.
 * @param  rxCb  Función que arranca el DMA RX: debe llamar internamente a HAL_UART_Receive_DMA.
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
 *         Solo acepta el encolado si el buffer TX está completamente vacío (txCount == 0).
 * @param  buf  Puntero al USART_Buffer_t correspondiente a USART1.
 * @param  str  Cadena C-terminada en '\0' que se quiere enviar (no incluye '\0').
 * @retval HAL_OK si pudo encolar y arrancar DMA; HAL_BUSY si txCount != 0 o no cabe; HAL_ERROR en error fatal.
 */
HAL_StatusTypeDef USART1_PushTxString(USART_Buffer_t *buf, const char *str);

/**
 * @brief  Registra el callback que procesará cada byte recibido por DMA.
 * @param  handler  Función que recibirá cada byte (p.ej. convertirlo, guardarlo, etc.).
 * @retval HAL_OK si handler != NULL; HAL_ERROR en otro caso.
 */
HAL_StatusTypeDef USART1_SetRxHandler(USART1_RxHandler handler);

/**
 * @brief  Debe llamarse periódicamente (p.ej. en el while principal).
 *         Extrae todos los bytes pendientes en el buffer RX y, para cada uno,
 *         invoca al callback registrado con USART1_SetRxHandler().
 */
void USART1_Update(void);
#endif /* INC_USART_DMA_BUFFER_H_ */
