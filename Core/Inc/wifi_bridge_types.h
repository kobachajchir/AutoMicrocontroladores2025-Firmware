/*
 * wifi_bridge_types.h - common API types shared by WifiBridge backends.
 */
#ifndef INC_WIFI_BRIDGE_TYPES_H_
#define INC_WIFI_BRIDGE_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"
#include "wifi_bridge_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_BRIDGE_OK = 0,
    WIFI_BRIDGE_ERROR,
    WIFI_BRIDGE_BUSY,
    WIFI_BRIDGE_QUEUE_FULL,
    WIFI_BRIDGE_BAD_ARG,
    WIFI_BRIDGE_TIMEOUT,
    WIFI_BRIDGE_UNSUPPORTED,
} WifiBridge_Result;

typedef enum {
    WIFI_BRIDGE_EVT_NONE = 0,
    WIFI_BRIDGE_EVT_BOOT,
    WIFI_BRIDGE_EVT_MODE_CHANGED,
    WIFI_BRIDGE_EVT_STA_CONNECTED,
    WIFI_BRIDGE_EVT_STA_DISCONNECTED,
    WIFI_BRIDGE_EVT_GOT_IP,
    WIFI_BRIDGE_EVT_SCAN_DONE,
    WIFI_BRIDGE_EVT_AP_STARTED,
    WIFI_BRIDGE_EVT_AP_STOPPED,
    WIFI_BRIDGE_EVT_ERROR
} WifiBridge_EventId;

typedef struct {
    WifiBridge_EventId id;
    uint8_t mode;
    uint8_t connected;
    uint8_t got_ip;
    uint8_t ip[WIFI_BRIDGE_IP_LEN];
    char ssid[WIFI_BRIDGE_SSID_MAX_LEN + 1u];
    uint8_t status;
} WifiBridge_Event;

typedef union {
    uint8_t raw;
    struct {
        uint8_t busy       : 1;
        uint8_t tx_busy    : 1;
        uint8_t rx_pending : 1;
        uint8_t got_ok     : 1;
        uint8_t got_error  : 1;
        uint8_t got_ip     : 1;
        uint8_t connected  : 1;
        uint8_t reserved   : 1;
    } bits;
} WifiBridge_Flags;

#define WIFI_FLAG_BUSY        bits.busy
#define WIFI_FLAG_TX_BUSY     bits.tx_busy
#define WIFI_FLAG_RX_PENDING  bits.rx_pending
#define WIFI_FLAG_GOT_OK      bits.got_ok
#define WIFI_FLAG_GOT_ERROR   bits.got_error
#define WIFI_FLAG_GOT_IP      bits.got_ip
#define WIFI_FLAG_CONNECTED   bits.connected

#define WIFI_FLAG_SET(h, flag)      ((h)->flags.flag = 1u)
#define WIFI_FLAG_CLEAR(h, flag)    ((h)->flags.flag = 0u)
#define WIFI_FLAG_IS_SET(h, flag)   ((h)->flags.flag != 0u)

typedef void (*WifiBridge_EventCallback)(const WifiBridge_Event *evt);

typedef struct {
    uint8_t asset_id;
    uint8_t width;
    uint8_t height;
    uint16_t len;
    const uint8_t *data;
} WifiBridge_BinaryAssetView;

typedef union {
    uint8_t raw;
    struct {
        uint8_t ssid_set     : 1;
        uint8_t password_set : 1;
        uint8_t reserved     : 6;
    } bits;
} WifiBridge_CredentialFlags;

typedef struct {
    WifiBridge_CredentialFlags flags;
    char ssid[WIFI_BRIDGE_SSID_MAX_LEN + 1u];
    char password[WIFI_BRIDGE_PASS_MAX_LEN + 1u];
} WifiBridge_Credentials;

typedef enum {
    WIFI_BRIDGE_MODE_UNKNOWN = 0u,
    WIFI_BRIDGE_MODE_STA     = 1u,
    WIFI_BRIDGE_MODE_AP      = 2u,
    WIFI_BRIDGE_MODE_AP_STA  = 3u,
} WifiBridge_Mode;

typedef enum {
    WIFI_BRIDGE_AT_OP_NONE = 0u,
    WIFI_BRIDGE_AT_OP_SET_STA,
    WIFI_BRIDGE_AT_OP_SET_AP,
    WIFI_BRIDGE_AT_OP_CONNECT,
    WIFI_BRIDGE_AT_OP_DISCONNECT,
    WIFI_BRIDGE_AT_OP_SCAN,
    WIFI_BRIDGE_AT_OP_STATUS,
} WifiBridge_AT_Operation;

typedef struct WifiBridge_Handle {
    UART_HandleTypeDef *huart;
    WifiBridge_Flags flags;
    WifiBridge_EventCallback on_event;
    WifiBridge_Event last_event;
    uint8_t mode;
    uint8_t status;
    uint8_t ip[WIFI_BRIDGE_IP_LEN];
    char ssid[WIFI_BRIDGE_SSID_MAX_LEN + 1u];

#if defined(WIFI_BRIDGE_USE_ESP_AT)
    uint8_t at_rx_dma[WIFI_BRIDGE_AT_RX_DMA_SIZE];
    uint16_t at_rx_last_pos;
    char at_line[WIFI_BRIDGE_AT_LINE_SIZE];
    uint16_t at_line_len;
    char at_tx_buf[WIFI_BRIDGE_AT_TX_SIZE];
    uint16_t at_tx_len;
    uint32_t at_deadline_ms;
    WifiBridge_AT_Operation at_active_op;
    uint8_t at_scan_count;
#endif
} WifiBridge_Handle;

