/*
 * uner_transport_uart1_dma.h
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#ifndef INC_UNER_TRANSPORT_UART1_DMA_H_
#define INC_UNER_TRANSPORT_UART1_DMA_H_

#include "uner_transport.h"
#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    UART_HandleTypeDef *huart;
    DMA_HandleTypeDef *hdma_rx;
    uint8_t *rx_buf;
    uint16_t rx_size;
    volatile uint16_t last_pos;
    volatile bool tx_busy;
} UNER_TransportUart1;

void UNER_TransportUart1_Init(UNER_Transport *transport, UNER_TransportUart1 *context);
void UNER_TransportUart1_OnTxComplete(UNER_TransportUart1 *context);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_TRANSPORT_UART1_DMA_H_ */
