/*
 * uner_core.c
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#include "uner_core.h"

static void UNER_Core_ResetInternal(UNER_Core *core)
{
    core->state = UNER_STATE_H0;
    core->len_expected = 0;
    core->payload_index = 0;
    core->chk_acc = 0;
    core->route = 0;
    core->cmd = 0;
    core->transport_id = 0;
    core->current_packet = NULL;
    core->payload_ptr = NULL;
}

void UNER_Core_Init(UNER_Core *core, UNER_Queue *queue, UNER_OnPacket callback, void *user_ctx)
{
    if (!core) {
        return;
    }
    core->queue = queue;
    core->on_packet = callback;
    core->user_ctx = user_ctx;
    core->stats.ok = 0;
    core->stats.chk_fail = 0;
    core->stats.len_fail = 0;
    core->stats.token_fail = 0;
    core->stats.ver_fail = 0;
    core->stats.header_resync = 0;
    core->stats.queue_overflow = 0;
    UNER_Core_ResetInternal(core);
    if (queue) {
        UNER_Queue_Init(queue);
    }
}

void UNER_Core_ResetParser(UNER_Core *core)
{
    if (!core) {
        return;
    }
    UNER_Core_ResetInternal(core);
}

static void UNER_Core_ResyncHeader(UNER_Core *core, uint8_t byte)
{
    UNER_Core_ResetInternal(core);
    if (byte == 'U') {
        core->state = UNER_STATE_H1;
        core->chk_acc = 'U';
    }
}

static bool UNER_Core_ReservePacket(UNER_Core *core)
{
    if (!core->queue) {
        return false;
    }
    if (!UNER_Queue_Reserve(core->queue, &core->current_packet, &core->payload_ptr)) {
        core->stats.queue_overflow++;
        return false;
    }
    return true;
}

static void UNER_Core_CommitPacket(UNER_Core *core)
{
    if (core->queue) {
        UNER_Queue_Commit(core->queue);
    }
    core->stats.ok++;
    if (core->on_packet) {
        core->on_packet(core->current_packet, core->user_ctx);
    }
}

UNER_Status UNER_Core_PushByte(UNER_Core *core, uint8_t transport_id, uint8_t max_payload, uint8_t byte)
{
    if (!core) {
        return UNER_STATUS_ERR_NULL;
    }
    switch (core->state) {
        case UNER_STATE_H0:
            if (byte == 'U') {
                core->chk_acc = byte;
                core->state = UNER_STATE_H1;
            }
            break;
        case UNER_STATE_H1:
            if (byte == 'N') {
                core->chk_acc ^= byte;
                core->state = UNER_STATE_H2;
            } else {
                core->stats.header_resync++;
                UNER_Core_ResyncHeader(core, byte);
            }
            break;
        case UNER_STATE_H2:
            if (byte == 'E') {
                core->chk_acc ^= byte;
                core->state = UNER_STATE_H3;
            } else {
                core->stats.header_resync++;
                UNER_Core_ResyncHeader(core, byte);
            }
            break;
        case UNER_STATE_H3:
            if (byte == 'R') {
                core->chk_acc ^= byte;
                core->state = UNER_STATE_LEN;
            } else {
                core->stats.header_resync++;
                UNER_Core_ResyncHeader(core, byte);
            }
            break;
        case UNER_STATE_LEN:
            core->len_expected = byte;
            core->chk_acc ^= byte;
            if (core->len_expected > max_payload) {
                core->stats.len_fail++;
                UNER_Core_ResyncHeader(core, byte);
            } else {
                core->state = UNER_STATE_TOKEN;
            }
            break;
        case UNER_STATE_TOKEN:
            core->chk_acc ^= byte;
            if (byte != UNER_TOKEN) {
                core->stats.token_fail++;
                UNER_Core_ResyncHeader(core, byte);
            } else {
                core->state = UNER_STATE_VER;
            }
            break;
        case UNER_STATE_VER:
            core->chk_acc ^= byte;
            if (byte != UNER_VERSION) {
                core->stats.ver_fail++;
                UNER_Core_ResyncHeader(core, byte);
            } else {
                core->state = UNER_STATE_ROUTE;
            }
            break;
        case UNER_STATE_ROUTE:
            core->route = byte;
            core->chk_acc ^= byte;
            core->state = UNER_STATE_CMD;
            break;
        case UNER_STATE_CMD:
            core->cmd = byte;
            core->chk_acc ^= byte;
            core->payload_index = 0;
            if (!UNER_Core_ReservePacket(core)) {
                UNER_Core_ResetInternal(core);
                break;
            }
            core->current_packet->cmd = core->cmd;
            core->current_packet->route = core->route;
            core->current_packet->src = UNER_ROUTE_SRC(core->route);
            core->current_packet->dst = UNER_ROUTE_DST(core->route);
            core->current_packet->payload_len = core->len_expected;
            core->current_packet->payload = core->payload_ptr;
            core->current_packet->transport_id = transport_id;
            if (core->len_expected == 0U) {
                core->state = UNER_STATE_CHK;
            } else {
                core->state = UNER_STATE_PAYLOAD;
            }
            break;
        case UNER_STATE_PAYLOAD:
            if (core->payload_index < core->len_expected) {
                core->payload_ptr[core->payload_index++] = byte;
                core->chk_acc ^= byte;
            }
            if (core->payload_index >= core->len_expected) {
                core->state = UNER_STATE_CHK;
            }
            break;
        case UNER_STATE_CHK:
            if (core->chk_acc == byte) {
                UNER_Core_CommitPacket(core);
            } else {
                core->stats.chk_fail++;
            }
            UNER_Core_ResetInternal(core);
            break;
        default:
            UNER_Core_ResetInternal(core);
            break;
    }
    return UNER_STATUS_OK;
}

bool UNER_Core_Dequeue(UNER_Core *core, UNER_Packet *out_packet)
{
    if (!core || !core->queue) {
        return false;
    }
    return UNER_Queue_Dequeue(core->queue, out_packet);
}
