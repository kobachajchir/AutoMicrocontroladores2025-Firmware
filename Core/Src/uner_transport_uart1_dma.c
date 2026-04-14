/*
 * uner_transport_uart1_dma.c - UNER UART1 DMA transport
 */
#include "uner_transport_uart1_dma.h"

static uint8_t UNER_TransportUart1Dma_QueueFull(const UNER_Core *core)
{
    return (core && core->q_count >= core->slot_count) ? 1u : 0u;
}

static UNER_Status UNER_TransportUart1Dma_FeedRange(
    UNER_Transport *transport,
    UNER_Core *core,
    uint16_t start_pos,
    uint16_t end_pos)
{
    UNER_TransportUart1Dma *uart = (UNER_TransportUart1Dma *)transport->ctx;

    for (uint16_t i = start_pos; i < end_pos; ++i) {
        if (UNER_TransportUart1Dma_QueueFull(core)) {
            uart->last_pos = i;
            return UNER_ERR_BUSY;
        }

        (void)UNER_Core_PushByte(core, transport->id, transport->max_payload, uart->rx_buf[i]);
        uart->last_pos = (uint16_t)(i + 1u);

        if (UNER_TransportUart1Dma_QueueFull(core)) {
            return UNER_ERR_BUSY;
        }
    }

    return UNER_OK;
}

static UNER_Status UNER_TransportUart1Dma_PollRx(UNER_Transport *transport, UNER_Core *core)
{
    UNER_TransportUart1Dma *uart = (UNER_TransportUart1Dma *)transport->ctx;
    if (!uart || !core || !uart->hdma_rx || !uart->rx_buf) {
        return UNER_ERR_LEN;
    }

    /* Snapshot único del DMA counter */
    uint16_t curr_pos = (uint16_t)(uart->rx_len - __HAL_DMA_GET_COUNTER(uart->hdma_rx));

    if (curr_pos > uart->rx_len) {
        uart->last_pos = 0u;
        return UNER_OK;
    }

    if (curr_pos == uart->last_pos) {
        return UNER_OK;
    }

    if (UNER_TransportUart1Dma_QueueFull(core)) {
        return UNER_ERR_BUSY;
    }

    if (curr_pos > uart->last_pos) {
        return UNER_TransportUart1Dma_FeedRange(transport, core, uart->last_pos, curr_pos);
    } else {
        UNER_Status status = UNER_TransportUart1Dma_FeedRange(
            transport, core, uart->last_pos, uart->rx_len);
        if (status != UNER_OK) {
            return status;
        }

        if (curr_pos == 0u) {
            uart->last_pos = 0u;
            return UNER_OK;
        }

        return UNER_TransportUart1Dma_FeedRange(transport, core, 0u, curr_pos);
    }
}

static UNER_Status UNER_TransportUart1Dma_TrySend(UNER_Transport *transport, const uint8_t *buf, uint16_t len)
{
    UNER_TransportUart1Dma *uart = (UNER_TransportUart1Dma *)transport->ctx;
    if (!uart || !uart->huart || !uart->tx_busy_flag) {
        return UNER_ERR_LEN;
    }

    if (*uart->tx_busy_flag) {
        return UNER_ERR_BUSY;
    }

    *uart->tx_busy_flag = 1u;
    if (HAL_UART_Transmit_DMA(uart->huart, (uint8_t *)buf, len) != HAL_OK) {
        *uart->tx_busy_flag = 0u;
        return UNER_ERR_BUSY;
    }

    return UNER_OK;
}

UNER_Status UNER_TransportUart1Dma_Init(
    UNER_TransportUart1Dma *transport,
    uint8_t transport_id,
    UART_HandleTypeDef *huart,
    DMA_HandleTypeDef *hdma_rx,
    volatile uint8_t *rx_buf,
    volatile uint16_t rx_len,
    volatile uint8_t *tx_busy_flag)
{
    if (!transport || !huart || !hdma_rx || !rx_buf || rx_len == 0u || !tx_busy_flag) {
        return UNER_ERR_LEN;
    }

    transport->base.id = transport_id;
    transport->base.max_payload = 255u;
    transport->base.poll_rx = UNER_TransportUart1Dma_PollRx;
    transport->base.try_send = UNER_TransportUart1Dma_TrySend;
    transport->base.ctx = transport;

    transport->huart = huart;
    transport->hdma_rx = hdma_rx;
    transport->rx_buf = rx_buf;
    transport->rx_len = rx_len;
    transport->last_pos = 0;
    transport->curr_pos = 0;
    transport->tx_busy_flag = tx_busy_flag;

    return UNER_OK;
}

void UNER_TransportUart1Dma_OnTxComplete(UNER_TransportUart1Dma *transport)
{
    if (!transport || !transport->tx_busy_flag) {
        return;
    }
    *transport->tx_busy_flag = 0u;
}
