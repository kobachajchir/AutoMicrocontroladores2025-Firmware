/*
 * uner_app.c - UNER v2 application wiring
 */
#include "uner_app.h"
#include "globals.h"
#include "uner_core.h"
#include "uner_handle.h"
#include "uner_transport_uart1_dma.h"
#include "uner_v2.h"
#include "oled_utils.h"
#include "permissions.h"
#include "screenWrappers.h"
#include "stm32f1xx_hal.h"
#include <string.h>

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;

#define UNER_QUEUE_SLOTS 6u
#define WIFI_ACTIVE_CODE 1
#define AP_ACTIVE_CODE 2
#define WIFI_SCAN_RESULTS_POLL_RUNNING_MS 750u
#define WIFI_SCAN_RESULTS_POLL_PENDING_MS 100u
#define WIFI_SCAN_RESULTS_POLL_RETRY_MS 250u

static UNER_Handle uner_handle;
static UNER_TransportUart1Dma uner_uart1;
static UNER_Packet uner_slots[UNER_QUEUE_SLOTS];
static volatile uint8_t uner_payload_pool[UNER_QUEUE_SLOTS * 255u];
static volatile uint8_t uner_waiting_validation = 0;
static volatile uint8_t uner_wait_cmd_id = 0;
static volatile uint32_t uner_wait_started_at = 0u;
static volatile uint16_t uner_wait_timeout_ms = 0u;

static uint8_t binary_asset_buffer[UNER_BINARY_ASSET_MAX_BYTES];
static uint8_t binary_asset_id = 0u;
static uint8_t binary_asset_width = 0u;
static uint8_t binary_asset_height = 0u;
static uint16_t binary_asset_total_len = 0u;
static uint16_t binary_asset_received_len = 0u;
static uint8_t binary_asset_ready = 0u;
static uint8_t binary_asset_in_progress = 0u;
static uint32_t wifi_scan_results_next_poll_at = 0u;

typedef enum {
    UNER_CMD_STATUS_OK = 0u,
    UNER_CMD_STATUS_BAD_PAYLOAD = 1u,
    UNER_CMD_STATUS_SCREEN_MISMATCH = 2u,
    UNER_CMD_STATUS_PIN_NOT_REQUIRED = 3u,
    UNER_CMD_STATUS_NO_PENDING_PERMISSION = 4u,
    UNER_CMD_STATUS_BAD_ARGUMENT = 5u,
} UNER_CommandStatus;

typedef enum {
    WIFI_MODE_OFF    = 0x00u, // WIFI_OFF
    WIFI_MODE_STA    = 0x01u, // WIFI_STA
    WIFI_MODE_AP     = 0x02u, // WIFI_AP
    WIFI_MODE_AP_STA = 0x03u  // WIFI_AP_STA
} UNER_WifiModeEsp;

static void UNER_App_UpdateModeFlags(uint8_t mode);
static void UNER_App_ParseFirmwareResponse(const UNER_Packet *p);
static void UNER_App_ParseStatusResponse(const UNER_Packet *p);
static void UNER_App_ParseWifiFinalizer(const UNER_Packet *p);
static void UNER_App_ParseScanResultsResponse(const UNER_Packet *p);
static void UNER_App_ParseAuthValidatePinResponse(const UNER_Packet *p);
static void UNER_App_ShowScanResultsScreen(void);
static void UNER_App_ServiceWifiScanResults(void);
static void UNER_App_StoreWifiInfoSsid(const uint8_t *payload, uint8_t len);
static void UNER_App_StoreTargetCredentialsSsid(const uint8_t *payload, uint8_t len);
static void UNER_App_StoreNetworkIpPayload(const uint8_t *payload, uint8_t len);
static void UNER_App_SetWifiNetworkEncryptionText(uint8_t index, uint8_t encryption_type);
static int8_t UNER_App_FindWifiNetworkIndexBySsid(const uint8_t *ssid, uint8_t len);
static uint8_t UNER_App_IsStaActive(void);
static void UNER_App_MarkStaActive(void);
static void UNER_App_ShowWifiConnectedNotification(uint8_t was_wifi_active);
static void UNER_App_RequestNetworkUiRefresh(void);
static void UNER_App_WriteScreenReportPayload(uint8_t *payload, uint32_t screen_code, uint8_t source);
static void UNER_App_WriteCarModePayload(uint8_t *payload, uint8_t mode);
static uint32_t UNER_App_ReadU32Le(const uint8_t *payload);
static void UNER_App_SendCommandStatus(const UNER_Packet *packet, uint8_t status);
static uint8_t UNER_App_PacketScreenMatchesCurrent(const UNER_Packet *packet, uint32_t *screen_code);
static int8_t UNER_App_FindVisibleMenuItemIndex(const SubMenu *menu, uint8_t visible_item);
static uint8_t UNER_App_DispatchUserEvent(UserEvent_t event);
static uint8_t screen_is_permission_pin_flow(uint32_t screen_code);
static uint16_t UNER_App_ReadU16Le(const uint8_t *payload);
static void UNER_App_ResetBinaryAssetState(uint8_t asset_id);
static uint8_t UNER_App_ParseBinaryAssetChunk(const UNER_Packet *packet);
static void UNER_App_ClearWaitingValidation(void);
static void UNER_App_ShowNotification(RenderFunction renderFn, uint16_t timeout_ms);
static void UNER_App_ShowSimpleNotification(OledSimpleNotificationId notificationId, uint16_t timeout_ms);
static void UNER_App_UpdateEncoderFastScroll(const UNER_Packet *packet);

typedef enum {
    UNER_CMD_SET_MODE_AP = 0x10u,
    UNER_CMD_SET_MODE_STA = 0x11u,
    UNER_CMD_SET_CREDENTIALS = 0x12u,
    UNER_CMD_CLEAR_CREDENTIALS = 0x13u,
    UNER_CMD_START_SCAN = 0x14u,
    UNER_CMD_GET_SCAN_RESULTS = 0x15u,
    UNER_CMD_REBOOT_ESP = 0x16u,
    UNER_CMD_FACTORY_RESET = 0x17u,
    UNER_CMD_STOP_SCAN = 0x18u,
    UNER_CMD_RESET_MCU = 0x19u,
    UNER_CMD_WIFI_GET_DETAIL = 0x1Au,

    UNER_CMD_GET_STATUS = 0x30u,
    UNER_CMD_PING = 0x31u,

    UNER_CMD_GET_PREFERENCES = 0x40u,
    UNER_CMD_REQUEST_FIRMWARE = 0x41u,
    UNER_CMD_ECHO = 0x42u,
    UNER_CMD_SET_ENCODER_FAST = 0x43u,
    UNER_CMD_GET_CONNECTED_USERS = 0x44u,
    UNER_CMD_GET_USER_INFO = 0x45u,
    UNER_CMD_GET_INTERFACES_CONNECTED = 0x46u,
    UNER_CMD_GET_CREDENTIALS = 0x47u,
    UNER_CMD_CONNECT_WIFI = 0x48u,
    UNER_CMD_DISCONNECT_WIFI = 0x49u,
    UNER_CMD_SET_AP_CONFIG = 0x4Au,
    UNER_CMD_START_AP = 0x4Bu,
    UNER_CMD_STOP_AP = 0x4Cu,
    UNER_CMD_GET_CONNECTED_USERS_MODE = 0x4Du,
    UNER_CMD_SET_AUTO_RECONNECT = 0x4Eu,
    UNER_CMD_BOOT_COMPLETE = 0x4Fu,
    UNER_CMD_NETWORK_IP = 0x50u,
    UNER_CMD_AUTH_VALIDATE_PIN = 0x51u,
    UNER_CMD_GET_CURRENT_SCREEN = 0x52u,
    UNER_CMD_GET_CAR_MODE = 0x5Bu,
    UNER_CMD_MENU_ITEM_CLICK = 0x53u,
    UNER_CMD_TRIGGER_ENCODER_BUTTON = 0x54u,
    UNER_CMD_TRIGGER_USER_BUTTON = 0x55u,
    UNER_CMD_REQUEST_SCREEN_PAGE = 0x56u,
    UNER_CMD_TRIGGER_ENCODER_ROTATE_LEFT = 0x57u,
    UNER_CMD_TRIGGER_ENCODER_ROTATE_RIGHT = 0x58u,
    UNER_CMD_AUTH_PIN_GRANTED = 0x59u,
    UNER_CMD_BINARY_ASSET_STREAM = 0x5Au,
    UNER_CMD_WIFI_CREDENTIALS_WEB_RESULT = 0x5Cu,
    UNER_CMD_AUTH_REMOTE_RESULT = 0x5Du,

    UNER_CMD_ACK = 0xE0u,
    UNER_CMD_NACK = 0xE1u,

    UNER_EVT_BOOT = 0x80u,
    UNER_EVT_MODE_CHANGED = 0x81u,
    UNER_EVT_STA_CONNECTED = 0x82u,
    UNER_EVT_STA_DISCONNECTED = 0x83u,
    UNER_EVT_AP_CLIENT_JOIN = 0x84u,
    UNER_EVT_AP_CLIENT_LEAVE = 0x85u,
    UNER_EVT_APP_USER_CONNECTED = 0x86u,
    UNER_EVT_APP_USER_DISCONNECTED = 0x87u,
    UNER_EVT_WEBSERVER_UP = 0x88u,
    UNER_EVT_WEBSERVER_CLIENT_CONNECTED = 0x89u,
    UNER_EVT_WEBSERVER_CLIENT_DISCONNECTED = 0x8Au,
    UNER_EVT_LASTWIFI_NOTFOUND = 0x8Bu,
    UNER_EVT_CONTROLLER_CONNECTED = 0x8Cu,
    UNER_EVT_CONTROLLER_DISCONNECTED = 0x8Du,
    UNER_EVT_USB_CONNECTED = 0x8Eu,
    UNER_EVT_USB_DISCONNECTED = 0x8Fu,
    UNER_EVT_APP_GET_MPU_READINGS = 0x90u,
    UNER_EVT_APP_GET_TCRT_READINGS = 0x91u,
    UNER_EVT_WIFI_CONNECTED = 0x92u,
    UNER_EVT_NETWORK_IP = 0x93u,
    UNER_EVT_BOOT_COMPLETE = 0x94u,
    UNER_EVT_SCREEN_CHANGED = 0x95u,
    UNER_EVT_MENU_SELECTION_CHANGED = 0x96u,
} UNER_CommandId;

