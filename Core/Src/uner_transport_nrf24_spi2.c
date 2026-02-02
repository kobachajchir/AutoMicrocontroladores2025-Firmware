/*
 * uner_transport_nrf24_spi2.c - UNER nRF24 transport (SPI2)
 */
#include "uner_transport_nrf24_spi2.h"
#include "uner_v2.h"

static UNER_Status UNER_TransportNrf24_PollRx(UNER_Transport *transport, UNER_Core *core)
{
    UNER_TransportNrf24 *nrf = (UNER_TransportNrf24 *)transport->ctx;
    if (!nrf || !core || !nrf->read_payload || !nrf->rx_buf) {
        return UNER_ERR_LEN;
    }

    uint8_t rx_ready = 0u;
    if (nrf->irq_pending && *nrf->irq_pending) {
        rx_ready = 1u;
        *nrf->irq_pending = 0u;
    } else if (nrf->is_rx_ready) {
        rx_ready = nrf->is_rx_ready(nrf->driver_ctx);
    }

    if (!rx_ready) {
        return UNER_OK;
    }

    uint8_t len = nrf->read_payload(nrf->driver_ctx, nrf->rx_buf, nrf->rx_buf_len);
    if (len == 0u) {
        return UNER_OK;
    }

    for (uint8_t i = 0; i < len; ++i) {
        UNER_Core_PushByte(core, transport->id, transport->max_payload, nrf->rx_buf[i]);
    }

    return UNER_OK;
}

static UNER_Status UNER_TransportNrf24_TrySend(UNER_Transport *transport, const uint8_t *buf, uint16_t len)
{
    UNER_TransportNrf24 *nrf = (UNER_TransportNrf24 *)transport->ctx;
    if (!nrf || !nrf->send_payload) {
        return UNER_ERR_LEN;
    }

    if (len > 32u) {
        return UNER_ERR_LEN;
    }

    if (!nrf->send_payload(nrf->driver_ctx, buf, (uint8_t)len)) {
        return UNER_ERR_BUSY;
    }

    return UNER_OK;
}

UNER_Status UNER_TransportNrf24_Init(
    UNER_TransportNrf24 *transport,
    uint8_t transport_id,
    uint8_t max_payload,
    uint8_t *rx_buf,
    uint8_t rx_buf_len,
    volatile uint8_t *irq_pending,
    UNER_NrfIsRxReadyFn is_rx_ready,
    UNER_NrfReadPayloadFn read_payload,
    UNER_NrfSendPayloadFn send_payload,
    void *driver_ctx)
{
    if (!transport || !rx_buf || rx_buf_len == 0u || !read_payload || !send_payload) {
        return UNER_ERR_LEN;
    }

    transport->base.id = transport_id;
    transport->base.max_payload = max_payload;
    transport->base.poll_rx = UNER_TransportNrf24_PollRx;
    transport->base.try_send = UNER_TransportNrf24_TrySend;
    transport->base.ctx = transport;

    transport->rx_buf = rx_buf;
    transport->rx_buf_len = rx_buf_len;
    transport->irq_pending = irq_pending;
    transport->is_rx_ready = is_rx_ready;
    transport->read_payload = read_payload;
    transport->send_payload = send_payload;
    transport->driver_ctx = driver_ctx;

    return UNER_OK;
}
