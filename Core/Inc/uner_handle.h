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
    uint8_t cmd;
    uint8_t min_args;
    uint8_t max_args;
} UNER_CommandSpec;

typedef void (*UNER_ExecuteCommandFn)(void *ctx, const UNER_Packet *packet);

typedef struct {
    UNER_Core core;
    UNER_Transport *transports[UNER_MAX_TRANSPORTS];
    uint8_t transport_count;
    volatile uint8_t packet_pending;
    const UNER_CommandSpec *command_table;
    uint8_t command_count;
    UNER_ExecuteCommandFn execute_command;
    void *execute_ctx;
    UNER_OnPacketCb app_on_packet;
    void *app_on_packet_ctx;
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
UNER_Status UNER_Handle_RegisterCommands(
    UNER_Handle *handle,
    const UNER_CommandSpec *table,
    uint8_t count,
    UNER_ExecuteCommandFn execute_command,
    void *execute_ctx);
void UNER_Handle_Poll(UNER_Handle *handle);
void UNER_Handle_ProcessPending(UNER_Handle *handle);

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
