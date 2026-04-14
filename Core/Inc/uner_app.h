/*
 * uner_app.h - UNER v2 application wiring
 */
#ifndef INC_UNER_APP_H_
#define INC_UNER_APP_H_

#include <stdint.h>
#include "uner_core.h"

#ifdef __cplusplus
extern "C" {
#endif

void UNER_App_Init(void);
void UNER_App_Poll(void);
UNER_Status UNER_App_SendCommand(uint8_t cmd, const uint8_t *payload, uint8_t len);
uint8_t UNER_App_IsWaitingValidation(void);
uint8_t UNER_App_GetWaitingCommandId(void);

typedef struct __attribute__((packed))
{
    uint32_t screen_code;
    uint8_t source;
} ScreenReport_t;

#define UNER_CMD_ID_SET_MODE_AP                0x10u
#define UNER_CMD_ID_SET_MODE_STA               0x11u
#define UNER_CMD_ID_SET_CREDENTIALS            0x12u
#define UNER_CMD_ID_CLEAR_CREDENTIALS          0x13u
#define UNER_CMD_ID_START_SCAN                 0x14u
#define UNER_CMD_ID_GET_SCAN_RESULTS           0x15u
#define UNER_CMD_ID_REBOOT_ESP                 0x16u
#define UNER_CMD_ID_FACTORY_RESET              0x17u
#define UNER_CMD_ID_STOP_SCAN                  0x18u
#define UNER_CMD_ID_RESET_MCU                  0x19u
#define UNER_CMD_ID_GET_STATUS                 0x30u
#define UNER_CMD_ID_PING                       0x31u
#define UNER_CMD_ID_GET_PREFERENCES            0x40u
#define UNER_CMD_ID_REQUEST_FIRMWARE           0x41u
#define UNER_CMD_ID_ECHO                       0x42u
#define UNER_CMD_ID_SET_ENCODER_FAST           0x43u
#define UNER_CMD_ID_GET_CONNECTED_USERS        0x44u
#define UNER_CMD_ID_GET_USER_INFO              0x45u
#define UNER_CMD_ID_GET_INTERFACES_CONNECTED   0x46u
#define UNER_CMD_ID_GET_CREDENTIALS            0x47u
#define UNER_CMD_ID_CONNECT_WIFI               0x48u
#define UNER_CMD_ID_DISCONNECT_WIFI            0x49u
#define UNER_CMD_ID_SET_AP_CONFIG              0x4Au
#define UNER_CMD_ID_START_AP                   0x4Bu
#define UNER_CMD_ID_STOP_AP                    0x4Cu
#define UNER_CMD_ID_GET_CONNECTED_USERS_MODE   0x4Du
#define UNER_CMD_ID_SET_AUTO_RECONNECT         0x4Eu
#define UNER_CMD_ID_AUTH_VALIDATE_PIN          0x51u
#define UNER_CMD_ID_GET_CURRENT_SCREEN         0x52u
#define UNER_EVT_ID_SCREEN_CHANGED             0x95u

UNER_Status UNER_App_ReportScreenChanged(uint32_t screen_code, uint8_t source);

void UNER_App_OnUart1TxComplete(void);
void UNER_App_NotifyUart1Rx(void);
uint16_t UNER_App_Uart1RxGetWritePos(void);
void UNER_App_ResetUart1RxPos(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_APP_H_ */
