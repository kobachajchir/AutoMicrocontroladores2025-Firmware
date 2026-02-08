/*
 * uner_app.h - UNER v2 application wiring
 */
#ifndef INC_UNER_APP_H_
#define INC_UNER_APP_H_

#include <stdint.h>
#include <stdbool.h>
#include "uner_core.h"

#ifdef __cplusplus
extern "C" {
#endif

void UNER_App_Init(void);
void UNER_App_Poll(void);
UNER_Status UNER_App_SendCommand(uint8_t cmd, const uint8_t *payload, uint8_t len);
uint8_t UNER_App_IsWaitingValidation(void);
uint8_t UNER_App_GetWaitingCommandId(void);

#define UNER_CMD_ID_START_SCAN           0x14u
#define UNER_CMD_ID_GET_SCAN_RESULTS     0x15u
#define UNER_CMD_ID_REBOOT_ESP           0x16u
#define UNER_CMD_ID_STOP_SCAN            0x18u
#define UNER_CMD_ID_GET_STATUS           0x30u
#define UNER_CMD_ID_PING                 0x31u
#define UNER_CMD_ID_REQUEST_FIRMWARE     0x41u
#define UNER_CMD_ID_SET_ENCODER_FAST     0x43u
#define UNER_CMD_ID_BOOT_COMPLETE        0x4Fu
#define UNER_CMD_ID_NETWORK_IP           0x50u

#define UNER_EVT_ID_NETWORK_IP           0x93u
#define UNER_EVT_ID_BOOT_COMPLETE        0x94u

#define UNER_FLAG_CMD_PING               (1u << 0)
#define UNER_FLAG_CMD_REQUEST_FIRMWARE   (1u << 1)
#define UNER_FLAG_CMD_REBOOT_ESP         (1u << 2)
#define UNER_FLAG_CMD_START_SCAN         (1u << 3)
#define UNER_FLAG_CMD_GET_SCAN_RESULTS   (1u << 4)
#define UNER_FLAG_CMD_STOP_SCAN          (1u << 5)
#define UNER_FLAG_CMD_GET_STATUS         (1u << 6)

void UNER_CmdFlag_Set(uint32_t mask);
void UNER_CmdFlag_Clear(uint32_t mask);
uint32_t UNER_CmdFlag_GetPending(void);
bool UNER_CmdScheduler_TrySendNext(void);
void UNER_CmdScheduler_OnTxComplete(void);

void UNER_App_OnUart1TxComplete(void);
void UNER_App_NotifyUart1Rx(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_APP_H_ */
