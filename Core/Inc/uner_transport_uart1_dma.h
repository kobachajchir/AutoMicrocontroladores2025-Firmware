/*
 * uner_transport_uart1_dma.h - UNER UART1 DMA transport
 */
#ifndef INC_UNER_TRANSPORT_UART1_DMA_H_
#define INC_UNER_TRANSPORT_UART1_DMA_H_

#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "uner_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    UNER_Transport base;
    UART_HandleTypeDef *huart;
    DMA_HandleTypeDef *hdma_rx;
    uint8_t *rx_buf;
    uint16_t rx_len;
    uint16_t last_pos;
    volatile uint8_t *tx_busy_flag;
} UNER_TransportUart1Dma;

UNER_Status UNER_TransportUart1Dma_Init(
    UNER_TransportUart1Dma *transport,
    uint8_t transport_id,
    UART_HandleTypeDef *huart,
    DMA_HandleTypeDef *hdma_rx,
    uint8_t *rx_buf,
    uint16_t rx_len,
    volatile uint8_t *tx_busy_flag);

void UNER_TransportUart1Dma_OnTxComplete(UNER_TransportUart1Dma *transport);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_TRANSPORT_UART1_DMA_H_ */
