/*
 * uner_router.h - UNER v2 router
 */
#ifndef INC_UNER_ROUTER_H_
#define INC_UNER_ROUTER_H_

#include <stdint.h>
#include "uner_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    UNER_Handle *handle;
    uint8_t this_node;
    uint8_t route_table[16];
    UNER_OnPacketCb local_cb;
    void *local_ctx;
} UNER_Router;

void UNER_Router_Init(
    UNER_Router *router,
    UNER_Handle *handle,
    uint8_t this_node,
    UNER_OnPacketCb local_cb,
    void *local_ctx);

void UNER_Router_SetRoute(UNER_Router *router, uint8_t dst, uint8_t transport_id);

void UNER_Router_OnPacket(void *ctx, const UNER_Packet *p);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_ROUTER_H_ */
