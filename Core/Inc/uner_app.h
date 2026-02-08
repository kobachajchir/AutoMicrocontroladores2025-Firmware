/*
 * uner_app.h - UNER v2 application wiring
 */
#ifndef INC_UNER_APP_H_
#define INC_UNER_APP_H_

#include <stdint.h>
#include "uner_core.h"
#include "types/bitmap_type.h"

#ifdef __cplusplus
extern "C" {
#endif

void UNER_App_Init(void);
void UNER_App_Poll(void);
UNER_Status UNER_App_SendCommand(uint8_t cmd, const uint8_t *payload, uint8_t len);
uint8_t UNER_App_CmdScheduler_TrySendNext(void);
uint8_t UNER_App_IsWaitingValidation(void);
uint8_t UNER_App_GetWaitingCommandId(void);
uint32_t UNER_App_GetPendingFlags(void);
uint32_t UNER_App_GetInFlightFlags(void);
void UNER_App_GetFlagBitmap(Byte_Flag_Struct *pending_low,
                            Byte_Flag_Struct *pending_high,
                            Byte_Flag_Struct *inflight_low,
                            Byte_Flag_Struct *inflight_high);

#define UNER_CMD_ID_START_SCAN         0x14u
#define UNER_CMD_ID_STOP_SCAN          0x18u
#define UNER_CMD_ID_REBOOT_ESP         0x16u
#define UNER_CMD_ID_GET_STATUS         0x30u
#define UNER_CMD_ID_PING               0x31u
#define UNER_CMD_ID_GET_CONNECTED_USERS 0x44u
#define UNER_CMD_ID_GET_USER_INFO      0x45u
#define UNER_CMD_ID_GET_INTERFACES_CONNECTED 0x46u
#define UNER_CMD_ID_GET_CREDENTIALS    0x47u
#define UNER_CMD_ID_CONNECT_WIFI       0x48u
#define UNER_CMD_ID_DISCONNECT_WIFI    0x49u
#define UNER_CMD_ID_SET_AP_CONFIG      0x4Au
#define UNER_CMD_ID_START_AP           0x4Bu
#define UNER_CMD_ID_STOP_AP            0x4Cu
#define UNER_CMD_ID_GET_CONNECTED_USERS_MODE 0x4Du
#define UNER_CMD_ID_SET_AUTO_RECONNECT 0x4Eu
#define UNER_CMD_ID_REQUEST_FIRMWARE   0x41u
#define UNER_CMD_ID_ECHO               0x42u
#define UNER_CMD_ID_SET_ENCODER_FAST   0x43u

void UNER_App_OnUart1TxComplete(void);
void UNER_App_NotifyUart1Rx(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_APP_H_ */
