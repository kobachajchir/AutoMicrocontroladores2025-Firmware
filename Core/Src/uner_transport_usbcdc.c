/*
 * uner_transport_usbcdc.c
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#include "uner_transport_usbcdc.h"

static void UNER_TransportUsbCdc_Poll(UNER_Transport *transport, UNER_Core *core)
{
    (void)transport;
    (void)core;
}

static UNER_Status UNER_TransportUsbCdc_TrySend(UNER_Transport *transport, const uint8_t *buf, uint16_t len)
{
    (void)transport;
    (void)buf;
    (void)len;
    return UNER_STATUS_ERR_BUSY;
}

void UNER_TransportUsbCdc_Init(UNER_Transport *transport, UNER_TransportUsbCdc *context)
{
    if (!transport || !context) {
        return;
    }
    transport->context = context;
    transport->poll_rx = UNER_TransportUsbCdc_Poll;
    transport->try_send = UNER_TransportUsbCdc_TrySend;
}
