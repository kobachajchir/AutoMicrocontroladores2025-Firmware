/*
 * uner_core.h - UNER v2 core parser/queue
 */
#ifndef INC_UNER_CORE_H_
#define INC_UNER_CORE_H_

#include <stdint.h>
#include <stddef.h>
#include "uner_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UNER_OK = 0,
    UNER_ERR_BUSY,
    UNER_ERR_LEN,
    UNER_ERR_CHK,
    UNER_ERR_HEADER,
    UNER_ERR_TOKEN,
    UNER_ERR_VER,
    UNER_ERR_QUEUE_FULL,
} UNER_Status;

typedef struct {
    uint8_t ver;
    uint8_t src;
    uint8_t dst;
    uint8_t cmd;
    uint8_t len;
    uint8_t *payload;
    uint8_t transport_id;
} UNER_Packet;

typedef void (*UNER_OnPacketCb)(void *ctx, const UNER_Packet *p);

typedef struct {
    uint8_t this_node;
    uint16_t accept_src_mask;
    uint16_t accept_dst_mask;
    uint8_t accept_broadcast;
    uint8_t default_max_payload;
    UNER_OnPacketCb on_packet;
    void *cb_ctx;
} UNER_CoreConfig;

typedef enum {
    UNER_S_H0 = 0,
    UNER_S_H1,
    UNER_S_H2,
    UNER_S_H3,
    UNER_S_LEN,
    UNER_S_TOKEN,
    UNER_S_VER,
    UNER_S_ROUTE,
    UNER_S_CMD,
    UNER_S_PAYLOAD,
    UNER_S_CHK
} UNER_ParserState;

typedef struct {
    UNER_CoreConfig cfg;

    uint8_t state;
    uint8_t len_expected;
    uint8_t payload_index;
    uint8_t chk_acc;
    uint8_t route;
    uint8_t cmd;
    uint8_t ver;
    uint8_t transport_id;
    uint8_t max_payload_transport;
    uint8_t drop_frame;

    UNER_Packet *pkt_slots;
    uint8_t *payload_pool;
    uint16_t payload_stride;
    uint8_t slot_count;
    volatile uint8_t q_head;
    volatile uint8_t q_tail;
    volatile uint8_t q_count;

    uint32_t ok_frames;
    uint32_t chk_fail;
    uint32_t len_fail;
    uint32_t header_resync;
    uint32_t token_fail;
    uint32_t ver_fail;
    uint32_t queue_overflow;
} UNER_Core;

UNER_Status UNER_Core_Init(
    UNER_Core *core,
    const UNER_CoreConfig *cfg,
    UNER_Packet *slots,
    uint8_t slot_count,
    uint8_t *payload_pool_bytes,
    uint16_t payload_pool_size);

void UNER_Core_ResetParser(UNER_Core *core);

UNER_Status UNER_Core_PushByte(
    UNER_Core *core,
    uint8_t transport_id,
    uint8_t max_payload_for_transport,
    uint8_t byte);

uint8_t UNER_Core_Dequeue(UNER_Core *core, UNER_Packet *out);
void UNER_Core_ReleasePacket(UNER_Core *core, const UNER_Packet *p);

uint8_t UNER_Core_ComputeChecksum(const uint8_t *data, uint16_t len);
UNER_Status UNER_BuildFrame(
    uint8_t *out,
    uint16_t out_max,
    uint8_t src,
    uint8_t dst,
    uint8_t cmd,
    const uint8_t *payload,
    uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_CORE_H_ */
