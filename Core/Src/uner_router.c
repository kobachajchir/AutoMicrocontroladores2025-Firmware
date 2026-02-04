/*
 * uner_router.c - UNER v2 router
 */
#include "uner_router.h"
#include <string.h>

void UNER_Router_Init(
    UNER_Router *router,
    UNER_Handle *handle,
    uint8_t this_node,
    UNER_OnPacketCb local_cb,
    void *local_ctx)
{
    if (!router) {
        return;
    }

    router->handle = handle;
    router->this_node = this_node;
    router->local_cb = local_cb;
    router->local_ctx = local_ctx;
    for (uint8_t i = 0; i < 16u; ++i) {
        router->route_table[i] = 0xFFu;
    }
}

void UNER_Router_SetRoute(UNER_Router *router, uint8_t dst, uint8_t transport_id)
{
    if (!router || dst > 0x0Fu) {
        return;
    }
    router->route_table[dst] = transport_id;
}

void UNER_Router_OnPacket(void *ctx, const UNER_Packet *p)
{
    UNER_Router *router = (UNER_Router *)ctx;
    if (!router || !p) {
        return;
    }

    if (p->dst == router->this_node || p->dst == UNER_NODE_BROADCAST) {
        if (router->local_cb) {
            router->local_cb(router->local_ctx, p);
        }
        if (p->dst == UNER_NODE_BROADCAST) {
            return;
        }
    }

    if (!router->handle) {
        return;
    }

    uint8_t dst = p->dst & 0x0Fu;
    uint8_t transport_id = router->route_table[dst];
    if (transport_id == 0xFFu) {
        return;
    }

    UNER_Send(
        router->handle,
        transport_id,
        p->src,
        p->dst,
        p->cmd,
        p->payload,
        p->len);
}