static void evt_boot_handler(void *ctx, const UNER_Packet *p);
static void evt_mode_changed_handler(void *ctx, const UNER_Packet *p);
static void evt_sta_connected_handler(void *ctx, const UNER_Packet *p);
static void evt_sta_disconnected_handler(void *ctx, const UNER_Packet *p);
static void evt_ap_client_join_handler(void *ctx, const UNER_Packet *p);
static void evt_ap_client_leave_handler(void *ctx, const UNER_Packet *p);
static void evt_webserver_up_handler(void *ctx, const UNER_Packet *p);
static void evt_webserver_client_conn_handler(void *ctx, const UNER_Packet *p);
static void evt_webserver_client_disconn_handler(void *ctx, const UNER_Packet *p);
static void evt_lastwifi_notfound_handler(void *ctx, const UNER_Packet *p);
static void evt_app_mpu_readings_handler(void *ctx, const UNER_Packet *p);
static void evt_app_tcrt_readings_handler(void *ctx, const UNER_Packet *p);
static void evt_app_user_connected_handler(void *ctx, const UNER_Packet *p);
static void evt_app_user_disconnected_handler(void *ctx, const UNER_Packet *p);
static void evt_controller_connected_handler(void *ctx, const UNER_Packet *p);
static void evt_controller_disconnected_handler(void *ctx, const UNER_Packet *p);
static void evt_usb_connected_handler(void *ctx, const UNER_Packet *p);
static void evt_usb_disconnected_handler(void *ctx, const UNER_Packet *p);
static void evt_wifi_connected_handler(void *ctx, const UNER_Packet *p);
static void evt_network_ip_handler(void *ctx, const UNER_Packet *p);
static void evt_boot_complete_handler(void *ctx, const UNER_Packet *p);
static void cmd_reset_mcu_handler(void *ctx, const UNER_Packet *p);
static void cmd_network_ip_handler(void *ctx, const UNER_Packet *p);
static void cmd_boot_complete_handler(void *ctx, const UNER_Packet *p);
static void cmd_get_current_screen_handler(void *ctx, const UNER_Packet *p);
static void cmd_request_firmware_handler(void *ctx, const UNER_Packet *packet);
static void cmd_ack_handler(void *ctx, const UNER_Packet *packet);
static void cmd_request_screen_page_handler(void *ctx, const UNER_Packet *packet);
static void cmd_menu_item_click_handler(void *ctx, const UNER_Packet *packet);
static void cmd_trigger_encoder_button_handler(void *ctx, const UNER_Packet *packet);
static void cmd_trigger_user_button_handler(void *ctx, const UNER_Packet *packet);
static void cmd_trigger_encoder_rotate_left_handler(void *ctx, const UNER_Packet *packet);
static void cmd_trigger_encoder_rotate_right_handler(void *ctx, const UNER_Packet *packet);
static void cmd_auth_pin_granted_handler(void *ctx, const UNER_Packet *packet);
static void cmd_auth_remote_result_handler(void *ctx, const UNER_Packet *packet);
static void cmd_get_car_mode_handler(void *ctx, const UNER_Packet *packet);
static void cmd_binary_asset_stream_handler(void *ctx, const UNER_Packet *packet);
static void cmd_wifi_credentials_web_result_handler(void *ctx, const UNER_Packet *packet);
static void cmd_wifi_get_detail_handler(void *ctx, const UNER_Packet *packet);

static const UNER_CommandSpec uner_commands[] = {
    { UNER_CMD_SET_MODE_AP, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_SET_MODE_STA, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 300u, NULL },
    { UNER_CMD_SET_CREDENTIALS, 0u, 255u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_CLEAR_CREDENTIALS, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_START_SCAN, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, NULL },
    { UNER_CMD_GET_SCAN_RESULTS, 0u, 0u, UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_REBOOT_ESP, 1u, 1u, UNER_SPEC_F_ACK, 50u, NULL },
    { UNER_CMD_FACTORY_RESET, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, NULL },
    { UNER_CMD_STOP_SCAN, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_RESET_MCU, 0u, 0u, 0u, 0u, cmd_reset_mcu_handler },
    { UNER_CMD_WIFI_GET_DETAIL, 1u, (WIFI_SSID_MAX_LEN + 1u), UNER_SPEC_F_RESP, 150u, cmd_wifi_get_detail_handler },
    { UNER_CMD_GET_STATUS, 0u, 0u, UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_PING, 0u, 0u, UNER_SPEC_F_RESP, 50u, NULL },
    { UNER_CMD_GET_PREFERENCES, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_REQUEST_FIRMWARE, 0u, 32u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, cmd_request_firmware_handler },
    { UNER_CMD_SET_ENCODER_FAST, 1u, 1u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_GET_CONNECTED_USERS, 0u, 0u, UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_GET_USER_INFO, 1u, 1u, UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_GET_INTERFACES_CONNECTED, 0u, 0u, UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_GET_CREDENTIALS, 0u, 0u, UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_CONNECT_WIFI, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 300u, NULL },
    { UNER_CMD_DISCONNECT_WIFI, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_SET_AP_CONFIG, 2u, 96u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_START_AP, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 300u, NULL },
    { UNER_CMD_STOP_AP, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_GET_CONNECTED_USERS_MODE, 0u, 0u, UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_SET_AUTO_RECONNECT, 1u, 1u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_BOOT_COMPLETE, 5u, 5u, UNER_SPEC_F_EVT, 0u, cmd_boot_complete_handler },
    { UNER_CMD_NETWORK_IP, 5u, 5u, UNER_SPEC_F_EVT, 0u, cmd_network_ip_handler },
    /* STM-originated PIN validation command; host-originated grants use 0x59. */
    { UNER_CMD_AUTH_VALIDATE_PIN, 6u, 6u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 500u, NULL },
	{ UNER_CMD_AUTH_PIN_GRANTED, 4u, 4u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, cmd_auth_pin_granted_handler },
    { UNER_CMD_AUTH_REMOTE_RESULT, 6u, 6u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, cmd_auth_remote_result_handler },
    { UNER_CMD_GET_CURRENT_SCREEN, 0u, 0u, UNER_SPEC_F_RESP, 50u, cmd_get_current_screen_handler },
    { UNER_CMD_GET_CAR_MODE, 0u, 0u, UNER_SPEC_F_RESP, 50u, cmd_get_car_mode_handler },
	{ UNER_CMD_REQUEST_SCREEN_PAGE, 5u, 5u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, cmd_request_screen_page_handler },
	{ UNER_CMD_MENU_ITEM_CLICK, 5u, 5u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, cmd_menu_item_click_handler },
	{ UNER_CMD_TRIGGER_ENCODER_BUTTON, 5u, 5u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, cmd_trigger_encoder_button_handler },
	{ UNER_CMD_TRIGGER_USER_BUTTON, 5u, 5u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, cmd_trigger_user_button_handler },
	{ UNER_CMD_TRIGGER_ENCODER_ROTATE_LEFT, 4u, 4u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, cmd_trigger_encoder_rotate_left_handler },
	{ UNER_CMD_TRIGGER_ENCODER_ROTATE_RIGHT, 4u, 4u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, cmd_trigger_encoder_rotate_right_handler },
    { UNER_CMD_BINARY_ASSET_STREAM, 1u, 255u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 1000u, cmd_binary_asset_stream_handler },
    { UNER_CMD_WIFI_CREDENTIALS_WEB_RESULT, 2u, 38u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, cmd_wifi_credentials_web_result_handler },
    { UNER_CMD_ACK, 1u, 2u, 0u, 0u, cmd_ack_handler },
    { UNER_CMD_NACK, 1u, 2u, 0u, 0u, NULL },
    { UNER_EVT_BOOT, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_boot_handler },
    { UNER_EVT_MODE_CHANGED, 0u, 255u, UNER_SPEC_F_EVT | UNER_SPEC_F_ACK, 50u, evt_mode_changed_handler },
    { UNER_EVT_STA_CONNECTED, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_sta_connected_handler },
    { UNER_EVT_STA_DISCONNECTED, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_sta_disconnected_handler },
    { UNER_EVT_AP_CLIENT_JOIN, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_ap_client_join_handler },
    { UNER_EVT_AP_CLIENT_LEAVE, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_ap_client_leave_handler },
    { UNER_EVT_APP_USER_CONNECTED, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_app_user_connected_handler },
    { UNER_EVT_APP_USER_DISCONNECTED, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_app_user_disconnected_handler },
    { UNER_EVT_CONTROLLER_CONNECTED, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_controller_connected_handler },
    { UNER_EVT_CONTROLLER_DISCONNECTED, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_controller_disconnected_handler },
    { UNER_EVT_USB_CONNECTED, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_usb_connected_handler },
    { UNER_EVT_USB_DISCONNECTED, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_usb_disconnected_handler },
    { UNER_EVT_WEBSERVER_UP, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_webserver_up_handler },
    { UNER_EVT_WEBSERVER_CLIENT_CONNECTED, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_webserver_client_conn_handler },
    { UNER_EVT_WEBSERVER_CLIENT_DISCONNECTED, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_webserver_client_disconn_handler },
    { UNER_EVT_LASTWIFI_NOTFOUND, 0u, 255u, UNER_SPEC_F_EVT | UNER_SPEC_F_ACK, 50u, evt_lastwifi_notfound_handler },
    { UNER_EVT_APP_GET_MPU_READINGS, 0u, 255u, UNER_SPEC_F_EVT | UNER_SPEC_F_ACK, 50u, evt_app_mpu_readings_handler },
    { UNER_EVT_APP_GET_TCRT_READINGS, 0u, 255u, UNER_SPEC_F_EVT | UNER_SPEC_F_ACK, 50u, evt_app_tcrt_readings_handler },
    { UNER_EVT_WIFI_CONNECTED, 0u, 255u, UNER_SPEC_F_EVT, 0u, evt_wifi_connected_handler },
    { UNER_EVT_NETWORK_IP, 5u, 5u, UNER_SPEC_F_EVT, 0u, evt_network_ip_handler },
    { UNER_EVT_BOOT_COMPLETE, 5u, 5u, UNER_SPEC_F_EVT, 0u, evt_boot_complete_handler },
    { UNER_EVT_SCREEN_CHANGED, 5u, 5u, UNER_SPEC_F_EVT, 0u, NULL },
    { UNER_EVT_MENU_SELECTION_CHANGED, 7u, 7u, UNER_SPEC_F_EVT, 0u, NULL },
    { UNER_CMD_ECHO, 4u, 4u, 0u, 0u, NULL },
};

