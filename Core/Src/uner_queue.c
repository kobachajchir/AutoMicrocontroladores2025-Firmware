/*
 * uner_queue.c
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#include "uner_queue.h"

void UNER_Queue_Init(UNER_Queue *queue)
{
    if (!queue) {
        return;
    }
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}

bool UNER_Queue_Reserve(UNER_Queue *queue, UNER_Packet **out_packet, uint8_t **out_payload)
{
    if (!queue || !out_packet || !out_payload) {
        return false;
    }
    if (queue->count >= UNER_QUEUE_SLOTS) {
        return false;
    }
    *out_packet = &queue->slots[queue->head];
    *out_payload = queue->payload_pool[queue->head];
    return true;
}

bool UNER_Queue_Commit(UNER_Queue *queue)
{
    if (!queue || queue->count >= UNER_QUEUE_SLOTS) {
        return false;
    }
    queue->head = (uint8_t)((queue->head + 1U) % UNER_QUEUE_SLOTS);
    queue->count++;
    return true;
}

bool UNER_Queue_Dequeue(UNER_Queue *queue, UNER_Packet *out_packet)
{
    if (!queue || !out_packet || queue->count == 0) {
        return false;
    }
    *out_packet = queue->slots[queue->tail];
    queue->tail = (uint8_t)((queue->tail + 1U) % UNER_QUEUE_SLOTS);
    queue->count--;
    return true;
}
