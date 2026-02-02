/*
 * uner_core.h
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#ifndef INC_UNER_CORE_H_
#define INC_UNER_CORE_H_

#include "uner_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*UNER_OnPacket)(const UNER_Packet *packet, void *user_ctx);

typedef struct {
    UNER_ParserState state;
    uint8_t len_expected;
    uint8_t payload_index;
    uint8_t chk_acc;
    uint8_t route;
    uint8_t cmd;
    uint8_t transport_id;
    UNER_Queue *queue;
    UNER_OnPacket on_packet;
    void *user_ctx;
    UNER_CoreStats stats;
    UNER_Packet *current_packet;
    uint8_t *payload_ptr;
} UNER_Core;

void UNER_Core_Init(UNER_Core *core, UNER_Queue *queue, UNER_OnPacket callback, void *user_ctx);
void UNER_Core_ResetParser(UNER_Core *core);
UNER_Status UNER_Core_PushByte(UNER_Core *core, uint8_t transport_id, uint8_t max_payload, uint8_t byte);
bool UNER_Core_Dequeue(UNER_Core *core, UNER_Packet *out_packet);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_CORE_H_ */