static void UNER_App_ClearWaitingValidation(void)
{
    uner_waiting_validation = 0u;
    uner_wait_cmd_id = 0u;
    uner_wait_started_at = 0u;
    uner_wait_timeout_ms = 0u;
}

static void UNER_App_ShowNotification(RenderFunction renderFn, uint16_t timeout_ms)
{
    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(renderFn, timeout_ms);
}

static void UNER_App_ShowSimpleNotification(OledSimpleNotificationId notificationId, uint16_t timeout_ms)
{
    OledUtils_ReplaceSimpleNotificationMs(notificationId, timeout_ms);
}

static void UNER_App_UpdateEncoderFastScroll(const UNER_Packet *packet)
{
    uint8_t value_index = 0u;

    if (!packet || !packet->payload || packet->len == 0u) {
        return;
    }

    /* Compatibilidad con respuestas viejas que llevaban un byte de estado al frente. */
    if (packet->len >= 2u) {
        value_index = 1u;
    }

    encoder_fast_scroll_enabled = (packet->payload[value_index] != 0u) ? 1u : 0u;
}

static void UNER_App_ExecuteCommand(void *ctx, const UNER_Packet *packet)
{
    (void)ctx;
    if (!packet) {
        return;
    }

    if (packet->cmd == UNER_CMD_ACK || packet->cmd == UNER_CMD_NACK) {
        if (packet->len >= 1u && packet->payload && packet->payload[0] == uner_wait_cmd_id) {
            UNER_App_ClearWaitingValidation();
        }
        return;
    }

    if (uner_waiting_validation && packet->cmd == uner_wait_cmd_id) {
        UNER_App_ClearWaitingValidation();
    }

    if (packet->cmd == UNER_CMD_ECHO) {
    	UNER_App_ShowSimpleNotification(OLED_SIMPLE_NOTIFICATION_COMMAND_RECEIVED, 2000u);
    }

    if (packet->cmd == UNER_CMD_PING) {
        SET_FLAG(systemFlags2, ESP_PRESENT);
        UNER_App_ShowSimpleNotification(OLED_SIMPLE_NOTIFICATION_PING_RECEIVED, 2000u);
    }

    if (packet->cmd == UNER_CMD_SET_ENCODER_FAST) {
        UNER_App_UpdateEncoderFastScroll(packet);
    }

    if (packet->cmd == UNER_CMD_REQUEST_FIRMWARE) {
        UNER_App_ParseFirmwareResponse(packet);
    }

    if (packet->cmd == UNER_CMD_GET_SCAN_RESULTS) {
        if (packet->len >= 1u && packet->payload && packet->payload[0] == 0xFEu) {
            UNER_App_ShowScanResultsScreen();
        } else {
            UNER_App_ParseScanResultsResponse(packet);
        }
    }

    if (packet->cmd == UNER_CMD_GET_STATUS) {
        UNER_App_ParseStatusResponse(packet);
    }

    if (packet->cmd == UNER_CMD_CONNECT_WIFI || packet->cmd == UNER_CMD_START_AP) {
        UNER_App_ParseWifiFinalizer(packet);
    }

    if (packet->cmd == UNER_CMD_AUTH_VALIDATE_PIN) {
        UNER_App_ParseAuthValidatePinResponse(packet);
    }
}

static void UNER_App_UpdateModeFlags(uint8_t mode)
{
    switch (mode) {
        case WIFI_MODE_AP:
            SET_FLAG(systemFlags2, AP_ACTIVE);
            CLEAR_FLAG(systemFlags2, WIFI_ACTIVE);
            break;

        case WIFI_MODE_STA:
            SET_FLAG(systemFlags2, WIFI_ACTIVE);
            CLEAR_FLAG(systemFlags2, AP_ACTIVE);
            break;

        case WIFI_MODE_AP_STA:
            SET_FLAG(systemFlags2, WIFI_ACTIVE);
            SET_FLAG(systemFlags2, AP_ACTIVE);
            break;

        case WIFI_MODE_OFF:
        default:
            CLEAR_FLAG(systemFlags2, WIFI_ACTIVE);
            CLEAR_FLAG(systemFlags2, AP_ACTIVE);
            break;
    }
}

static void UNER_App_SetOledSsidFromBytes(const uint8_t *ssid, uint8_t len)
{
    char ssid_buf[WIFI_SSID_MAX_LEN + 1u];
    uint8_t copy_len = len;

    if (!ssid || len == 0u) {
        return;
    }

    if (copy_len > WIFI_SSID_MAX_LEN) {
        copy_len = WIFI_SSID_MAX_LEN;
    }

    (void)memcpy(ssid_buf, ssid, copy_len);
    ssid_buf[copy_len] = '\0';
    OledUtils_SetWiFiConnectedSsid(ssid_buf);
}

