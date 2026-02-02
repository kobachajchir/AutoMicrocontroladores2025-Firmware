/*
 * uner_router.c
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#include "uner_router.h"

void UNER_Router_Init(UNER_Router *router, uint8_t this_node, UNER_LocalHandler handler, void *user_ctx)
{
    if (!router) {
        return;
    }
    router->this_node = this_node;
    router->on_local = handler;
    router->user_ctx = user_ctx;
    for (uint8_t i = 0; i < 16U; ++i) {
        router->route_table[i] = UNER_ROUTE_NONE;
    }
}

void UNER_Router_SetRoute(UNER_Router *router, uint8_t dst, uint8_t transport_id)
{
    if (!router || dst > 0x0FU) {
        return;
    }
    router->route_table[dst] = transport_id;
}

void UNER_Router_Process(UNER_Router *router, UNER_Handle *handle)
{
    if (!router || !handle) {
        return;
    }
    UNER_Packet packet;
    while (UNER_Dequeue(handle, &packet)) {
        if (packet.dst == router->this_node || packet.dst == UNER_NODE_BROADCAST) {
            if (router->on_local) {
                router->on_local(&packet, router->user_ctx);
            }
        } else {
            uint8_t transport_id = router->route_table[packet.dst & 0x0FU];
            if (transport_id != UNER_ROUTE_NONE) {
                UNER_Send(handle,
                          transport_id,
                          packet.src,
                          packet.dst,
                          packet.cmd,
                          packet.payload,
                          packet.payload_len);
            }
        }
    }
}
