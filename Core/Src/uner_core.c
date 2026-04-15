/*
 * uner_core.c - UNER v2 core parser/queue
 */
#include "stm32f1xx_hal.h"
#include "uner_core.h"
#include <string.h>

static uint8_t uner_accepts_src(const UNER_Core *core, uint8_t src)
{
    return (core->cfg.accept_src_mask & (uint16_t)(1u << src)) != 0u;
}

static uint8_t uner_accepts_dst(const UNER_Core *core, uint8_t dst)
{
    if (dst == UNER_NODE_BROADCAST) {
        return core->cfg.accept_broadcast != 0u;
    }
    return (core->cfg.accept_dst_mask & (uint16_t)(1u << dst)) != 0u;
}

static void uner_reset_parser(UNER_Core *core, uint8_t reset_reason)
{
    __NOP();

    core->last_reset_reason = reset_reason;
    core->last_reset_state = (uint8_t)core->state;

    core->state = UNER_S_H0;
    core->len_expected = 0u;
    core->payload_index = 0u;
    core->chk_acc = 0u;
    core->route = 0u;
    core->cmd = 0u;
    core->ver = 0u;
    core->transport_id = 0u;
    core->max_payload_transport = core->cfg.default_max_payload;
    core->drop_frame = 0u;
}

UNER_Status UNER_Core_Init(
    UNER_Core *core,
    const UNER_CoreConfig *cfg,
    UNER_Packet *slots,
    uint8_t slot_count,
    uint8_t *payload_pool_bytes,
    uint16_t payload_pool_size)
{
    if (!core || !cfg || !slots || !payload_pool_bytes || slot_count == 0u) {
        return UNER_ERR_LEN;
    }

    core->cfg = *cfg;
    core->pkt_slots = slots;
    core->payload_pool = payload_pool_bytes;
    core->slot_count = slot_count;
    core->payload_stride = (uint16_t)(payload_pool_size / slot_count);

    core->q_head = 0;
    core->q_tail = 0;
    core->q_count = 0;

    core->ok_frames = 0;
    core->chk_fail = 0;
    core->len_fail = 0;
    core->header_resync = 0;
    core->token_fail = 0;
    core->ver_fail = 0;
    core->queue_overflow = 0;

    uner_reset_parser(core, UNER_RESET_NONE);
}

static UNER_Status uner_queue_commit(UNER_Core *core, uint8_t len)
{
    if (core->q_count >= core->slot_count) {
        core->queue_overflow++;
        return UNER_ERR_QUEUE_FULL;
    }

    UNER_Packet *slot = &core->pkt_slots[core->q_head];
    slot->ver = core->ver;
    slot->src = (uint8_t)(core->route >> 4);
    slot->dst = (uint8_t)(core->route & 0x0Fu);
    slot->cmd = core->cmd;
    slot->len = len;
    slot->payload = &core->payload_pool[(uint16_t)core->q_head * core->payload_stride];
    slot->transport_id = core->transport_id;

    core->q_head = (uint8_t)((core->q_head + 1u) % core->slot_count);
    core->q_count++;

    if (core->cfg.on_packet) {
        core->cfg.on_packet(core->cfg.cb_ctx, slot);
    }
    __NOP();
    return UNER_OK;
}