static void UNER_App_SetStaIp(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
{
    espStaIp.block1 = b1;
    espStaIp.block2 = b2;
    espStaIp.block3 = b3;
    espStaIp.block4 = b4;
    espWifiConnection.staIp.block1 = b1;
    espWifiConnection.staIp.block2 = b2;
    espWifiConnection.staIp.block3 = b3;
    espWifiConnection.staIp.block4 = b4;
    espWifiConnection.staIpValid = (b1 || b2 || b3 || b4) ? 1u : 0u;
}

static void UNER_App_SetApIp(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
{
    espApIp.block1 = b1;
    espApIp.block2 = b2;
    espApIp.block3 = b3;
    espApIp.block4 = b4;
    espWifiConnection.apIp.block1 = b1;
    espWifiConnection.apIp.block2 = b2;
    espWifiConnection.apIp.block3 = b3;
    espWifiConnection.apIp.block4 = b4;
    espWifiConnection.apIpValid = (b1 || b2 || b3 || b4) ? 1u : 0u;
}

static void UNER_App_StoreWifiInfoSsid(const uint8_t *payload, uint8_t len)
{
    if (!payload || len < 7u) {
        return;
    }

    const uint8_t ssid_len = payload[6u];
    if (ssid_len == 0u || ((uint16_t)7u + ssid_len) > len) {
        return;
    }

    UNER_App_SetOledSsidFromBytes(&payload[7u], ssid_len);
}

static void UNER_App_StoreTargetCredentialsSsid(const uint8_t *payload, uint8_t len)
{
    if (!payload || len < 2u) {
        return;
    }

    const uint8_t ssid_len = payload[1u];
    if (ssid_len == 0u || ((uint16_t)2u + ssid_len) > len) {
        return;
    }

    UNER_App_SetOledSsidFromBytes(&payload[2u], ssid_len);
}

static void UNER_App_StoreNetworkIpPayload(const uint8_t *payload, uint8_t len)
{
    if (!payload || len < 5u) {
        return;
    }

    switch (payload[0]) {
        case 0x01u:
            UNER_App_SetStaIp(payload[1], payload[2], payload[3], payload[4]);
            UNER_App_MarkStaActive();
            break;

        case 0x02u:
            UNER_App_SetApIp(payload[1], payload[2], payload[3], payload[4]);
            SET_FLAG(systemFlags2, AP_ACTIVE);
            break;

        default:
            break;
    }
}

static void UNER_App_SetWifiNetworkEncryptionText(uint8_t index, uint8_t encryption_type)
{
    char *dst;
    const char *text = NULL;

    if (index >= WIFI_SCAN_MAX_NETWORKS) {
        return;
    }

    dst = wifiNetworkEncryptions[index];

    switch (encryption_type) {
        case 0u:
            text = "OPEN";
            break;

        case 1u:
            text = "WEP";
            break;

        case 2u:
            text = "WPA";
            break;

        case 3u:
            text = "WPA2";
            break;

        case 4u:
            text = "WPA/WPA2";
            break;

        case 5u:
            text = "WPA2 ENT";
            break;

        case 6u:
            text = "WPA3";
            break;

        case 7u:
            text = "WPA2/WPA3";
            break;

        case 8u:
            text = "WAPI";
            break;

        default:
            text = NULL;
            break;
    }

    if (text) {
        (void)strncpy(dst, text, WIFI_ENCRYPTION_MAX_LEN);
        dst[WIFI_ENCRYPTION_MAX_LEN] = '\0';
        return;
    }

    (void)memcpy(dst, "ENC ", 4u);
    if (encryption_type >= 100u) {
        dst[4] = (char)('0' + ((encryption_type / 100u) % 10u));
        dst[5] = (char)('0' + ((encryption_type / 10u) % 10u));
        dst[6] = (char)('0' + (encryption_type % 10u));
        dst[7] = '\0';
    } else if (encryption_type >= 10u) {
        dst[4] = (char)('0' + ((encryption_type / 10u) % 10u));
        dst[5] = (char)('0' + (encryption_type % 10u));
        dst[6] = '\0';
    } else {
        dst[4] = (char)('0' + encryption_type);
        dst[5] = '\0';
    }
}

static int8_t UNER_App_FindWifiNetworkIndexBySsid(const uint8_t *ssid, uint8_t len)
{
    if (!ssid || len == 0u) {
        return -1;
    }

    for (uint8_t i = 0u; i < networksFound && i < WIFI_SCAN_MAX_NETWORKS; ++i) {
        if (strlen(wifiNetworkSsids[i]) == len &&
            memcmp(wifiNetworkSsids[i], ssid, len) == 0) {
            return (int8_t)i;
        }
    }

    return -1;
}

static uint8_t UNER_App_IsStaActive(void)
{
    return IS_FLAG_SET(systemFlags2, WIFI_ACTIVE) ? 1u : 0u;
}

static void UNER_App_MarkStaActive(void)
{
    SET_FLAG(systemFlags2, ESP_PRESENT);
    SET_FLAG(systemFlags2, WIFI_ACTIVE);
    CLEAR_FLAG(systemFlags2, AP_ACTIVE);
}

static void UNER_App_ShowWifiConnectedNotification(uint8_t was_wifi_active)
{
    if (OledUtils_IsNotificationShowing(OledUtils_RenderESPWifiConnectedNotification)) {
        return;
    }

    if (!was_wifi_active ||
        OledUtils_IsNotificationShowing(OledUtils_RenderESPWifiConnectingNotification) ||
        OledUtils_IsSimpleNotificationShowing(OLED_SIMPLE_NOTIFICATION_ESP_CHECKING_CONNECTION)) {
        OledUtils_ReplaceNotificationMs(OledUtils_RenderESPWifiConnectedNotification, 2500u);
    }
}

static void UNER_App_RequestNetworkUiRefresh(void)
{
    if (menuSystem.renderFn == OledUtils_RenderDashboard_Wrapper ||
        menuSystem.renderFn == OledUtils_RenderWiFiStatus) {
        menuSystem.renderFlag = true;
    }
}

static void UNER_App_ParseFirmwareResponse(const UNER_Packet *p)
{
    // Formato real response: [status, ascii_fw...]
    if (!p || !p->payload || p->len < 2u) {
        return;
    }

    if (p->payload[0] != 0u) { // status != OK
        return;
    }
    SET_FLAG(systemFlags2, ESP_PRESENT);

    uint8_t fw_len = (uint8_t)(p->len - 1u);
    if (fw_len > 32u) {
        fw_len = 32u;
    }

    (void)memcpy(espFirmwareVersion, &p->payload[1], fw_len);
    espFirmwareVersion[fw_len] = '\0';

    OledUtils_DismissNotification();
    menuSystem.renderFn = OledUtils_RenderESPFirmwareScreen_Wrapper;
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static void UNER_App_ParseStatusResponse(const UNER_Packet *p)
{
    // GET_STATUS real:
    // [0]=status
    // [1]=ap_active
    // [2]=wifi_active
    // [3..] = wifi_info = [mode, wl_status, ip0,ip1,ip2,ip3,ssid_len,ssid...]
    // [len-8 .. len-5] = sta_ip
    // [len-4 .. len-1] = ap_ip
    //
    // Largo mínimo cuando ssid_len=0:
    // 1 + 2 + 7 + 8 = 18 bytes
    if (!p || !p->payload || p->len < 18u) {
        return;
    }

    if (p->payload[0] != 0u) { // status != OK
        return;
    }

    // Flags "rápidos" del payload
    SET_FLAG(systemFlags2, ESP_PRESENT);

    if (p->payload[1] != 0u) {
        SET_FLAG(systemFlags2, AP_ACTIVE);
    } else {
        CLEAR_FLAG(systemFlags2, AP_ACTIVE);
    }

    if (p->payload[2] != 0u) {
        SET_FLAG(systemFlags2, WIFI_ACTIVE);
    } else {
        CLEAR_FLAG(systemFlags2, WIFI_ACTIVE);
    }

    // mode dentro de wifi_info
    UNER_App_UpdateModeFlags(p->payload[3]);
    UNER_App_StoreWifiInfoSsid(&p->payload[3], (uint8_t)(p->len - 3u));

    // IPs finales (robusto: tomadas desde el tail)
    UNER_App_SetStaIp(p->payload[p->len - 8u],
                      p->payload[p->len - 7u],
                      p->payload[p->len - 6u],
                      p->payload[p->len - 5u]);

    UNER_App_SetApIp(p->payload[p->len - 4u],
                     p->payload[p->len - 3u],
                     p->payload[p->len - 2u],
                     p->payload[p->len - 1u]);
    UNER_App_RequestNetworkUiRefresh();

    if (OledUtils_IsSimpleNotificationShowing(OLED_SIMPLE_NOTIFICATION_ESP_CHECKING_CONNECTION)) {
        if (IS_FLAG_SET(systemFlags2, WIFI_ACTIVE)) {
            OledUtils_ReplaceNotificationMs(OledUtils_RenderESPWifiConnectedNotification, 2500u);
        } else {
            OledUtils_ReplaceNotificationMs(OledUtils_RenderWiFiNotConnected, 2500u);
        }
    }
}

static void UNER_App_ParseWifiFinalizer(const UNER_Packet *p)
{
    // Finalizadores raw:
    // - CMD 0x48 (CONNECT_WIFI): [0xFE, sta_ip0..3]
    // - CMD 0x4B (START_AP):     [0xFE, ap_ip0..3]
    if (!p || !p->payload || p->len < 5u) {
        return;
    }

    if (p->payload[0] != 0xFEu) {
        return;
    }

    if (p->cmd == UNER_CMD_CONNECT_WIFI) {
        uint8_t was_wifi_active = UNER_App_IsStaActive();

        UNER_App_SetStaIp(p->payload[1], p->payload[2], p->payload[3], p->payload[4]);

        UNER_App_MarkStaActive();
        UNER_App_ShowWifiConnectedNotification(was_wifi_active);
        UNER_App_RequestNetworkUiRefresh();
    }
    else if (p->cmd == UNER_CMD_START_AP) {
        UNER_App_SetApIp(p->payload[1], p->payload[2], p->payload[3], p->payload[4]);

        SET_FLAG(systemFlags2, AP_ACTIVE);
        UNER_App_ShowNotification(OledUtils_RenderESPAPStartedNotification, 2500u);
        UNER_App_RequestNetworkUiRefresh();
    }
}

static void UNER_App_ShowScanResultsScreen(void)
{
    if (!wifiScanSessionActive &&
        !wifiScanResultsPending &&
        !IS_FLAG_SET(systemFlags3, WIFI_SEARCHING)) {
        return;
    }

    wifiScanSessionActive = 0u;
    wifiScanResultsPending = 1u;
    wifi_scan_results_next_poll_at = 0u;
    CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
    wifiSearchingTimeout = 0u;

    menuSystem.currentMenu = &wifiResultsMenu;
    menuSystem.renderFn = OledUtils_RenderWiFiSearchResults_Wrapper;
    menuSystem.renderFlag = true;
    oled_first_draw = false;
}

static void UNER_App_ServiceWifiScanResults(void)
{
    const uint8_t scan_in_progress =
        (wifiScanSessionActive && IS_FLAG_SET(systemFlags3, WIFI_SEARCHING)) ? 1u : 0u;
    uint32_t now;
    UNER_Status status;

    if (!scan_in_progress && !wifiScanResultsPending) {
        wifi_scan_results_next_poll_at = 0u;
        return;
    }

    if (uner_waiting_validation) {
        return;
    }

    now = HAL_GetTick();
    if (wifi_scan_results_next_poll_at != 0u &&
        (int32_t)(now - wifi_scan_results_next_poll_at) < 0) {
        return;
    }

    status = UNER_App_SendCommand(UNER_CMD_GET_SCAN_RESULTS, NULL, 0u);
    wifi_scan_results_next_poll_at = now + ((status == UNER_OK)
        ? (scan_in_progress ? WIFI_SCAN_RESULTS_POLL_RUNNING_MS : WIFI_SCAN_RESULTS_POLL_PENDING_MS)
        : WIFI_SCAN_RESULTS_POLL_RETRY_MS);
}

static void UNER_App_ParseScanResultsResponse(const UNER_Packet *p)
{
    if (!p || !p->payload || p->len < 2u) {
        return;
    }

    if (!wifiScanSessionActive &&
        !wifiScanResultsPending &&
        !IS_FLAG_SET(systemFlags3, WIFI_SEARCHING) &&
        MenuSys_GetCurrentScreenCode(&menuSystem) != SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS) {
        return;
    }

    const uint8_t status = p->payload[0];
    const uint8_t reportedCount = p->payload[1];
    uint8_t offset = 2u;

    if (status == 1u) {
        wifiScanSessionActive = 1u;
        wifiScanResultsPending = 0u;
        SET_FLAG(systemFlags3, WIFI_SEARCHING);
        wifi_scan_results_next_poll_at = HAL_GetTick() + WIFI_SCAN_RESULTS_POLL_RUNNING_MS;
        return;
    }

    WiFiScan_ClearResults();

    if (status == 0u) {
        while (offset < p->len &&
               networksFound < reportedCount &&
               networksFound < WIFI_SCAN_MAX_NETWORKS) {
            const uint8_t ssidLen = p->payload[offset++];
            uint8_t copyLen = ssidLen;

            if ((uint16_t)offset + ssidLen > p->len) {
                break;
            }

            if (copyLen > WIFI_SSID_MAX_LEN) {
                copyLen = WIFI_SSID_MAX_LEN;
            }

            (void)memcpy(wifiNetworkSsids[networksFound], &p->payload[offset], copyLen);
            wifiNetworkSsids[networksFound][copyLen] = '\0';
            wifiNetworkEncryptions[networksFound][0] = '\0';
            wifiNetworkSignalStrengths[networksFound] = 0;
            wifiNetworkChannels[networksFound] = 0u;
            wifiNetworkEncryptionTypes[networksFound] = 0u;
            wifiNetworkDetailValid[networksFound] = 0u;

            offset = (uint8_t)(offset + ssidLen);
            networksFound++;
        }
    }

    wifiSearchingTimeout = 0u;
    CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
    wifiScanSessionActive = 0u;
    wifiScanResultsPending = 0u;
    wifi_scan_results_next_poll_at = 0u;
    menuSystem.currentMenu = &wifiResultsMenu;
    menuSystem.renderFn = OledUtils_RenderWiFiSearchResults_Wrapper;
    menuSystem.renderFlag = true;
    oled_first_draw = false;

    if (status == 0u) {
        UNER_App_ShowNotification(OledUtils_ShowWifiResults, 1200u);
    }
}

static void UNER_App_ParseAuthValidatePinResponse(const UNER_Packet *p)
{
    if (!p || !p->payload || p->len < 4u || p->payload[0] != 0u) {
        return;
    }

    uint8_t offset = 1u;
    const uint8_t request_id = p->payload[offset++];
    const PermissionId_t permission = (PermissionId_t)p->payload[offset++];
    const bool granted = (p->payload[offset++] != 0u);

    uint32_t ttl_ms = 0u;
    if ((uint8_t)(p->len - offset) >= 4u) {
        ttl_ms = (uint32_t)p->payload[offset] |
                 ((uint32_t)p->payload[offset + 1u] << 8) |
                 ((uint32_t)p->payload[offset + 2u] << 16) |
                 ((uint32_t)p->payload[offset + 3u] << 24);
        offset = (uint8_t)(offset + 4u);
    }

    const uint8_t attempts_left = ((uint8_t)(p->len - offset) >= 1u)
        ? p->payload[offset]
        : 0xFFu;

    Permission_OnValidationResult(request_id, permission, granted, ttl_ms, attempts_left);
}

static uint32_t UNER_App_ReadU32Le(const uint8_t *payload)
{
    if (!payload) {
        return 0u;
    }

    return (uint32_t)payload[0] |
           ((uint32_t)payload[1] << 8) |
           ((uint32_t)payload[2] << 16) |
           ((uint32_t)payload[3] << 24);
}

static uint16_t UNER_App_ReadU16Le(const uint8_t *payload)
{
    if (!payload) {
        return 0u;
    }

    return (uint16_t)payload[0] |
           ((uint16_t)payload[1] << 8);
}

static void UNER_App_ResetBinaryAssetState(uint8_t asset_id)
{
    binary_asset_id = asset_id;
    binary_asset_width = 0u;
    binary_asset_height = 0u;
    binary_asset_total_len = 0u;
    binary_asset_received_len = 0u;
    binary_asset_ready = 0u;
    binary_asset_in_progress = 0u;
}

static uint8_t UNER_App_ParseBinaryAssetChunk(const UNER_Packet *packet)
{
    const uint8_t *payload;
    uint8_t status;
    uint8_t asset_id;
    uint8_t width;
    uint8_t height;
    uint16_t total_len;
    uint16_t offset;
    uint8_t data_len;
    uint16_t payload_data_len;
    uint16_t expected_len;

    if (!packet || !packet->payload || packet->len < UNER_BINARY_ASSET_CHUNK_HEADER_SIZE) {
        return 0u;
    }

    payload = packet->payload;
    status = payload[0];
    asset_id = payload[1];
    width = payload[2];
    height = payload[3];
    total_len = UNER_App_ReadU16Le(&payload[4]);
    offset = UNER_App_ReadU16Le(&payload[6]);
    data_len = payload[8];
    payload_data_len = (uint16_t)(packet->len - UNER_BINARY_ASSET_CHUNK_HEADER_SIZE);

    if (status != 0u || asset_id == 0u) {
        UNER_App_ResetBinaryAssetState(asset_id);
        return 0u;
    }

    if (data_len == 0u || payload_data_len != (uint16_t)data_len) {
        return 0u;
    }

    if (width == 0u || height == 0u || width > 128u || height > 64u) {
        return 0u;
    }

    expected_len = (uint16_t)((((uint16_t)width + 7u) / 8u) * (uint16_t)height);
    if (total_len == 0u || total_len > UNER_BINARY_ASSET_MAX_BYTES || total_len != expected_len) {
        return 0u;
    }

    if ((uint16_t)(offset + (uint16_t)data_len) > total_len) {
        return 0u;
    }

    if (offset == 0u) {
        binary_asset_id = asset_id;
        binary_asset_width = width;
        binary_asset_height = height;
        binary_asset_total_len = total_len;
        binary_asset_received_len = 0u;
        binary_asset_ready = 0u;
        binary_asset_in_progress = 1u;
    } else if (!binary_asset_in_progress ||
               binary_asset_id != asset_id ||
               binary_asset_width != width ||
               binary_asset_height != height ||
               binary_asset_total_len != total_len) {
        return 0u;
    }

    if (offset != binary_asset_received_len) {
        return 0u;
    }

    (void)memcpy(&binary_asset_buffer[offset],
                 &payload[UNER_BINARY_ASSET_CHUNK_HEADER_SIZE],
                 data_len);
    binary_asset_received_len = (uint16_t)(binary_asset_received_len + data_len);

    if (binary_asset_received_len == binary_asset_total_len) {
        binary_asset_ready = 1u;
        binary_asset_in_progress = 0u;

        if (MenuSys_GetCurrentScreenCode(&menuSystem) == SCREEN_CODE_SETTINGS_ABOUT_REPO) {
            menuSystem.renderFlag = true;
            oled_first_draw = true;
        }
    }

    return 1u;
}

static void UNER_App_SendCommandStatus(const UNER_Packet *packet, uint8_t status)
{
    uint8_t payload[1u] = { status };

    if (!packet) {
        return;
    }

    (void)UNER_Send(&uner_handle,
                    packet->transport_id,
                    UNER_NODE_MCU,
                    packet->src,
                    packet->cmd,
                    payload,
                    (uint8_t)sizeof(payload));
}

static uint8_t UNER_App_PacketScreenMatchesCurrent(const UNER_Packet *packet, uint32_t *screen_code)
{
    uint32_t requested_screen;

    if (!packet || !packet->payload || packet->len < 4u) {
        return 0u;
    }

    requested_screen = UNER_App_ReadU32Le(packet->payload);
    if (screen_code) {
        *screen_code = requested_screen;
    }

    return requested_screen == (uint32_t)MenuSys_GetCurrentScreenCode(&menuSystem);
}

static int8_t UNER_App_FindVisibleMenuItemIndex(const SubMenu *menu, uint8_t visible_item)
{
    uint8_t visible_count = 0u;

    if (!menu || visible_item == 0u || visible_item > MENU_VISIBLE_ITEMS) {
        return -1;
    }

    for (int8_t i = menu->firstVisibleItem; i >= 0 && i < menu->itemCount; ++i) {
        if (MenuSys_IsItemVisible(&menu->items[i])) {
            visible_count++;
            if (visible_count == visible_item) {
                return i;
            }
        }
    }

    return -1;
}

static uint8_t UNER_App_DispatchUserEvent(UserEvent_t event)
{
    if (!menuSystem.userEventManagerFn) {
        return 0u;
    }

    menuSystem.userEventManagerFn(event);
    return 1u;
}

static uint8_t UNER_App_IsCurrentMenuScreen(uint32_t screen_code)
{
    return (menuSystem.currentMenu != NULL &&
            screen_code == (uint32_t)menuSystem.currentMenu->screen_code &&
            screen_code == (uint32_t)MenuSys_GetCurrentScreenCode(&menuSystem));
}

static uint8_t screen_is_permission_pin_flow(uint32_t screen_code)
{
    switch ((ScreenCode_t)screen_code) {
        case SCREEN_CODE_WARNING_PIN_ENTRY:
        case SCREEN_CODE_WARNING_PIN_WAITING:
            return 1u;

        default:
            return 0u;
    }
}

static void cmd_auth_pin_granted_handler(void *ctx, const UNER_Packet *packet)
{
    const AuthPinGranted_t *msg;
    uint32_t requested_screen;
    uint32_t current_screen;

    (void)ctx;

    if (!packet || !packet->payload || packet->len != sizeof(AuthPinGranted_t)) {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_BAD_PAYLOAD);
        return;
    }

    msg = (const AuthPinGranted_t *)packet->payload;
    requested_screen = UNER_App_ReadU32Le((const uint8_t *)msg);
    current_screen = (uint32_t)MenuSys_GetCurrentScreenCode(&menuSystem);

    if (requested_screen != current_screen) {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_SCREEN_MISMATCH);
        return;
    }

    if (!screen_is_permission_pin_flow(current_screen)) {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_PIN_NOT_REQUIRED);
        return;
    }

    if (!Permission_GrantCurrentRequest(0u)) {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_NO_PENDING_PERMISSION);
        return;
    }

    UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_OK);
}

