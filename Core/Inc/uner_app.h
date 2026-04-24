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
UNER_Status UNER_App_SendWifiCredentialRequest(const char *ssid);
UNER_Status UNER_App_RequestWifiNetworkDetail(const char *ssid);
UNER_Status UNER_App_SendEspReboot(uint8_t boot_mode);
uint8_t UNER_App_IsWaitingValidation(void);
uint8_t UNER_App_GetWaitingCommandId(void);

typedef struct __attribute__((packed))
{
    uint32_t screen_code;
} AuthPinGranted_t;

typedef struct __attribute__((packed))
{
    uint32_t screen_code;
    uint8_t result_code;
    uint8_t attempts_left;
} AuthRemoteResult_t;

typedef struct __attribute__((packed))
{
    uint8_t asset_id;
} BinaryAssetRequest_t;

/*
 * UNER_CMD_ID_BINARY_ASSET_STREAM
 *
 * STM -> ESP request:
 *   BinaryAssetRequest_t { asset_id }
 *
 * ESP -> STM response chunks:
 *   byte 0      status, 0 = OK
 *   byte 1      asset_id
 *   byte 2      width, pixels
 *   byte 3      height, pixels
 *   bytes 4..5  total_len, little-endian
 *   bytes 6..7  offset, little-endian
 *   byte 8      data_len
 *   bytes 9..   bitmap bytes, 1 bpp, row-packed, MSB first
 *
 * This keeps bulky assets like the repository QR outside STM flash. The ESP can
 * store them and stream them when the STM renders the screen that needs them.
 */
typedef struct __attribute__((packed))
{
    uint8_t status;
    uint8_t asset_id;
    uint8_t width;
    uint8_t height;
    uint16_t total_len;
    uint16_t offset;
    uint8_t data_len;
} BinaryAssetChunkHeader_t;

typedef struct
{
    uint8_t asset_id;
    uint8_t width;
    uint8_t height;
    uint16_t len;
    const uint8_t *data;
} BinaryAssetView_t;

typedef struct __attribute__((packed))
{
    uint32_t screen_code;
    uint8_t source;
} ScreenReport_t;

typedef struct __attribute__((packed))
{
    uint32_t screen_code;
    uint8_t selected_index;
    uint8_t item_count;
    uint8_t source;
} MenuSelectionReport_t;

#define UNER_SELECTION_INDEX_INVALID          0xFFu
#define UNER_SELECTION_PAYLOAD_SIZE           7u

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
#define UNER_CMD_ID_WIFI_GET_DETAIL            0x1Au
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
/* STM-originated PIN validation; external grants use AUTH_PIN_GRANTED. */
#define UNER_CMD_ID_AUTH_VALIDATE_PIN          0x51u
#define UNER_CMD_ID_GET_CURRENT_SCREEN         0x52u
#define UNER_CMD_ID_GET_CAR_MODE               0x5Bu
#define UNER_CMD_ID_MENU_ITEM_CLICK            0x53u
#define UNER_CMD_ID_TRIGGER_ENCODER_BUTTON     0x54u
#define UNER_CMD_ID_TRIGGER_USER_BUTTON        0x55u
#define UNER_CMD_ID_REQUEST_SCREEN_PAGE        0x56u
#define UNER_CMD_ID_TRIGGER_ENCODER_ROTATE_LEFT 0x57u
#define UNER_CMD_ID_TRIGGER_ENCODER_ROTATE_RIGHT 0x58u
#define UNER_CMD_ID_AUTH_PIN_GRANTED           0x59u
#define UNER_CMD_ID_BINARY_ASSET_STREAM        0x5Au
#define UNER_CMD_ID_WIFI_CREDENTIALS_WEB_RESULT 0x5Cu
#define UNER_CMD_ID_AUTH_REMOTE_RESULT         0x5Du

#define UNER_AUTH_REMOTE_RESULT_DENIED         0x00u
#define UNER_AUTH_REMOTE_RESULT_TIMEOUT        0x01u
#define UNER_AUTH_REMOTE_RESULT_BLOCKED        0x02u

#define UNER_WIFI_WEB_CREDENTIALS_STATUS_SUCCESS 0x00u
#define UNER_WIFI_WEB_CREDENTIALS_STATUS_FAILED  0x01u
#define UNER_WIFI_WEB_CREDENTIALS_STATUS_TIMEOUT 0x02u
#define UNER_WIFI_WEB_CREDENTIALS_STATUS_CANCELED 0x03u

#define UNER_ESP_REBOOT_BOOT_MODE_NORMAL       0x00u
#define UNER_ESP_REBOOT_BOOT_MODE_AP           0x01u

#define UNER_EVT_ID_SCREEN_CHANGED             0x95u
#define UNER_EVT_ID_MENU_SELECTION_CHANGED     0x96u
#define UNER_EVT_ID_CAR_MODE_CHANGED           0x97u

#define UNER_BINARY_ASSET_ID_GITHUB_QR         0x01u
#define UNER_BINARY_ASSET_CHUNK_HEADER_SIZE    9u
#define UNER_BINARY_ASSET_MAX_BYTES            1024u

UNER_Status UNER_App_ReportScreenChanged(uint32_t screen_code, uint8_t source);
UNER_Status UNER_App_ReportMenuSelectionChanged(uint32_t screen_code,
                                                uint8_t selected_index,
                                                uint8_t item_count,
                                                uint8_t source);
UNER_Status UNER_App_ReportCarModeChanged(uint8_t mode);
UNER_Status UNER_App_RequestBinaryAsset(uint8_t asset_id);
uint8_t UNER_App_GetBinaryAsset(uint8_t asset_id, BinaryAssetView_t *asset);

void UNER_App_OnUart1TxComplete(void);
void UNER_App_NotifyUart1Rx(void);
uint16_t UNER_App_Uart1RxGetWritePos(void);
void UNER_App_ResetUart1RxPos(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_APP_H_ */
