/*
 * uner_handle.c
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#include "uner_handle.h"

void UNER_Init(UNER_Handle *handle, UNER_Transport *transports, uint8_t transport_count, uint8_t this_node)
{
    if (!handle || !transports || transport_count == 0U) {
        return;
    }
    handle->transports = transports;
    handle->transport_count = transport_count;
    handle->this_node = this_node;
    UNER_Core_Init(&handle->core, &handle->queue, NULL, NULL);
}

void UNER_Poll(UNER_Handle *handle)
{
    if (!handle || !handle->transports) {
        return;
    }
    for (uint8_t i = 0; i < handle->transport_count; ++i) {
        UNER_Transport *transport = &handle->transports[i];
        if (transport->poll_rx) {
            transport->poll_rx(transport, &handle->core);
        }
    }
}

UNER_Status UNER_Send(UNER_Handle *handle,
                      uint8_t transport_id,
                      uint8_t src,
                      uint8_t dst,
                      uint8_t cmd,
                      const uint8_t *payload,
                      uint8_t payload_len)
{
    if (!handle || !handle->transports) {
        return UNER_STATUS_ERR_NULL;
    }
    for (uint8_t i = 0; i < handle->transport_count; ++i) {
        UNER_Transport *transport = &handle->transports[i];
        if (transport->id == transport_id) {
            if (payload_len > transport->max_payload) {
                return UNER_STATUS_ERR_LEN;
            }
            uint8_t frame[UNER_OVERHEAD + UNER_MAX_PAYLOAD];
            uint16_t frame_len = 0;
            UNER_Status status = UNER_BuildFrame(frame, sizeof(frame), src, dst, cmd, payload, payload_len, &frame_len);
            if (status != UNER_STATUS_OK) {
                return status;
            }
            if (!transport->try_send) {
                return UNER_STATUS_ERR_TRANSPORT;
            }
            return transport->try_send(transport, frame, frame_len);
        }
    }
    return UNER_STATUS_ERR_TRANSPORT;
}

bool UNER_Dequeue(UNER_Handle *handle, UNER_Packet *out_packet)
{
    if (!handle) {
        return false;
    }
    return UNER_Core_Dequeue(&handle->core, out_packet);
}