static void cmd_auth_remote_result_handler(void *ctx, const UNER_Packet *packet)
{
    const AuthRemoteResult_t *msg;
    uint32_t requested_screen;
    uint32_t current_screen;

    (void)ctx;

    if (!packet || !packet->payload || packet->len != sizeof(AuthRemoteResult_t)) {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_BAD_PAYLOAD);
        return;
    }

    msg = (const AuthRemoteResult_t *)packet->payload;
    requested_screen = UNER_App_ReadU32Le((const uint8_t *)&msg->screen_code);
    current_screen = (uint32_t)MenuSys_GetCurrentScreenCode(&menuSystem);

    if (requested_screen != current_screen) {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_SCREEN_MISMATCH);
        return;
    }

    if (!screen_is_permission_pin_flow(current_screen)) {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_PIN_NOT_REQUIRED);
        return;
    }

    if (msg->result_code > UNER_AUTH_REMOTE_RESULT_BLOCKED) {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_BAD_PAYLOAD);
        return;
    }

    if (!Permission_ApplyRemoteResult(msg->result_code, msg->attempts_left)) {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_NO_PENDING_PERMISSION);
        return;
    }

    UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_OK);
}

static void cmd_request_screen_page_handler(void *ctx, const UNER_Packet *packet)
{
    uint32_t screen_code = SCREEN_CODE_NONE;
    uint8_t direction;
    uint8_t steps;

    (void)ctx;

    if (!UNER_App_PacketScreenMatchesCurrent(packet, &screen_code) ||
        !UNER_App_IsCurrentMenuScreen(screen_code) ||
        packet->len != 5u) {
        UNER_App_SendCommandStatus(packet, 1u);
        return;
    }

    direction = packet->payload[4];
    if (direction != UNER_SCREEN_PAGE_DIR_UP &&
        direction != UNER_SCREEN_PAGE_DIR_DOWN) {
        UNER_App_SendCommandStatus(packet, 1u);
        return;
    }

    for (steps = 0u; steps < MENU_VISIBLE_ITEMS; ++steps) {
        if (direction == UNER_SCREEN_PAGE_DIR_UP) {
            MenuSys_MoveCursorUp(&menuSystem);
        } else {
            MenuSys_MoveCursorDown(&menuSystem);
        }
    }

    menuSystem.renderFlag = true;
    UNER_App_SendCommandStatus(packet, 0u);
}

