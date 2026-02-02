/*
 * uner_transport_nrf24_spi2.c
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#include "uner_transport_nrf24_spi2.h"

static void UNER_TransportNrf24_Poll(UNER_Transport *transport, UNER_Core *core)
{
    UNER_TransportNrf24 *ctx = (UNER_TransportNrf24 *)transport->context;
    if (!ctx || !core || !ctx->rx_ready || !ctx->read_payload) {
        return;
    }
    if (!ctx->rx_ready(ctx->user_ctx)) {
        return;
    }
    uint8_t buf[32];
    uint8_t len = 0;
    if (!ctx->read_payload(ctx->user_ctx, buf, &len)) {
        return;
    }
    if (len == 0) {
        return;
    }
    for (uint8_t i = 0; i < len; ++i) {
        UNER_Core_PushByte(core, transport->id, transport->max_payload, buf[i]);
    }
}

static UNER_Status UNER_TransportNrf24_TrySend(UNER_Transport *transport, const uint8_t *buf, uint16_t len)
{
    UNER_TransportNrf24 *ctx = (UNER_TransportNrf24 *)transport->context;
    if (!ctx || !ctx->send_payload || !buf || len == 0) {
        return UNER_STATUS_ERR_NULL;
    }
    if (len > 32U) {
        return UNER_STATUS_ERR_LEN;
    }
    if (!ctx->send_payload(ctx->user_ctx, buf, (uint8_t)len)) {
        return UNER_STATUS_ERR_BUSY;
    }
    return UNER_STATUS_OK;
}

void UNER_TransportNrf24_Init(UNER_Transport *transport, UNER_TransportNrf24 *context)
{
    if (!transport || !context) {
        return;
    }
    transport->context = context;
    transport->poll_rx = UNER_TransportNrf24_Poll;
    transport->try_send = UNER_TransportNrf24_TrySend;
}
