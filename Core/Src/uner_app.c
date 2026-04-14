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

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;

#define UNER_QUEUE_SLOTS 6u
#define WIFI_ACTIVE_CODE 1
#define AP_ACTIVE_CODE 2

static UNER_Handle uner_handle;
static UNER_TransportUart1Dma uner_uart1;
static UNER_Packet uner_slots[UNER_QUEUE_SLOTS];
static volatile uint8_t uner_payload_pool[UNER_QUEUE_SLOTS * 255u];
static volatile uint8_t uner_tx_marked = 0;
static volatile uint8_t uner_waiting_validation = 0;
static volatile uint8_t uner_wait_cmd_id = 0;
static volatile uint32_t uner_wait_started_at = 0u;
static volatile uint16_t uner_wait_timeout_ms = 0u;

static volatile uint8_t lastAckCmd = 0u;
static volatile uint8_t lastAckStatus = 0u;

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
static void UNER_App_StoreWifiInfoSsid(const uint8_t *payload, uint8_t len);
static void UNER_App_StoreTargetCredentialsSsid(const uint8_t *payload, uint8_t len);
static void UNER_App_StoreNetworkIpPayload(const uint8_t *payload, uint8_t len);
static uint8_t UNER_App_IsStaActive(void);
static void UNER_App_MarkStaActive(void);
static void UNER_App_ShowWifiConnectedNotification(uint8_t was_wifi_active);
static void UNER_App_RequestNetworkUiRefresh(void);
static void UNER_App_WriteScreenReportPayload(uint8_t *payload, uint32_t screen_code, uint8_t source);

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

static const UNER_CommandSpec uner_commands[] = {
    { UNER_CMD_SET_MODE_AP, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_SET_MODE_STA, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 300u, NULL },
    { UNER_CMD_SET_CREDENTIALS, 0u, 255u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_CLEAR_CREDENTIALS, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_START_SCAN, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, NULL },
    { UNER_CMD_GET_SCAN_RESULTS, 0u, 0u, UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_REBOOT_ESP, 0u, 0u, UNER_SPEC_F_ACK, 50u, NULL },
    { UNER_CMD_FACTORY_RESET, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, NULL },
    { UNER_CMD_STOP_SCAN, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_RESET_MCU, 0u, 0u, 0u, 0u, cmd_reset_mcu_handler },
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
    { UNER_CMD_AUTH_VALIDATE_PIN, 6u, 6u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 500u, NULL },
    { UNER_CMD_GET_CURRENT_SCREEN, 0u, 0u, UNER_SPEC_F_RESP, 50u, cmd_get_current_screen_handler },
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
    { UNER_CMD_ECHO, 4u, 4u, 0u, 0u, NULL },
};

static void UNER_App_ExecuteCommand(void *ctx, const UNER_Packet *packet)
{
    (void)ctx;
    if (!packet) {
        return;
    }

    if (packet->cmd == UNER_CMD_ACK || packet->cmd == UNER_CMD_NACK) {
        if (packet->len >= 1u && packet->payload && packet->payload[0] == uner_wait_cmd_id) {
            uner_waiting_validation = 0u;
            uner_wait_cmd_id = 0u;
            uner_wait_started_at = 0u;
            uner_wait_timeout_ms = 0u;
            __NOP();
        }
        return;
    }

    if (uner_waiting_validation && packet->cmd == uner_wait_cmd_id) {
        uner_waiting_validation = 0u;
        uner_wait_cmd_id = 0u;
        uner_wait_started_at = 0u;
        uner_wait_timeout_ms = 0u;
        __NOP();
    }

    if (packet->cmd == UNER_CMD_ECHO) {
    	OledUtils_DismissNotification();
        OledUtils_ShowNotificationMs(OledUtils_RenderCommandReceivedNotification, 2000u);
        __NOP();
    }

    if (packet->cmd == UNER_CMD_PING) {
        SET_FLAG(systemFlags2, ESP_PRESENT);
        OledUtils_DismissNotification();
        OledUtils_ShowNotificationMs(OledUtils_RenderPingReceivedNotification, 2000u);
        __NOP();
    }

    if (packet->cmd == UNER_CMD_SET_ENCODER_FAST && packet->len >= 2u && packet->payload) {
        encoder_fast_scroll_enabled = (packet->payload[1] != 0u) ? 1u : 0u;
    }

    if (packet->cmd == UNER_CMD_REQUEST_FIRMWARE) {
    	__NOP();
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
		__NOP();
        UNER_App_ParseStatusResponse(packet);
    }

    if (packet->cmd == UNER_CMD_CONNECT_WIFI || packet->cmd == UNER_CMD_START_AP) {
    	__NOP();
        UNER_App_ParseWifiFinalizer(packet);
    }

    if (packet->cmd == UNER_CMD_SET_ENCODER_FAST && packet->len >= 1u && packet->payload) {
        encoder_fast_scroll_enabled = (packet->payload[0] != 0u) ? 1u : 0u;
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
        OledUtils_IsNotificationShowing(OledUtils_RenderESPCheckingConnectionNotification)) {
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

    if (OledUtils_IsNotificationShowing(OledUtils_RenderESPCheckingConnectionNotification)) {
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
        OledUtils_DismissNotification();
        OledUtils_ShowNotificationMs(OledUtils_RenderESPAPStartedNotification, 2500u);
        UNER_App_RequestNetworkUiRefresh();
    }
}

static void UNER_App_ShowScanResultsScreen(void)
{
    wifiScanSessionActive = 0u;
    wifiScanResultsPending = 1u;
    CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
    wifiSearchingTimeout = 0u;

    menuSystem.currentMenu = &wifiResultsMenu;
    menuSystem.renderFn = OledUtils_RenderWiFiSearchResults_Wrapper;
    menuSystem.renderFlag = true;
    oled_first_draw = false;
}

static void UNER_App_ParseScanResultsResponse(const UNER_Packet *p)
{
    if (!p || !p->payload || p->len < 2u) {
        return;
    }

    const uint8_t status = p->payload[0];
    const uint8_t reportedCount = p->payload[1];
    uint8_t offset = 2u;

    networksFound = 0u;
    memset(wifiNetworkSsids, 0, sizeof(wifiNetworkSsids));

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

            offset = (uint8_t)(offset + ssidLen);
            networksFound++;
        }
    }

    if (status != 1u) {
        wifiSearchingTimeout = 0u;
        CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
        wifiScanSessionActive = 0u;
        wifiScanResultsPending = 0u;
        menuSystem.currentMenu = &wifiResultsMenu;
        menuSystem.renderFn = OledUtils_RenderWiFiSearchResults_Wrapper;
        menuSystem.renderFlag = true;
        oled_first_draw = false;
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


static void evt_boot_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    espBootRebootPendingResponse = 0;
    SET_FLAG(systemFlags2, ESP_PRESENT);
    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(OledUtils_RenderESPBootReceivedNotification, 2000u);
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

    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(OledUtils_RenderESPModeChangedNotification, 2200u);
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

static void evt_ap_client_join_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_ap_client_leave_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_webserver_up_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(OledUtils_RenderESPWebServerUpNotification, 2000u);
}
static void evt_webserver_client_conn_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(OledUtils_RenderESPWebClientConnectedNotification, 2000u);
}