static void cmd_menu_item_click_handler(void *ctx, const UNER_Packet *packet)
{
    uint32_t screen_code = SCREEN_CODE_NONE;
    int8_t item_index;
    uint8_t visible_item;

    (void)ctx;

    if (!UNER_App_PacketScreenMatchesCurrent(packet, &screen_code) ||
        !UNER_App_IsCurrentMenuScreen(screen_code) ||
        packet->len != 5u) {
        UNER_App_SendCommandStatus(packet, 1u);
        return;
    }

    visible_item = packet->payload[4];
    item_index = UNER_App_FindVisibleMenuItemIndex(menuSystem.currentMenu, visible_item);
    if (item_index < 0) {
        UNER_App_SendCommandStatus(packet, 1u);
        return;
    }

    menuSystem.currentMenu->lastSelectedItemIndex = menuSystem.currentMenu->currentItemIndex;
    menuSystem.currentMenu->currentItemIndex = item_index;
    MenuSys_ReportCurrentMenuSelection(&menuSystem, SCREEN_REPORT_SOURCE_MENU);
    MenuSys_HandleClick(&menuSystem);
    UNER_App_SendCommandStatus(packet, 0u);
}

static void cmd_trigger_encoder_button_handler(void *ctx, const UNER_Packet *packet)
{
    uint8_t press_kind;
    UserEvent_t event;

    (void)ctx;

    if (!UNER_App_PacketScreenMatchesCurrent(packet, NULL) || packet->len != 5u) {
        UNER_App_SendCommandStatus(packet, 1u);
        return;
    }

    press_kind = packet->payload[4];
    if (press_kind == UNER_PRESS_KIND_SHORT) {
        event = UE_ENC_SHORT_PRESS;
    } else if (press_kind == UNER_PRESS_KIND_LONG) {
        event = UE_ENC_LONG_PRESS;
    } else {
        UNER_App_SendCommandStatus(packet, 1u);
        return;
    }

    UNER_App_SendCommandStatus(packet, UNER_App_DispatchUserEvent(event) ? 0u : 1u);
}

static void cmd_trigger_user_button_handler(void *ctx, const UNER_Packet *packet)
{
    uint8_t press_kind;
    UserEvent_t event;

    (void)ctx;

    if (!UNER_App_PacketScreenMatchesCurrent(packet, NULL) || packet->len != 5u) {
        UNER_App_SendCommandStatus(packet, 1u);
        return;
    }

    press_kind = packet->payload[4];
    if (press_kind == UNER_PRESS_KIND_SHORT) {
        event = UE_SHORT_PRESS;
    } else if (press_kind == UNER_PRESS_KIND_LONG) {
        event = UE_LONG_PRESS;
    } else {
        UNER_App_SendCommandStatus(packet, 1u);
        return;
    }

    UNER_App_SendCommandStatus(packet, UNER_App_DispatchUserEvent(event) ? 0u : 1u);
}

static void cmd_trigger_encoder_rotate_left_handler(void *ctx, const UNER_Packet *packet)
{
    (void)ctx;

    if (!UNER_App_PacketScreenMatchesCurrent(packet, NULL) || packet->len != 4u) {
        UNER_App_SendCommandStatus(packet, 1u);
        return;
    }

    UNER_App_SendCommandStatus(packet, UNER_App_DispatchUserEvent(UE_ROTATE_CCW) ? 0u : 1u);
}

static void cmd_trigger_encoder_rotate_right_handler(void *ctx, const UNER_Packet *packet)
{
    (void)ctx;

    if (!UNER_App_PacketScreenMatchesCurrent(packet, NULL) || packet->len != 4u) {
        UNER_App_SendCommandStatus(packet, 1u);
        return;
    }

    UNER_App_SendCommandStatus(packet, UNER_App_DispatchUserEvent(UE_ROTATE_CW) ? 0u : 1u);
}

static void cmd_binary_asset_stream_handler(void *ctx, const UNER_Packet *packet)
{
    (void)ctx;

    if (!packet || !packet->payload) {
        return;
    }

    if (uner_waiting_validation && uner_wait_cmd_id == UNER_CMD_BINARY_ASSET_STREAM) {
        UNER_App_ClearWaitingValidation();
    }

    if (packet->len == 1u) {
        if (packet->payload[0] != 0u) {
            UNER_App_ResetBinaryAssetState(binary_asset_id);
        }
        return;
    }

    /*
     * This handler consumes ESP responses. The STM request is one byte
     * (asset_id); ESP responses are chunked and include width/height metadata.
     */
    (void)UNER_App_ParseBinaryAssetChunk(packet);
}

static void cmd_wifi_credentials_web_result_handler(void *ctx, const UNER_Packet *packet)
{
    uint8_t result;
    uint8_t ssid_len;
    uint8_t offset;
    uint8_t remaining;

    (void)ctx;

    if (!packet || !packet->payload || packet->len < 2u) {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_BAD_PAYLOAD);
        return;
    }

    result = packet->payload[0u];
    ssid_len = packet->payload[1u];
    offset = 2u;

    if (ssid_len > WIFI_SSID_MAX_LEN ||
        ((uint16_t)offset + ssid_len) > packet->len) {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_BAD_PAYLOAD);
        return;
    }

    if (ssid_len > 0u) {
        UNER_App_SetOledSsidFromBytes(&packet->payload[offset], ssid_len);
    }
    offset = (uint8_t)(offset + ssid_len);
    remaining = (uint8_t)(packet->len - offset);

    if (result == UNER_WIFI_WEB_CREDENTIALS_STATUS_SUCCESS) {
        if (remaining != 4u) {
            UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_BAD_PAYLOAD);
            return;
        }

        UNER_App_SetStaIp(packet->payload[offset],
                          packet->payload[offset + 1u],
                          packet->payload[offset + 2u],
                          packet->payload[offset + 3u]);
        OledUtils_SetWiFiCredentialsWebResultStatus(result);
        UNER_App_MarkStaActive();
        UNER_App_ShowNotification(OledUtils_RenderWiFiCredentialsSucceededNotification, 2500u);
    } else if (result == UNER_WIFI_WEB_CREDENTIALS_STATUS_FAILED ||
               result == UNER_WIFI_WEB_CREDENTIALS_STATUS_TIMEOUT ||
               result == UNER_WIFI_WEB_CREDENTIALS_STATUS_CANCELED) {
        if (remaining != 0u) {
            UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_BAD_PAYLOAD);
            return;
        }

        CLEAR_FLAG(systemFlags2, WIFI_ACTIVE);
        UNER_App_SetStaIp(0u, 0u, 0u, 0u);
        OledUtils_SetWiFiCredentialsWebResultStatus(result);
        UNER_App_ShowNotification(OledUtils_RenderWiFiCredentialsFailedNotification, 2500u);
    } else {
        UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_BAD_ARGUMENT);
        return;
    }

    UNER_App_RequestNetworkUiRefresh();
    UNER_App_SendCommandStatus(packet, UNER_CMD_STATUS_OK);
}

static void cmd_wifi_get_detail_handler(void *ctx, const UNER_Packet *packet)
{
    uint8_t ssid_len;
    uint8_t offset;
    int8_t index;

    (void)ctx;

    if (!packet || !packet->payload || packet->len < 4u) {
        return;
    }

    ssid_len = packet->payload[0u];
    offset = 1u;

    if (ssid_len == 0u ||
        ssid_len > WIFI_SSID_MAX_LEN ||
        ((uint16_t)offset + ssid_len + 3u) != packet->len) {
        return;
    }

    index = UNER_App_FindWifiNetworkIndexBySsid(&packet->payload[offset], ssid_len);
    if (index < 0) {
        return;
    }

    offset = (uint8_t)(offset + ssid_len);
    wifiNetworkSignalStrengths[(uint8_t)index] = (int8_t)packet->payload[offset++];
    wifiNetworkEncryptionTypes[(uint8_t)index] = packet->payload[offset++];
    wifiNetworkChannels[(uint8_t)index] = packet->payload[offset];
    wifiNetworkDetailValid[(uint8_t)index] = 1u;
    UNER_App_SetWifiNetworkEncryptionText((uint8_t)index,
                                          wifiNetworkEncryptionTypes[(uint8_t)index]);

    if (wifiSelectedNetworkIndex == (uint8_t)index &&
        MenuSys_GetCurrentScreenCode(&menuSystem) == SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS) {
        menuSystem.renderFlag = true;
        oled_first_draw = true;
    }
}


static void evt_boot_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    espBootRebootPendingResponse = 0;
    SET_FLAG(systemFlags2, ESP_PRESENT);
    UNER_App_ShowSimpleNotification(OLED_SIMPLE_NOTIFICATION_ESP_BOOT_RECEIVED, 2000u);
}

