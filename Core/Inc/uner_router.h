/*
 * uner_router.h
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#ifndef INC_UNER_ROUTER_H_
#define INC_UNER_ROUTER_H_

#include "uner_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UNER_ROUTE_NONE 0xFFU

typedef void (*UNER_LocalHandler)(const UNER_Packet *packet, void *user_ctx);

typedef struct {
    uint8_t route_table[16];
    uint8_t this_node;
    UNER_LocalHandler on_local;
    void *user_ctx;
} UNER_Router;

void UNER_Router_Init(UNER_Router *router, uint8_t this_node, UNER_LocalHandler handler, void *user_ctx);
void UNER_Router_SetRoute(UNER_Router *router, uint8_t dst, uint8_t transport_id);
void UNER_Router_Process(UNER_Router *router, UNER_Handle *handle);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_ROUTER_H_ */
