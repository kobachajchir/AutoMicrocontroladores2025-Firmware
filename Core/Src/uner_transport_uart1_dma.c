/*
 * uner_transport_uart1_dma.c - UNER UART1 DMA transport
 */
#include "uner_transport_uart1_dma.h"

void UNER_Uart1Dma_Init(UNER_Uart1Dma *tr,
                        UART_HandleTypeDef *huart,
                        DMA_HandleTypeDef *hdma_rx,
                        uint8_t *rx_dma_buf,
                        uint16_t rx_dma_sz)
{
    if (!tr) {
        return;
    }

    tr->huart = huart;
    tr->hdma_rx = hdma_rx;
    tr->rx_dma_buf = rx_dma_buf;
    tr->rx_dma_sz = rx_dma_sz;
    tr->last_pos = 0;
    tr->tx_busy = 0;
}

UNER_Status UNER_Uart1Dma_TrySend(UNER_Uart1Dma *tr,
                                  const uint8_t *buf,
                                  uint16_t len)
{
    if (!tr || !tr->huart || !buf || len == 0u) {
        return UNER_ERR_LEN;
    }

    if (tr->tx_busy) {
        return UNER_ERR_BUSY;
    }

    tr->tx_busy = 1;
    if (HAL_UART_Transmit_DMA(tr->huart, (uint8_t *)buf, len) != HAL_OK) {
        tr->tx_busy = 0;
        return UNER_ERR_BUSY;
    }

    return UNER_OK;
}

void UNER_Uart1Dma_PollRx(UNER_Uart1Dma *tr,
                          UNER_Core *core,
                          uint8_t transport_id,
                          uint8_t max_payload)
{
    if (!tr || !core || !tr->hdma_rx || !tr->rx_dma_buf || tr->rx_dma_sz == 0u) {
        return;
    }

    uint16_t curr_pos = (uint16_t)(tr->rx_dma_sz - __HAL_DMA_GET_COUNTER(tr->hdma_rx));
    if (curr_pos == tr->last_pos) {
        return;
    }

    if (curr_pos > tr->last_pos) {
        for (uint16_t i = tr->last_pos; i < curr_pos; ++i) {
            UNER_Core_PushByte(core, transport_id, max_payload, tr->rx_dma_buf[i]);
        }
    } else {
        for (uint16_t i = tr->last_pos; i < tr->rx_dma_sz; ++i) {
            UNER_Core_PushByte(core, transport_id, max_payload, tr->rx_dma_buf[i]);
        }
        for (uint16_t i = 0; i < curr_pos; ++i) {
            UNER_Core_PushByte(core, transport_id, max_payload, tr->rx_dma_buf[i]);
        }
    }

    tr->last_pos = curr_pos;
}

void UNER_Uart1Dma_TxCplt(UNER_Uart1Dma *tr)
{
    if (tr) {
        tr->tx_busy = 0;
    }
}
