/*
 * uner_handle.c - UNER v2 manager
 */
#include "stm32f1xx_hal.h"
#include "uner_handle.h"
#include <string.h>

static void UNER_Handle_OnPacketQueued(void *ctx, const UNER_Packet *p)
{
    UNER_Handle *handle = (UNER_Handle *)ctx;
    (void)p;
    if (!handle) {
        return;
    }
    handle->packet_pending = 1u;
    __NOP();
}


static uint8_t UNER_Handle_IsRawAsyncPush(const UNER_Packet *packet)
{
    if (!packet || packet->len == 0u || !packet->payload) {
        return 0u;
    }

    if ((packet->cmd == 0x15u || packet->cmd == 0x48u || packet->cmd == 0x4Bu) && packet->payload[0] == 0xFEu) {
        return 1u;
    }

    return 0u;
}

static uint8_t UNER_Handle_IsLengthValid(const UNER_CommandSpec *spec, const UNER_Packet *packet)
{
    if (!spec || !packet) {
        return 0u;
    }

    if (packet->len >= spec->min_args && packet->len <= spec->max_args) {
        return 1u;
    }

    if ((spec->flags & UNER_SPEC_F_RESP) && packet->len >= 1u) {
        return 1u;
    }

    if (UNER_Handle_IsRawAsyncPush(packet)) {
        return 1u;
    }

    return 0u;
}

static uint8_t UNER_Handle_FindCommandSpec(
    const UNER_CommandSpec *table,
    uint8_t count,
    uint8_t cmd,
    UNER_CommandSpec *out)
{
    if (!table || !out) {
        return 0u;
    }

    for (uint8_t i = 0u; i < count; ++i) {
        if (table[i].id == cmd) {
            *out = table[i];
            return 1u;
        }
    }
    return 0u;
}

UNER_Status UNER_Handle_Init(
    UNER_Handle *handle,
    const UNER_CoreConfig *cfg,
    UNER_Packet *slots,
    uint8_t slot_count,
    uint8_t *payload_pool_bytes,
    uint16_t payload_pool_size)
{
    if (!handle || !cfg) {
        return UNER_ERR_LEN;
    }

    handle->transport_count = 0;
    memset(handle->transports, 0, sizeof(handle->transports));

    handle->packet_pending = 0u;
    handle->command_table = NULL;
    handle->command_count = 0u;
    handle->execute_command = NULL;
    handle->execute_ctx = NULL;
    handle->app_on_packet = cfg->on_packet;
    handle->app_on_packet_ctx = cfg->cb_ctx;

    UNER_CoreConfig cfg_local = *cfg;
    cfg_local.on_packet = UNER_Handle_OnPacketQueued;
    cfg_local.cb_ctx = handle;

    return UNER_Core_Init(
        &handle->core,
        &cfg_local,
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

UNER_Status UNER_Handle_RegisterCommands(
    UNER_Handle *handle,
    const UNER_CommandSpec *table,
    uint8_t count,
    UNER_ExecuteCommandFn execute_command,
    void *execute_ctx)
{
    if (!handle || !table || count == 0u) {
        return UNER_ERR_LEN;
    }

    handle->command_table = table;
    handle->command_count = count;
    handle->execute_command = execute_command;
    handle->execute_ctx = execute_ctx;
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

void UNER_Handle_ProcessPending(UNER_Handle *handle)
{
    if (!handle || !handle->packet_pending) {
        return;
    }

    UNER_Packet packet;
    while (UNER_Core_Dequeue(&handle->core, &packet)) {
        uint8_t valid_cmd = 0u;
        UNER_CommandSpec spec = {0u, 0u, 0u, 0u, 0u, NULL};

        if (UNER_Handle_FindCommandSpec(handle->command_table, handle->command_count, packet.cmd, &spec)) {
            if (UNER_Handle_IsLengthValid(&spec, &packet)) {
                valid_cmd = 1u;
            }
        }

        if (valid_cmd) {
            if (spec.handler) {
                spec.handler(handle->execute_ctx, &packet);
            } else if (handle->execute_command) {
            	__NOP();
                handle->execute_command(handle->execute_ctx, &packet);
            }

            if (handle->app_on_packet) {
                handle->app_on_packet(handle->app_on_packet_ctx, &packet);
            }
        }

        UNER_Core_ReleasePacket(&handle->core, &packet);
    }

    handle->packet_pending = 0u;
    __NOP();
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
