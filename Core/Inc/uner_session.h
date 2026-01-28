/*
 * uner_session.h - UNER v2 session with seq/ack/resp handling
 */
#ifndef INC_UNER_SESSION_H_
#define INC_UNER_SESSION_H_

#include <stdint.h>
#include "uner_core.h"
#include "uner_cmd_meta.h"
#include "uner_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef UNER_Status (*UNER_SendFn)(void *ctx, const uint8_t *buf, uint16_t len);
typedef void (*UNER_OnTimeout)(void *ctx, uint8_t cmd_id, uint8_t seq);
typedef void (*UNER_OnResponse)(void *ctx, uint8_t cmd_id, uint8_t seq,
                                const uint8_t *payload, uint8_t length);
typedef void (*UNER_OnEvent)(void *ctx, uint8_t cmd_id, uint8_t seq,
                             const uint8_t *payload, uint8_t length);

typedef struct {
    const UNER_CmdMeta *table;
    uint16_t table_count;
    UNER_SendFn send_fn;
    void *send_ctx;
    UNER_OnTimeout on_timeout;
    UNER_OnResponse on_response;
    UNER_OnEvent on_event;
    void *cb_ctx;
    uint16_t default_timeout_ticks;
    uint8_t max_retries;
    uint8_t ack_timeout_ticks;
} UNER_SessionConfig;

typedef struct {
    uint8_t active;
    uint8_t ack_received;
    uint8_t waiting_resp;
    uint8_t seq;
    uint8_t cmd_id;
    uint8_t retries;
    uint16_t ticks_waited;
    uint16_t timeout_ticks;
    uint16_t ack_timer;
} UNER_ActiveCmd;

typedef struct {
    UNER_SessionConfig cfg;
    UNER_ActiveCmd current;
    uint8_t next_seq;
} UNER_Session;

void UNER_Session_Init(UNER_Session *session, const UNER_SessionConfig *cfg);

UNER_Status UNER_Session_Send(UNER_Session *session,
                              uint8_t transport_id,
                              uint8_t src,
                              uint8_t dst,
                              uint8_t cmd_id,
                              const uint8_t *payload,
                              uint8_t length);

void UNER_Session_OnPacket(UNER_Session *session, const UNER_Packet *pkt);
void UNER_Session_Tick10ms(UNER_Session *session);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_SESSION_H_ */