UNER_Status UNER_Core_PushByte(
    UNER_Core *core,
    uint8_t transport_id,
    uint8_t max_payload_for_transport,
    uint8_t byte)
{
    if (!core) {
        return UNER_ERR_LEN;
    }

    __NOP();

    volatile uint8_t byteDebug = byte;
    (void)byteDebug;

    core->transport_id = transport_id;
    core->max_payload_transport = max_payload_for_transport;

    if (core->state > UNER_S_CHK) {
        uner_reset_parser(core, UNER_RESET_STATE_INVALID);
    }

    switch (core->state) {
    case UNER_S_H0:
        if (byte == (uint8_t)'U') {
            core->chk_acc = byte;
            core->state = UNER_S_H1;
        }
        __NOP();
        break;

    case UNER_S_H1:
        if (byte == (uint8_t)'N') {
            core->chk_acc ^= byte;
            core->state = UNER_S_H2;
        } else {
            core->header_resync++;
            core->state = (byte == (uint8_t)'U') ? UNER_S_H1 : UNER_S_H0;
            core->chk_acc = (byte == (uint8_t)'U') ? byte : 0u;
        }
        break;

    case UNER_S_H2:
        if (byte == (uint8_t)'E') {
            core->chk_acc ^= byte;
            core->state = UNER_S_H3;
        } else {
            core->header_resync++;
            core->state = (byte == (uint8_t)'U') ? UNER_S_H1 : UNER_S_H0;
            core->chk_acc = (byte == (uint8_t)'U') ? byte : 0u;
        }
        break;

    case UNER_S_H3:
        if (byte == (uint8_t)'R') {
            core->chk_acc ^= byte;
            core->state = UNER_S_LEN;
        } else {
            core->header_resync++;
            core->state = (byte == (uint8_t)'U') ? UNER_S_H1 : UNER_S_H0;
            core->chk_acc = (byte == (uint8_t)'U') ? byte : 0u;
        }
        __NOP();
        break;

    case UNER_S_LEN:
        core->len_expected = byte;
        core->chk_acc ^= byte;

        if (core->len_expected > core->cfg.default_max_payload ||
            core->len_expected > core->max_payload_transport) {
            core->len_fail++;
            uner_reset_parser(core, UNER_RESET_LEN_FAIL);
            return UNER_ERR_LEN;
        }

        if (core->q_count >= core->slot_count) {
            core->queue_overflow++;
            core->drop_frame = 1u;
        }

        core->payload_index = 0u;
        core->state = UNER_S_TOKEN;
        break;

    case UNER_S_TOKEN:
        if (byte != UNER_TOKEN) {
            core->token_fail++;
            uner_reset_parser(core, UNER_RESET_TOKEN_FAIL);
            return UNER_ERR_TOKEN;
        }

        core->chk_acc ^= byte;
        core->state = UNER_S_VER;
        break;

    case UNER_S_VER:
        if (byte != UNER_VERSION) {
            core->ver_fail++;
            uner_reset_parser(core, UNER_RESET_VER_FAIL);
            return UNER_ERR_VER;
        }

        core->ver = byte;
        core->chk_acc ^= byte;
        core->state = UNER_S_ROUTE;
        break;

    case UNER_S_ROUTE:
        core->route = byte;
        core->chk_acc ^= byte;
        core->state = UNER_S_CMD;
        break;

    case UNER_S_CMD:
        core->cmd = byte;
        core->chk_acc ^= byte;

        if (core->len_expected == 0u) {
            core->state = UNER_S_CHK;
            __NOP();
        } else {
            core->state = UNER_S_PAYLOAD;
        }
        break;

    case UNER_S_PAYLOAD:
    {
        if (!core->drop_frame) {
            uint8_t *payload_base = &core->payload_pool[(uint16_t)core->q_head * core->payload_stride];
            payload_base[core->payload_index++] = byte;
        } else {
            core->payload_index++;
        }

        core->chk_acc ^= byte;

        if (core->payload_index >= core->len_expected) {
            core->state = UNER_S_CHK;
            __NOP();
        }
        break;
    }

    case UNER_S_CHK:
    {
        __NOP();

        if (byte != core->chk_acc) {
            core->chk_fail++;
            __NOP();
            uner_reset_parser(core, UNER_RESET_CHK_FAIL);
            return UNER_ERR_CHK;
        }

        __NOP();

        uint8_t src = (uint8_t)(core->route >> 4);
        uint8_t dst = (uint8_t)(core->route & 0x0Fu);

        if (!core->drop_frame && uner_accepts_src(core, src) && uner_accepts_dst(core, dst)) {
            if (uner_queue_commit(core, core->len_expected) == UNER_OK) {
                core->ok_frames++;
            }
        }

        __NOP();
        uner_reset_parser(core, UNER_RESET_FRAME_DONE);
        break;
    }

    default:
        uner_reset_parser(core, UNER_RESET_DEFAULT_CASE);
        break;
    }

    __NOP();
    return UNER_OK;
}
uint8_t UNER_Core_Dequeue(UNER_Core *core, UNER_Packet *out)
{
    if (!core || !out || core->q_count == 0u) {
        return 0u;
    }

    *out = core->pkt_slots[core->q_tail];
    core->q_tail = (uint8_t)((core->q_tail + 1u) % core->slot_count);
    core->q_count--;
    return 1u;
}

void UNER_Core_ReleasePacket(UNER_Core *core, const UNER_Packet *p)
{
    (void)core;
    (void)p;
}

uint8_t UNER_Core_ComputeChecksum(const uint8_t *data, uint16_t len)
{
    uint8_t chk = 0u;
    for (uint16_t i = 0; i < len; ++i) {
        chk ^= data[i];
    }
    return chk;
}

UNER_Status UNER_BuildFrame(
    uint8_t *out,
    uint16_t out_max,
    uint8_t src,
    uint8_t dst,
    uint8_t cmd,
    const uint8_t *payload,
    uint8_t len)
{
    uint16_t total = (uint16_t)(UNER_OVERHEAD + len);
    if (!out || out_max < total) {
        return UNER_ERR_LEN;
    }

    out[0] = (uint8_t)'U';
    out[1] = (uint8_t)'N';
    out[2] = (uint8_t)'E';
    out[3] = (uint8_t)'R';
    out[4] = len;
    out[5] = UNER_TOKEN;
    out[6] = UNER_VERSION;
    out[7] = (uint8_t)((src << 4) | (dst & 0x0Fu));
    out[8] = cmd;

    if (len > 0u && payload) {
        memcpy(&out[9], payload, len);
    }

    out[total - 1u] = UNER_Core_ComputeChecksum(out, (uint16_t)(total - 1u));
    return UNER_OK;
}
