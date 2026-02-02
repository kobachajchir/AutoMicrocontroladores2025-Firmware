/*
 * uner_handle.c - UNER v2 manager
 */
#include "uner_handle.h"
#include <string.h>

UNER_Status UNER_Handle_Init(
    UNER_Handle *handle,
    const UNER_CoreConfig *cfg,
    UNER_Packet *slots,
    uint8_t slot_count,
    uint8_t *payload_pool_bytes,
    uint16_t payload_pool_size)
{
    if (!handle) {
        return UNER_ERR_LEN;
    }

    handle->transport_count = 0;
    memset(handle->transports, 0, sizeof(handle->transports));

    return UNER_Core_Init(
        &handle->core,
        cfg,
        slots,
        slot_count,
        payload_pool_bytes,
        payload_pool_size);
}

UNER_Status UNER_Handle_RegisterTransport(UNER_Handle *handle, UNER_Transport *transport)
{
    if (!handle || !transport) {
        return UNER_ERR_LEN;
    }

    if (handle->transport_count >= UNER_MAX_TRANSPORTS) {
        return UNER_ERR_LEN;
    }

    handle->transports[handle->transport_count++] = transport;
    return UNER_OK;
}

void UNER_Handle_Poll(UNER_Handle *handle)
{
    if (!handle) {
        return;
    }

    for (uint8_t i = 0; i < handle->transport_count; ++i) {
        UNER_Transport *transport = handle->transports[i];
        if (transport && transport->poll_rx) {
            transport->poll_rx(transport, &handle->core);
        }
    }
}

UNER_Status UNER_Send(
    UNER_Handle *handle,
    uint8_t transport_id,
    uint8_t src,
    uint8_t dst,
    uint8_t cmd,
    const uint8_t *payload,
    uint8_t len)
{
    if (!handle) {
        return UNER_ERR_LEN;
    }

    UNER_Transport *transport = NULL;
    for (uint8_t i = 0; i < handle->transport_count; ++i) {
        if (handle->transports[i] && handle->transports[i]->id == transport_id) {
            transport = handle->transports[i];
            break;
        }
    }

    if (!transport) {
        return UNER_ERR_LEN;
    }

    if (len > transport->max_payload) {
        return UNER_ERR_LEN;
    }

    UNER_Status status = UNER_BuildFrame(handle->tx_buf, sizeof(handle->tx_buf), src, dst, cmd, payload, len);
    if (status != UNER_OK) {
        return status;
    }

    uint16_t total = (uint16_t)(UNER_OVERHEAD + len);
    return transport->try_send(transport, handle->tx_buf, total);
}
