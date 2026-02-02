/*
 * uner_handle.h - UNER v2 manager
 */
#ifndef INC_UNER_HANDLE_H_
#define INC_UNER_HANDLE_H_

#include <stdint.h>
#include "uner_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UNER_MAX_TRANSPORTS 4u

typedef struct {
    UNER_Core core;
    UNER_Transport *transports[UNER_MAX_TRANSPORTS];
    uint8_t transport_count;
    uint8_t tx_buf[UNER_OVERHEAD + 255u];
} UNER_Handle;

UNER_Status UNER_Handle_Init(
    UNER_Handle *handle,
    const UNER_CoreConfig *cfg,
    UNER_Packet *slots,
    uint8_t slot_count,
    uint8_t *payload_pool_bytes,
    uint16_t payload_pool_size);

UNER_Status UNER_Handle_RegisterTransport(UNER_Handle *handle, UNER_Transport *transport);
void UNER_Handle_Poll(UNER_Handle *handle);

UNER_Status UNER_Send(
    UNER_Handle *handle,
    uint8_t transport_id,
    uint8_t src,
    uint8_t dst,
    uint8_t cmd,
    const uint8_t *payload,
    uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_HANDLE_H_ */