#define WIFI_BRIDGE_CMD_ID_SET_MODE_AP                 0x10u
#define WIFI_BRIDGE_CMD_ID_SET_MODE_STA                0x11u
#define WIFI_BRIDGE_CMD_ID_SET_CREDENTIALS             0x12u
#define WIFI_BRIDGE_CMD_ID_CLEAR_CREDENTIALS           0x13u
#define WIFI_BRIDGE_CMD_ID_START_SCAN                  0x14u
#define WIFI_BRIDGE_CMD_ID_GET_SCAN_RESULTS            0x15u
#define WIFI_BRIDGE_CMD_ID_REBOOT_ESP                  0x16u
#define WIFI_BRIDGE_CMD_ID_FACTORY_RESET               0x17u
#define WIFI_BRIDGE_CMD_ID_STOP_SCAN                   0x18u
#define WIFI_BRIDGE_CMD_ID_RESET_MCU                   0x19u
#define WIFI_BRIDGE_CMD_ID_WIFI_GET_DETAIL             0x1Au
#define WIFI_BRIDGE_CMD_ID_GET_STATUS                  0x30u
#define WIFI_BRIDGE_CMD_ID_PING                        0x31u
#define WIFI_BRIDGE_CMD_ID_GET_PREFERENCES             0x40u
#define WIFI_BRIDGE_CMD_ID_REQUEST_FIRMWARE            0x41u
#define WIFI_BRIDGE_CMD_ID_ECHO                        0x42u
#define WIFI_BRIDGE_CMD_ID_SET_ENCODER_FAST            0x43u
#define WIFI_BRIDGE_CMD_ID_GET_CONNECTED_USERS         0x44u
#define WIFI_BRIDGE_CMD_ID_GET_USER_INFO               0x45u
#define WIFI_BRIDGE_CMD_ID_GET_INTERFACES_CONNECTED    0x46u
#define WIFI_BRIDGE_CMD_ID_GET_CREDENTIALS             0x47u
#define WIFI_BRIDGE_CMD_ID_CONNECT_WIFI                0x48u
#define WIFI_BRIDGE_CMD_ID_DISCONNECT_WIFI             0x49u
#define WIFI_BRIDGE_CMD_ID_SET_AP_CONFIG               0x4Au
#define WIFI_BRIDGE_CMD_ID_START_AP                    0x4Bu
#define WIFI_BRIDGE_CMD_ID_STOP_AP                     0x4Cu
#define WIFI_BRIDGE_CMD_ID_GET_CONNECTED_USERS_MODE    0x4Du
#define WIFI_BRIDGE_CMD_ID_SET_AUTO_RECONNECT          0x4Eu
#define WIFI_BRIDGE_CMD_ID_AUTH_VALIDATE_PIN           0x51u
#define WIFI_BRIDGE_CMD_ID_GET_CURRENT_SCREEN          0x52u
#define WIFI_BRIDGE_CMD_ID_MENU_ITEM_CLICK             0x53u
#define WIFI_BRIDGE_CMD_ID_TRIGGER_ENCODER_BUTTON      0x54u
#define WIFI_BRIDGE_CMD_ID_TRIGGER_USER_BUTTON         0x55u
#define WIFI_BRIDGE_CMD_ID_REQUEST_SCREEN_PAGE         0x56u
#define WIFI_BRIDGE_CMD_ID_TRIGGER_ENCODER_ROTATE_LEFT 0x57u
#define WIFI_BRIDGE_CMD_ID_TRIGGER_ENCODER_ROTATE_RIGHT 0x58u
#define WIFI_BRIDGE_CMD_ID_AUTH_PIN_GRANTED            0x59u
#define WIFI_BRIDGE_CMD_ID_BINARY_ASSET_STREAM         0x5Au
#define WIFI_BRIDGE_CMD_ID_GET_CAR_MODE                0x5Bu
#define WIFI_BRIDGE_CMD_ID_WIFI_CREDENTIALS_WEB_RESULT 0x5Cu
#define WIFI_BRIDGE_CMD_ID_AUTH_REMOTE_RESULT          0x5Du

#define WIFI_BRIDGE_EVT_ID_SCREEN_CHANGED              0x95u
#define WIFI_BRIDGE_EVT_ID_MENU_SELECTION_CHANGED      0x96u
#define WIFI_BRIDGE_EVT_ID_CAR_MODE_CHANGED            0x97u

#define WIFI_BRIDGE_SELECTION_INDEX_INVALID            0xFFu
#define WIFI_BRIDGE_SELECTION_PAYLOAD_SIZE             7u

#define WIFI_BRIDGE_AUTH_REMOTE_RESULT_DENIED          0x00u
#define WIFI_BRIDGE_AUTH_REMOTE_RESULT_TIMEOUT         0x01u
#define WIFI_BRIDGE_AUTH_REMOTE_RESULT_BLOCKED         0x02u

#define WIFI_BRIDGE_WIFI_WEB_CREDENTIALS_STATUS_SUCCESS  0x00u
#define WIFI_BRIDGE_WIFI_WEB_CREDENTIALS_STATUS_FAILED   0x01u
#define WIFI_BRIDGE_WIFI_WEB_CREDENTIALS_STATUS_TIMEOUT  0x02u
#define WIFI_BRIDGE_WIFI_WEB_CREDENTIALS_STATUS_CANCELED 0x03u

#define WIFI_BRIDGE_ESP_REBOOT_BOOT_MODE_NORMAL        0x00u
#define WIFI_BRIDGE_ESP_REBOOT_BOOT_MODE_AP            0x01u

#define WIFI_BRIDGE_BINARY_ASSET_ID_GITHUB_QR          0x01u
#define WIFI_BRIDGE_BINARY_ASSET_MAX_BYTES             1024u

#ifdef __cplusplus
}
#endif

#endif /* INC_WIFI_BRIDGE_TYPES_H_ */
