/*
 * uner_transport_uart1_dma.h - UNER UART1 DMA transport
 */
#ifndef INC_UNER_TRANSPORT_UART1_DMA_H_
#define INC_UNER_TRANSPORT_UART1_DMA_H_

#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "uner_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    UART_HandleTypeDef *huart;
    DMA_HandleTypeDef *hdma_rx;
    uint8_t *rx_dma_buf;
    uint16_t rx_dma_sz;
    uint16_t last_pos;
    volatile uint8_t tx_busy;
} UNER_Uart1Dma;

void UNER_Uart1Dma_Init(UNER_Uart1Dma *tr,
                        UART_HandleTypeDef *huart,
                        DMA_HandleTypeDef *hdma_rx,
                        uint8_t *rx_dma_buf,
                        uint16_t rx_dma_sz);

UNER_Status UNER_Uart1Dma_TrySend(UNER_Uart1Dma *tr,
                                  const uint8_t *buf,
                                  uint16_t len);

void UNER_Uart1Dma_PollRx(UNER_Uart1Dma *tr,
                          UNER_Core *core,
                          uint8_t transport_id,
                          uint8_t max_payload);

void UNER_Uart1Dma_TxCplt(UNER_Uart1Dma *tr);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_TRANSPORT_UART1_DMA_H_ */
