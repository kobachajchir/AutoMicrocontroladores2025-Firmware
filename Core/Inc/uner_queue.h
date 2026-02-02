/*
 * uner_queue.h
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#ifndef INC_UNER_QUEUE_H_
#define INC_UNER_QUEUE_H_

#include "uner_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UNER_QUEUE_SLOTS 4U
#define UNER_QUEUE_PAYLOAD_SIZE 255U

typedef struct {
    UNER_Packet slots[UNER_QUEUE_SLOTS];
    uint8_t payload_pool[UNER_QUEUE_SLOTS][UNER_QUEUE_PAYLOAD_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} UNER_Queue;

void UNER_Queue_Init(UNER_Queue *queue);
bool UNER_Queue_Reserve(UNER_Queue *queue, UNER_Packet **out_packet, uint8_t **out_payload);
bool UNER_Queue_Commit(UNER_Queue *queue);
bool UNER_Queue_Dequeue(UNER_Queue *queue, UNER_Packet *out_packet);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_QUEUE_H_ */