static void evt_mode_changed_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    if (!p || p->len == 0u || !p->payload) {
        return;
    }

    // Evento 0x81: payload directo wifi_info => byte0=mode, byte1=wl_status,
    // byte2..5=IP, byte6=ssid_len, resto ssid
    const uint8_t mode = p->payload[0];
    UNER_App_UpdateModeFlags(mode);
    UNER_App_StoreWifiInfoSsid(p->payload, p->len);

    if (p->len >= 6u) {
        // IP reportada por el wifi_info del evento
        UNER_App_SetStaIp(p->payload[2], p->payload[3], p->payload[4], p->payload[5]);

        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_AP_STA) {
            UNER_App_SetApIp(p->payload[2], p->payload[3], p->payload[4], p->payload[5]);
        }
    }

    UNER_App_ShowSimpleNotification(OLED_SIMPLE_NOTIFICATION_ESP_MODE_CHANGED, 2200u);
}

static void evt_sta_connected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    uint8_t was_wifi_active = UNER_App_IsStaActive();
    UNER_App_MarkStaActive();

    if (p && p->payload) {
        UNER_App_StoreWifiInfoSsid(p->payload, p->len);
        if (p->len >= 6u) {
            UNER_App_SetStaIp(p->payload[2u], p->payload[3u], p->payload[4u], p->payload[5u]);
        }
    }

    UNER_App_ShowWifiConnectedNotification(was_wifi_active);
    UNER_App_RequestNetworkUiRefresh();
}

static void evt_sta_disconnected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    CLEAR_FLAG(systemFlags2, WIFI_ACTIVE);
    UNER_App_SetStaIp(0u, 0u, 0u, 0u);
    UNER_App_RequestNetworkUiRefresh();
}

static void evt_ap_client_join_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; }
static void evt_ap_client_leave_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; }
static void evt_webserver_up_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    UNER_App_ShowSimpleNotification(OLED_SIMPLE_NOTIFICATION_ESP_WEB_SERVER_UP, 2000u);
}
static void evt_webserver_client_conn_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    UNER_App_ShowSimpleNotification(OLED_SIMPLE_NOTIFICATION_ESP_WEB_CLIENT_CONNECTED, 2000u);
}

static void evt_webserver_client_disconn_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    UNER_App_ShowSimpleNotification(OLED_SIMPLE_NOTIFICATION_ESP_WEB_CLIENT_DISCONNECTED, 2000u);
}
static void evt_lastwifi_notfound_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; }
static void evt_app_mpu_readings_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; }
static void evt_app_tcrt_readings_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; }
static void evt_app_user_connected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; }
static void evt_app_user_disconnected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; }
static void evt_controller_connected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    SET_FLAG(systemFlags2, RF_ACTIVE);
    UNER_App_ShowSimpleNotification(OLED_SIMPLE_NOTIFICATION_CONTROLLER_CONNECTED, 2000u);
}

static void evt_controller_disconnected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    CLEAR_FLAG(systemFlags2, RF_ACTIVE);
    UNER_App_ShowSimpleNotification(OLED_SIMPLE_NOTIFICATION_CONTROLLER_DISCONNECTED, 2000u);
}

static void evt_usb_connected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    //SET_FLAG(systemFlags2, USB_ACTIVE);
    UNER_App_ShowSimpleNotification(OLED_SIMPLE_NOTIFICATION_ESP_USB_CONNECTED, 2000u);
}

static void evt_usb_disconnected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
}

static void evt_wifi_connected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    uint8_t was_wifi_active = UNER_App_IsStaActive();
    UNER_App_MarkStaActive();

    if (p && p->payload) {
        UNER_App_StoreTargetCredentialsSsid(p->payload, p->len);
    }

    if (espWifiConnection.staIpValid) {
        UNER_App_ShowWifiConnectedNotification(was_wifi_active);
    }
    UNER_App_RequestNetworkUiRefresh();
}

static void evt_network_ip_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    if (!p || !p->payload) {
        return;
    }

    uint8_t was_wifi_active = UNER_App_IsStaActive();
    uint8_t is_sta_ip = (p->len >= 5u && p->payload[0] == 0x01u) ? 1u : 0u;

    UNER_App_StoreNetworkIpPayload(p->payload, p->len);
    if (is_sta_ip) {
        UNER_App_ShowWifiConnectedNotification(was_wifi_active);
    }
    UNER_App_RequestNetworkUiRefresh();
}

static void evt_boot_complete_handler(void *ctx, const UNER_Packet *p)
{
    evt_network_ip_handler(ctx, p);
}

static void cmd_network_ip_handler(void *ctx, const UNER_Packet *p)
{
    evt_network_ip_handler(ctx, p);
}

static void cmd_boot_complete_handler(void *ctx, const UNER_Packet *p)
{
    evt_network_ip_handler(ctx, p);
}

static void UNER_App_WriteMenuSelectionReportPayload(uint8_t *payload,
                                                     uint32_t screen_code,
                                                     uint8_t selected_index,
                                                     uint8_t item_count,
                                                     uint8_t source)
{
    payload[0] = (uint8_t)(screen_code & 0xFFu);
    payload[1] = (uint8_t)((screen_code >> 8) & 0xFFu);
    payload[2] = (uint8_t)((screen_code >> 16) & 0xFFu);
    payload[3] = (uint8_t)((screen_code >> 24) & 0xFFu);
    payload[4] = selected_index;
    payload[5] = item_count;
    payload[6] = source;
}

UNER_Status UNER_App_ReportMenuSelectionChanged(uint32_t screen_code,
                                                uint8_t selected_index,
                                                uint8_t item_count,
                                                uint8_t source)
{
    uint8_t payload[UNER_SELECTION_PAYLOAD_SIZE];

    if (screen_code == SCREEN_CODE_NONE || item_count == 0u || selected_index >= item_count) {
        return UNER_ERR_LEN;
    }

    UNER_App_WriteMenuSelectionReportPayload(payload,
                                             screen_code,
                                             selected_index,
                                             item_count,
                                             source);

    return UNER_Send(&uner_handle,
                     UNER_TR_UART1_ESP,
                     UNER_NODE_MCU,
                     UNER_NODE_BROADCAST,
                     UNER_EVT_MENU_SELECTION_CHANGED,
                     payload,
                     (uint8_t)sizeof(payload));
}

static void UNER_App_WriteScreenReportPayload(uint8_t *payload, uint32_t screen_code, uint8_t source)
{
    if (!payload) {
        return;
    }

    payload[0] = (uint8_t)(screen_code & 0xFFu);
    payload[1] = (uint8_t)((screen_code >> 8) & 0xFFu);
    payload[2] = (uint8_t)((screen_code >> 16) & 0xFFu);
    payload[3] = (uint8_t)((screen_code >> 24) & 0xFFu);
    payload[4] = source;
}

static void UNER_App_WriteCarModePayload(uint8_t *payload, uint8_t mode)
{
    if (!payload) {
        return;
    }

    payload[0] = mode;
}

UNER_Status UNER_App_ReportScreenChanged(uint32_t screen_code, uint8_t source)
{
    uint8_t payload[5u];

    if (screen_code == SCREEN_CODE_NONE) {
        return UNER_ERR_LEN;
    }

    UNER_App_WriteScreenReportPayload(payload, screen_code, source);

    return UNER_Send(&uner_handle,
                     UNER_TR_UART1_ESP,
                     UNER_NODE_MCU,
                     UNER_NODE_BROADCAST,
                     UNER_EVT_SCREEN_CHANGED,
                     payload,
                     (uint8_t)sizeof(payload));
}

UNER_Status UNER_App_ReportCarModeChanged(uint8_t mode)
{
    uint8_t payload[1u];

    UNER_App_WriteCarModePayload(payload, mode);

    return UNER_Send(&uner_handle,
                     UNER_TR_UART1_ESP,
                     UNER_NODE_MCU,
                     UNER_NODE_BROADCAST,
                     UNER_EVT_ID_CAR_MODE_CHANGED,
                     payload,
                     (uint8_t)sizeof(payload));
}

UNER_Status UNER_App_RequestBinaryAsset(uint8_t asset_id)
{
    BinaryAssetRequest_t request;
    UNER_Status status;

    if (asset_id == 0u) {
        return UNER_ERR_LEN;
    }

    if (binary_asset_ready && binary_asset_id == asset_id) {
        return UNER_OK;
    }

    if (binary_asset_in_progress && binary_asset_id == asset_id) {
        return UNER_OK;
    }

    if (uner_waiting_validation && uner_wait_cmd_id != UNER_CMD_BINARY_ASSET_STREAM) {
        return UNER_ERR_BUSY;
    }

    request.asset_id = asset_id;
    status = UNER_App_SendCommand(UNER_CMD_BINARY_ASSET_STREAM,
                                  (const uint8_t *)&request,
                                  (uint8_t)sizeof(request));
    if (status == UNER_OK) {
        UNER_App_ResetBinaryAssetState(asset_id);
        binary_asset_in_progress = 1u;
    }

    return status;
}

uint8_t UNER_App_GetBinaryAsset(uint8_t asset_id, BinaryAssetView_t *asset)
{
    if (!asset || !binary_asset_ready || binary_asset_id != asset_id) {
        return 0u;
    }

    asset->asset_id = binary_asset_id;
    asset->width = binary_asset_width;
    asset->height = binary_asset_height;
    asset->len = binary_asset_total_len;
    asset->data = binary_asset_buffer;

    return 1u;
}

static void cmd_request_firmware_handler(void *ctx, const UNER_Packet *packet)
{
    (void)ctx;

    if (!packet || packet->len < 1u) {
        return;
    }

    uint8_t status = packet->payload[0];

    if (status != 0u) {
        espFirmwareVersion[0] = '\0';
        esp_firmware_received_flag = 1u;
        return;
    }

    uint16_t str_len = (uint16_t)(packet->len - 1u);
    if (str_len >= sizeof(espFirmwareVersion)) {
        str_len = (uint16_t)(sizeof(espFirmwareVersion) - 1u);
    }

    memcpy(espFirmwareVersion, &packet->payload[1], str_len);
    espFirmwareVersion[str_len] = '\0';

    esp_firmware_received_flag = 1u;
}

