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
#include "stm32f1xx_hal.h"

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;

#define UNER_QUEUE_SLOTS 4u
#define WIFI_ACTIVE_CODE 1
#define AP_ACTIVE_CODE 2

static UNER_Handle uner_handle;
static UNER_TransportUart1Dma uner_uart1;
static UNER_Packet uner_slots[UNER_QUEUE_SLOTS];
static uint8_t uner_payload_pool[UNER_QUEUE_SLOTS * 255u];
static volatile uint8_t uner_uart1_rx_hint = 0;
static volatile uint8_t uner_tx_marked = 0;
static volatile uint8_t uner_waiting_validation = 0;
static volatile uint8_t uner_wait_cmd_id = 0;

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
    { UNER_CMD_GET_STATUS, 0u, 0u, UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_PING, 0u, 0u, UNER_SPEC_F_RESP, 50u, NULL },
    { UNER_CMD_GET_PREFERENCES, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_REQUEST_FIRMWARE, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, NULL },
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
    { UNER_CMD_ACK, 1u, 2u, 0u, 0u, NULL },
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
            __NOP();
        }
        return;
    }

    if (uner_waiting_validation && packet->cmd == uner_wait_cmd_id) {
        uner_waiting_validation = 0u;
        uner_wait_cmd_id = 0u;
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

static void UNER_App_ParseFirmwareResponse(const UNER_Packet *p)
{
    // Formato real response: [status, ascii_fw...]
    if (!p || !p->payload || p->len < 2u) {
        return;
    }

    if (p->payload[0] != 0u) { // status != OK
        return;
    }

    uint8_t fw_len = (uint8_t)(p->len - 1u);
    if (fw_len > 32u) {
        fw_len = 32u;
    }

    (void)memcpy(espFirmwareVersion, &p->payload[1], fw_len);
    espFirmwareVersion[fw_len] = '\0';
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

    // IPs finales (robusto: tomadas desde el tail)
    espStaIp.block1 = p->payload[p->len - 8u];
    espStaIp.block2 = p->payload[p->len - 7u];
    espStaIp.block3 = p->payload[p->len - 6u];
    espStaIp.block4 = p->payload[p->len - 5u];

    espApIp.block1 = p->payload[p->len - 4u];
    espApIp.block2 = p->payload[p->len - 3u];
    espApIp.block3 = p->payload[p->len - 2u];
    espApIp.block4 = p->payload[p->len - 1u];
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
        espStaIp.block1 = p->payload[1];
        espStaIp.block2 = p->payload[2];
        espStaIp.block3 = p->payload[3];
        espStaIp.block4 = p->payload[4];

        SET_FLAG(systemFlags2, WIFI_ACTIVE);
        OledUtils_DismissNotification();
        OledUtils_ShowNotificationMs(OledUtils_RenderESPWifiConnectedNotification, 2500u);
    }
    else if (p->cmd == UNER_CMD_START_AP) {
        espApIp.block1 = p->payload[1];
        espApIp.block2 = p->payload[2];
        espApIp.block3 = p->payload[3];
        espApIp.block4 = p->payload[4];

        SET_FLAG(systemFlags2, AP_ACTIVE);
        OledUtils_DismissNotification();
        OledUtils_ShowNotificationMs(OledUtils_RenderESPAPStartedNotification, 2500u);
    }
}


static void evt_boot_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
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

    if (p->len >= 6u) {
        // IP reportada por el wifi_info del evento
        espStaIp.block1 = p->payload[2];
        espStaIp.block2 = p->payload[3];
        espStaIp.block3 = p->payload[4];
        espStaIp.block4 = p->payload[5];

        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_AP_STA) {
            espApIp = espStaIp;
        }
    }

    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(OledUtils_RenderESPModeChangedNotification, 2200u);
}

static void evt_sta_connected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    SET_FLAG(systemFlags2, WIFI_ACTIVE);
    CLEAR_FLAG(systemFlags2, AP_ACTIVE);
}

static void evt_sta_disconnected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    CLEAR_FLAG(systemFlags2, WIFI_ACTIVE);
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
static void evt_webserver_client_conn_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_webserver_client_disconn_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
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
    OledUtils_ShowNotificationMs(OledUtils_RenderControllerConnected, 2000u);
}

static void evt_controller_disconnected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    CLEAR_FLAG(systemFlags2, RF_ACTIVE);
    OledUtils_ShowNotificationMs(OledUtils_RenderControllerDisconnected, 2000u);
}

static void evt_usb_connected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    SET_FLAG(systemFlags2, USB_ACTIVE);
    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(OledUtils_RenderESPUsbConnectedNotification, 2000u);
}

static void evt_usb_disconnected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    CLEAR_FLAG(systemFlags2, USB_ACTIVE);
    OledUtils_DismissNotification();
    OledUtils_ShowNotificationMs(OledUtils_RenderESPUsbDisconnectedNotification, 2000u);
}

static void evt_wifi_connected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    SET_FLAG(systemFlags2, WIFI_ACTIVE);
    CLEAR_FLAG(systemFlags2, AP_ACTIVE);
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

    __NOP();
    uner_tx_marked = 1u;
    if (spec->flags & (UNER_SPEC_F_ACK | UNER_SPEC_F_RESP)) {
        uner_waiting_validation = 1u;
        uner_wait_cmd_id = cmd;
    } else {
        uner_waiting_validation = 0u;
        uner_wait_cmd_id = 0u;
    }
    __NOP();
    __NOP();

    return UNER_Send(&uner_handle, UNER_TR_UART1_ESP, UNER_NODE_MCU, dst, cmd, payload, len);
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
    usart1_feed_pending = 0;
    usart1_rx_prev_pos = 0;
    (void)HAL_UART_Receive_DMA(&huart1, usart1_rx_dma_buf, USART1_RX_DMA_BUF_LEN);
}

void UNER_App_Poll(void)
{
    if (uner_uart1_rx_hint) {
        uner_uart1_rx_hint = 0;
    }
    UNER_Handle_Poll(&uner_handle);
    UNER_Handle_ProcessPending(&uner_handle);
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
    __NOP();
}

uint16_t UNER_App_Uart1RxGetWritePos(void)
{
    return (uint16_t)(USART1_RX_DMA_BUF_LEN - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx));
}