static void evt_webserver_client_disconn_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(OledUtils_RenderESPWebClientDisconnectedNotification, 2000u);
}
static void evt_lastwifi_notfound_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_app_mpu_readings_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_app_tcrt_readings_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_app_user_connected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_app_user_disconnected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_controller_connected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    SET_FLAG(systemFlags2, RF_ACTIVE);
    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(OledUtils_RenderControllerConnected, 2000u);
}

static void evt_controller_disconnected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    CLEAR_FLAG(systemFlags2, RF_ACTIVE);
    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(OledUtils_RenderControllerDisconnected, 2000u);
}

static void evt_usb_connected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    //SET_FLAG(systemFlags2, USB_ACTIVE);
    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(OledUtils_RenderESPUsbConnectedNotification, 2000u);
}

static void evt_usb_disconnected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    //CLEAR_FLAG(systemFlags2, USB_ACTIVE);
    //OledUtils_DismissNotification();
    //OledUtils_ShowNotificationMs(OledUtils_RenderESPUsbDisconnectedNotification, 2000u);
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

static void cmd_request_firmware_handler(void *ctx, const UNER_Packet *packet)
{
	__NOP();
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
    uint8_t ack_status = (packet->len >= 2u) ? packet->payload[1] : 0u;

    lastAckCmd = acked_cmd;
    lastAckStatus = ack_status;

    /* 🔹 Integración con sistema de validación */
    if (uner_waiting_validation && uner_wait_cmd_id == acked_cmd) {
        uner_waiting_validation = 0u;
        uner_wait_cmd_id = 0u;
        uner_wait_started_at = 0u;
        uner_wait_timeout_ms = 0u;
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

    __NOP();

    uint8_t dst = UNER_NODE_PC_QT;
    if (spec->flags & UNER_SPEC_F_EVT) {
        dst = UNER_NODE_BROADCAST;
    }

    UNER_Status status = UNER_Send(&uner_handle, UNER_TR_UART1_ESP, UNER_NODE_MCU, dst, cmd, payload, len);
    if (status == UNER_OK) {
        uner_tx_marked = 1u;
        if (spec->flags & (UNER_SPEC_F_ACK | UNER_SPEC_F_RESP)) {
            uner_waiting_validation = 1u;
            uner_wait_cmd_id = cmd;
            uner_wait_started_at = HAL_GetTick();
            uner_wait_timeout_ms = spec->timeout_ms;
        } else {
            uner_waiting_validation = 0u;
            uner_wait_cmd_id = 0u;
            uner_wait_started_at = 0u;
            uner_wait_timeout_ms = 0u;
        }

        if (cmd == UNER_CMD_CONNECT_WIFI) {
            OledUtils_DismissNotification();
            OledUtils_ShowNotificationMs(OledUtils_RenderESPWifiConnectingNotification, 5000u);
        }
    }
    __NOP();
    __NOP();

    return status;
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
    /* El hint puede limpiarse aquí; Poll ya corre siempre */
    uner_uart1_rx_hint = 0u;

    UNER_Handle_Poll(&uner_handle);
    UNER_Handle_ProcessPending(&uner_handle);

    if (uner_waiting_validation &&
        uner_wait_timeout_ms > 0u &&
        ((uint32_t)(HAL_GetTick() - uner_wait_started_at) >= (uint32_t)uner_wait_timeout_ms)) {
        uner_waiting_validation = 0u;
        uner_wait_cmd_id = 0u;
        uner_wait_started_at = 0u;
        uner_wait_timeout_ms = 0u;
    }

    MenuSys_FlushScreenReport(&menuSystem);
}

void UNER_App_OnUart1TxComplete(void)
{
    __NOP();
    uner_tx_marked = 0u;
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
