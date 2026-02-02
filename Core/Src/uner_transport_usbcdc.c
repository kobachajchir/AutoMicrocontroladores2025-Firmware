/*
 * uner_transport_usbcdc.c - UNER USB CDC transport (stub)
 */
#include "uner_transport_usbcdc.h"

static UNER_Status UNER_TransportUsbCdc_PollRx(UNER_Transport *transport, UNER_Core *core)
{
    (void)transport;
    (void)core;
    return UNER_OK;
}

static UNER_Status UNER_TransportUsbCdc_TrySend(UNER_Transport *transport, const uint8_t *buf, uint16_t len)
{
    (void)transport;
    (void)buf;
    (void)len;
    return UNER_ERR_BUSY;
}

UNER_Status UNER_TransportUsbCdc_Init(UNER_TransportUsbCdc *transport, uint8_t transport_id)
{
    if (!transport) {
        return UNER_ERR_LEN;
    }

    transport->base.id = transport_id;
    transport->base.max_payload = 255u;
    transport->base.poll_rx = UNER_TransportUsbCdc_PollRx;
    transport->base.try_send = UNER_TransportUsbCdc_TrySend;
    transport->base.ctx = transport;

    return UNER_OK;
}
