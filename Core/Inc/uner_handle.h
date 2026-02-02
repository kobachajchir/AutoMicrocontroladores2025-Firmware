/*
 * uner_handle.h
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#ifndef INC_UNER_HANDLE_H_
#define INC_UNER_HANDLE_H_

#include "uner_frame.h"
#include "uner_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    UNER_Core core;
    UNER_Queue queue;
    UNER_Transport *transports;
    uint8_t transport_count;
    uint8_t this_node;
} UNER_Handle;

void UNER_Init(UNER_Handle *handle, UNER_Transport *transports, uint8_t transport_count, uint8_t this_node);
void UNER_Poll(UNER_Handle *handle);
UNER_Status UNER_Send(UNER_Handle *handle,
                      uint8_t transport_id,
                      uint8_t src,
                      uint8_t dst,
                      uint8_t cmd,
                      const uint8_t *payload,
                      uint8_t payload_len);
bool UNER_Dequeue(UNER_Handle *handle, UNER_Packet *out_packet);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_HANDLE_H_ */