static void cmd_ack_handler(void *ctx, const UNER_Packet *packet)
{
    (void)ctx;

    if (!packet || packet->len < 1u) {
        return;
    }

    uint8_t acked_cmd = packet->payload[0];

    /* 🔹 Integración con sistema de validación */
    if (uner_waiting_validation && uner_wait_cmd_id == acked_cmd) {
        UNER_App_ClearWaitingValidation();
    }
}

static void cmd_get_current_screen_handler(void *ctx, const UNER_Packet *p)
{
    uint8_t payload[5u];

    (void)ctx;

    if (!p || p->len != 0u) {
        return;
    }

    UNER_App_WriteScreenReportPayload(payload,
                                      MenuSys_GetCurrentScreenCode(&menuSystem),
                                      MenuSys_GetCurrentScreenSource(&menuSystem));

    (void)UNER_Send(&uner_handle,
                    p->transport_id,
                    UNER_NODE_MCU,
                    p->src,
                    UNER_CMD_GET_CURRENT_SCREEN,
                    payload,
                    (uint8_t)sizeof(payload));
}

static void cmd_get_car_mode_handler(void *ctx, const UNER_Packet *p)
{
    uint8_t payload[1u];

    (void)ctx;

    if (!p || p->len != 0u) {
        return;
    }

    payload[0] = (uint8_t)GET_CAR_MODE();

    (void)UNER_Send(&uner_handle,
                    p->transport_id,
                    UNER_NODE_MCU,
                    p->src,
                    UNER_CMD_GET_CAR_MODE,
                    payload,
                    (uint8_t)sizeof(payload));
}

static void cmd_reset_mcu_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    if (p && p->transport_id == UNER_TR_UART1_ESP) {
        uint8_t ack_payload[2u] = { UNER_CMD_RESET_MCU, 0u };
        uint8_t ack_frame[UNER_OVERHEAD + sizeof(ack_payload)];

        if (UNER_BuildFrame(
                ack_frame,
                sizeof(ack_frame),
                UNER_NODE_MCU,
                p->src,
                UNER_CMD_ACK,
                ack_payload,
                (uint8_t)sizeof(ack_payload)) == UNER_OK) {
            uint32_t started_at = HAL_GetTick();
            while (usart1_tx_busy &&
                   (uint32_t)(HAL_GetTick() - started_at) < 20u) {
                __NOP();
            }

            if (!usart1_tx_busy) {
                usart1_tx_busy = 1u;
                (void)HAL_UART_Transmit(&huart1, ack_frame, sizeof(ack_frame), 20u);
                usart1_tx_busy = 0u;
            }
        }
    }

    __disable_irq();
    NVIC_SystemReset();
    while (1) {
    }
}



static const UNER_CommandSpec *UNER_App_FindSpecById(uint8_t cmd)
{
    for (uint8_t i = 0u; i < (uint8_t)(sizeof(uner_commands) / sizeof(uner_commands[0])); ++i) {
        if (uner_commands[i].id == cmd) {
            return &uner_commands[i];
        }
    }
    return NULL;
}

UNER_Status UNER_App_SendCommand(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    const UNER_CommandSpec *spec = UNER_App_FindSpecById(cmd);
    if (!spec) {
        return UNER_ERR_LEN;
    }

    if (len < spec->min_args || len > spec->max_args) {
        return UNER_ERR_LEN;
    }

    uint8_t dst = UNER_NODE_PC_QT;
    if (spec->flags & UNER_SPEC_F_EVT) {
        dst = UNER_NODE_BROADCAST;
    }

    UNER_Status status = UNER_Send(&uner_handle, UNER_TR_UART1_ESP, UNER_NODE_MCU, dst, cmd, payload, len);
    if (status == UNER_OK) {
        if (spec->flags & (UNER_SPEC_F_ACK | UNER_SPEC_F_RESP)) {
            uner_waiting_validation = 1u;
            uner_wait_cmd_id = cmd;
            uner_wait_started_at = HAL_GetTick();
            uner_wait_timeout_ms = spec->timeout_ms;
        } else {
            UNER_App_ClearWaitingValidation();
        }

        if (cmd == UNER_CMD_CONNECT_WIFI) {
            UNER_App_ShowNotification(OledUtils_RenderESPWifiConnectingNotification, 5000u);
        }
    }
    return status;
}

UNER_Status UNER_App_SendWifiCredentialRequest(const char *ssid)
{
    static const char password[] = "connRequest";
    uint8_t payload[3u + WIFI_SSID_MAX_LEN + sizeof(password)];
    uint8_t ssid_len;
    uint8_t pass_len;
    uint8_t offset = 0u;

    if (!ssid || ssid[0] == '\0') {
        return UNER_ERR_LEN;
    }

    ssid_len = (uint8_t)strlen(ssid);
    if (ssid_len > WIFI_SSID_MAX_LEN) {
        ssid_len = WIFI_SSID_MAX_LEN;
    }

    pass_len = (uint8_t)(sizeof(password) - 1u);

    payload[offset++] = 1u;
    payload[offset++] = ssid_len;
    (void)memcpy(&payload[offset], ssid, ssid_len);
    offset = (uint8_t)(offset + ssid_len);
    payload[offset++] = pass_len;
    (void)memcpy(&payload[offset], password, pass_len);
    offset = (uint8_t)(offset + pass_len);

    return UNER_App_SendCommand(UNER_CMD_ID_SET_CREDENTIALS, payload, offset);
}

UNER_Status UNER_App_RequestWifiNetworkDetail(const char *ssid)
{
    uint8_t payload[1u + WIFI_SSID_MAX_LEN];
    uint8_t ssid_len;

    if (!ssid || ssid[0] == '\0') {
        return UNER_ERR_LEN;
    }

    ssid_len = (uint8_t)strlen(ssid);
    if (ssid_len == 0u || ssid_len > WIFI_SSID_MAX_LEN) {
        return UNER_ERR_LEN;
    }

    payload[0] = ssid_len;
    (void)memcpy(&payload[1], ssid, ssid_len);

    return UNER_App_SendCommand(UNER_CMD_ID_WIFI_GET_DETAIL,
                                payload,
                                (uint8_t)(ssid_len + 1u));
}

UNER_Status UNER_App_SendEspReboot(uint8_t boot_mode)
{
    uint8_t payload[1u];

    if (boot_mode != UNER_ESP_REBOOT_BOOT_MODE_NORMAL &&
        boot_mode != UNER_ESP_REBOOT_BOOT_MODE_AP) {
        return UNER_ERR_LEN;
    }

    payload[0] = boot_mode;
    return UNER_App_SendCommand(UNER_CMD_ID_REBOOT_ESP,
                                payload,
                                (uint8_t)sizeof(payload));
}


uint8_t UNER_App_IsWaitingValidation(void)
{
    return uner_waiting_validation;
}

uint8_t UNER_App_GetWaitingCommandId(void)
{
    return uner_wait_cmd_id;
}

static void UNER_App_InitConfig(UNER_CoreConfig *cfg)
{
    cfg->this_node = UNER_NODE_MCU;
    cfg->accept_src_mask = 0xFFFFu;
    cfg->accept_dst_mask = (uint16_t)(1u << UNER_NODE_MCU);
    cfg->accept_broadcast = 1u;
    cfg->default_max_payload = 255u;
    cfg->on_packet = NULL;
    cfg->cb_ctx = NULL;
}

void UNER_App_Init(void)
{
    UNER_CoreConfig cfg;
    UNER_App_InitConfig(&cfg);

    (void)UNER_Handle_Init(
        &uner_handle,
        &cfg,
        uner_slots,
        UNER_QUEUE_SLOTS,
        uner_payload_pool,
        sizeof(uner_payload_pool));

    (void)UNER_TransportUart1Dma_Init(
        &uner_uart1,
        UNER_TR_UART1_ESP,
        &huart1,
        &hdma_usart1_rx,
        usart1_rx_dma_buf,
        USART1_RX_DMA_BUF_LEN,
        &usart1_tx_busy);

    (void)UNER_Handle_RegisterTransport(&uner_handle, &uner_uart1.base);
    (void)UNER_Handle_RegisterCommands(
        &uner_handle,
        uner_commands,
        (uint8_t)(sizeof(uner_commands) / sizeof(uner_commands[0])),
        UNER_App_ExecuteCommand,
        NULL);
    (void)HAL_UART_Receive_DMA(&huart1, usart1_rx_dma_buf, USART1_RX_DMA_BUF_LEN);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    UNER_App_ResetUart1RxPos();
}

void UNER_App_Poll(void)
{
    uner_uart1_rx_hint = 0u;

    UNER_Handle_Poll(&uner_handle);
    UNER_Handle_ProcessPending(&uner_handle);

    if (uner_waiting_validation &&
        uner_wait_timeout_ms > 0u &&
        ((uint32_t)(HAL_GetTick() - uner_wait_started_at) >= (uint32_t)uner_wait_timeout_ms)) {
        const uint8_t timed_out_cmd = uner_wait_cmd_id;
        UNER_App_ClearWaitingValidation();

        if (timed_out_cmd == UNER_CMD_BINARY_ASSET_STREAM) {
            UNER_App_ResetBinaryAssetState(binary_asset_id);
        }
    }

    UNER_App_ServiceWifiScanResults();

    MenuSys_FlushScreenReport(&menuSystem);
    MenuSys_FlushMenuSelectionReport(&menuSystem);
}

void UNER_App_OnUart1TxComplete(void)
{
    UNER_TransportUart1Dma_OnTxComplete(&uner_uart1);
}

void UNER_App_NotifyUart1Rx(void)
{
    uner_uart1_rx_hint = 1u;
}

void UNER_App_ResetUart1RxPos(void)
{
    /*
     * Tras rearmar DMA RX, el transporte debe sincronizar su last_pos
     * con la posición actual del DMA para no reprocesar basura vieja.
     */
    uner_uart1.last_pos = UNER_App_Uart1RxGetWritePos();
}

uint16_t UNER_App_Uart1RxGetWritePos(void)
{
    return (uint16_t)(USART1_RX_DMA_BUF_LEN - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx));
}
