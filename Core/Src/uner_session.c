/*
 * uner_session.c - UNER v2 session with seq/ack/resp handling
 */
#include "uner_session.h"
#include <string.h>

static const UNER_CmdMeta *uner_find_meta(const UNER_Session *session, uint8_t cmd_id)
{
    if (!session || !session->cfg.table) {
        return NULL;
    }

    for (uint16_t i = 0; i < session->cfg.table_count; ++i) {
        if (session->cfg.table[i].cmd_id == cmd_id) {
            return &session->cfg.table[i];
        }
    }

    return NULL;
}

void UNER_Session_Init(UNER_Session *session, const UNER_SessionConfig *cfg)
{
    if (!session || !cfg) {
        return;
    }

    session->cfg = *cfg;
    session->current.active = 0;
    session->current.ack_received = 0;
    session->current.waiting_resp = 0;
    session->current.seq = 0;
    session->current.cmd_id = 0;
    session->current.retries = 0;
    session->current.ticks_waited = 0;
    session->current.timeout_ticks = 0;
    session->current.ack_timer = 0;
    session->next_seq = 0;
}

UNER_Status UNER_Session_Send(UNER_Session *session,
                              uint8_t transport_id,
                              uint8_t src,
                              uint8_t dst,
                              uint8_t cmd_id,
                              const uint8_t *payload,
                              uint8_t length)
{
    if (!session || !session->cfg.send_fn) {
        return UNER_ERR_LEN;
    }

    if (session->current.active) {
        return UNER_ERR_BUSY;
    }

    const UNER_CmdMeta *meta = uner_find_meta(session, cmd_id);
    if (!meta) {
        return UNER_ERR_LEN;
    }

    uint8_t seq = session->next_seq++;
    uint16_t timeout = meta->timeout_ticks ? meta->timeout_ticks : session->cfg.default_timeout_ticks;
    uint8_t needs_ack = (meta->flags & UNER_FLAG_NEED_ACK) != 0u;
    uint8_t needs_resp = (meta->flags & UNER_FLAG_NEED_RESP) != 0u;

    uint8_t frame[UNER_OVERHEAD + 255u];
    uint8_t temp_payload[256u];
    temp_payload[0] = seq;
    if (length > 0u && payload) {
        memcpy(&temp_payload[1], payload, length);
    }

    UNER_Status st = UNER_BuildFrame(frame,
                                     sizeof(frame),
                                     src,
                                     dst,
                                     cmd_id,
                                     temp_payload,
                                     (uint8_t)(length + 1u));
    if (st != UNER_OK) {
        return st;
    }

    st = session->cfg.send_fn(session->cfg.send_ctx, frame,
                              (uint16_t)(UNER_OVERHEAD + (uint16_t)(length + 1u)));
    if (st != UNER_OK) {
        return st;
    }

    session->current.active = 1;
    session->current.ack_received = (needs_ack == 0u);
    session->current.waiting_resp = needs_resp;
    session->current.seq = seq;
    session->current.cmd_id = cmd_id;
    session->current.retries = 1;
    session->current.ticks_waited = 0;
    session->current.timeout_ticks = timeout;
    session->current.ack_timer = needs_ack ? session->cfg.ack_timeout_ticks : 0u;

    (void)transport_id;
    return UNER_OK;
}

static void uner_complete_cmd(UNER_Session *session)
{
    session->current.active = 0;
    session->current.ack_received = 0;
    session->current.waiting_resp = 0;
    session->current.seq = 0;
    session->current.cmd_id = 0;
    session->current.retries = 0;
    session->current.ticks_waited = 0;
    session->current.timeout_ticks = 0;
    session->current.ack_timer = 0;
}

void UNER_Session_OnPacket(UNER_Session *session, const UNER_Packet *pkt)
{
    if (!session || !pkt || pkt->len == 0u) {
        return;
    }

    uint8_t seq = pkt->payload[0];
    const UNER_CmdMeta *meta = uner_find_meta(session, pkt->cmd);

    if (pkt->cmd == UNER_CMD_ACK) {
        if (session->current.active && seq == session->current.seq) {
            session->current.ack_received = 1;
            if (!session->current.waiting_resp) {
                uner_complete_cmd(session);
            }
        }
        return;
    }

    if (meta && (meta->flags & UNER_FLAG_IS_EVENT)) {
        if (meta->handler) {
            meta->handler(&pkt->payload[1], (uint8_t)(pkt->len - 1u));
        }
        if (session->cfg.on_event) {
            session->cfg.on_event(session->cfg.cb_ctx, pkt->cmd, seq,
                                  &pkt->payload[1], (uint8_t)(pkt->len - 1u));
        }
        return;
    }

    if (session->current.active &&
        session->current.seq == seq &&
        session->current.cmd_id == pkt->cmd) {
        if (session->cfg.on_response) {
            session->cfg.on_response(session->cfg.cb_ctx, pkt->cmd, seq,
                                     &pkt->payload[1], (uint8_t)(pkt->len - 1u));
        }
        uner_complete_cmd(session);
    }
}

void UNER_Session_Tick10ms(UNER_Session *session)
{
    if (!session || !session->current.active) {
        return;
    }

    session->current.ticks_waited++;
    if (session->current.timeout_ticks > 0u) {
        session->current.timeout_ticks--;
    }

    if (!session->current.ack_received && session->current.ack_timer > 0u) {
        session->current.ack_timer--;
        if (session->current.ack_timer == 0u) {
            if (session->current.retries < session->cfg.max_retries) {
                session->current.retries++;
                session->current.ack_timer = session->cfg.ack_timeout_ticks;
            } else {
                if (session->cfg.on_timeout) {
                    session->cfg.on_timeout(session->cfg.cb_ctx,
                                            session->current.cmd_id,
                                            session->current.seq);
                }
                uner_complete_cmd(session);
            }
        }
    }

    if (session->current.timeout_ticks == 0u && session->current.active) {
        if (session->cfg.on_timeout) {
            session->cfg.on_timeout(session->cfg.cb_ctx,
                                    session->current.cmd_id,
                                    session->current.seq);
        }
        uner_complete_cmd(session);
    }
}
