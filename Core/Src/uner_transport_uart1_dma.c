/*
 * uner_transport_uart1_dma.c
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#include "uner_transport_uart1_dma.h"

static void UNER_TransportUart1_Poll(UNER_Transport *transport, UNER_Core *core)
{
    UNER_TransportUart1 *ctx = (UNER_TransportUart1 *)transport->context;
    if (!ctx || !ctx->hdma_rx || !ctx->rx_buf || !core) {
        return;
    }

    uint16_t curr_pos = (uint16_t)(ctx->rx_size - __HAL_DMA_GET_COUNTER(ctx->hdma_rx));
    if (curr_pos == ctx->last_pos) {
        return;
    }

    if (curr_pos > ctx->last_pos) {
        for (uint16_t i = ctx->last_pos; i < curr_pos; ++i) {
            UNER_Core_PushByte(core, transport->id, transport->max_payload, ctx->rx_buf[i]);
        }
    } else {
        for (uint16_t i = ctx->last_pos; i < ctx->rx_size; ++i) {
            UNER_Core_PushByte(core, transport->id, transport->max_payload, ctx->rx_buf[i]);
        }
        for (uint16_t i = 0; i < curr_pos; ++i) {
            UNER_Core_PushByte(core, transport->id, transport->max_payload, ctx->rx_buf[i]);
        }
    }

    ctx->last_pos = curr_pos;
}

static UNER_Status UNER_TransportUart1_TrySend(UNER_Transport *transport, const uint8_t *buf, uint16_t len)
{
    UNER_TransportUart1 *ctx = (UNER_TransportUart1 *)transport->context;
    if (!ctx || !ctx->huart || !buf || len == 0) {
        return UNER_STATUS_ERR_NULL;
    }
    if (ctx->tx_busy) {
        return UNER_STATUS_ERR_BUSY;
    }
    if (HAL_UART_Transmit_DMA(ctx->huart, (uint8_t *)buf, len) != HAL_OK) {
        return UNER_STATUS_ERR_TRANSPORT;
    }
    ctx->tx_busy = true;
    return UNER_STATUS_OK;
}

void UNER_TransportUart1_Init(UNER_Transport *transport, UNER_TransportUart1 *context)
{
    if (!transport || !context) {
        return;
    }
    transport->context = context;
    transport->poll_rx = UNER_TransportUart1_Poll;
    transport->try_send = UNER_TransportUart1_TrySend;
}

void UNER_TransportUart1_OnTxComplete(UNER_TransportUart1 *context)
{
    if (!context) {
        return;
    }
    context->tx_busy = false;
}
