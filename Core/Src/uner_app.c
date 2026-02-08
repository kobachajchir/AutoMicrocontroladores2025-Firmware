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
#include "screenWrappers.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;

#define UNER_QUEUE_SLOTS 4u
#define UNER_FLAG_NONE   0u

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
    UNER_CMD_BOOT_COMPLETE = 0x4Fu,
    UNER_CMD_NETWORK_IP = 0x50u,
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
} UNER_CommandId;

typedef enum {
    UF_CMD_SET_MODE_AP = 0,
    UF_CMD_SET_MODE_STA,
    UF_CMD_SET_CREDENTIALS,
    UF_CMD_CLEAR_CREDENTIALS,
    UF_CMD_START_SCAN,
    UF_CMD_GET_SCAN_RESULTS,
    UF_CMD_REBOOT_ESP,
    UF_CMD_FACTORY_RESET,
    UF_CMD_STOP_SCAN,
    UF_CMD_GET_STATUS,
    UF_CMD_PING,
    UF_CMD_GET_PREFERENCES,
    UF_CMD_REQUEST_FIRMWARE,
    UF_CMD_ECHO,
    UF_CMD_SET_ENCODER_FAST,
    UF_CMD_GET_CONNECTED_USERS,
    UF_CMD_GET_USER_INFO,
    UF_CMD_GET_INTERFACES_CONNECTED,
    UF_CMD_GET_CREDENTIALS,
    UF_CMD_CONNECT_WIFI,
    UF_CMD_DISCONNECT_WIFI,
    UF_CMD_SET_AP_CONFIG,
    UF_CMD_START_AP,
    UF_CMD_STOP_AP,
    UF_CMD_GET_CONNECTED_USERS_MODE,
    UF_CMD_SET_AUTO_RECONNECT,
    UF_CMD_COUNT
} UNER_CmdFlagIndex;

#define UNER_CMD_FLAG(idx) (1u << (idx))

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[255u];
    uint8_t valid;
} UNER_CommandSnapshot;

static UNER_Handle uner_handle;
static UNER_TransportUart1Dma uner_uart1;
static UNER_Packet uner_slots[UNER_QUEUE_SLOTS];
static uint8_t uner_payload_pool[UNER_QUEUE_SLOTS * 255u];
static volatile uint8_t uner_uart1_rx_hint = 0;
static volatile uint8_t uner_waiting_validation = 0;
static volatile uint8_t uner_wait_cmd_id = 0;

static volatile uint32_t uner_cmd_flags_pending = 0u;
static volatile uint32_t uner_cmd_flags_inflight = 0u;
static volatile uint8_t uner_cmd_last_sent = 0u;
static UNER_CommandSnapshot uner_snapshots[UF_CMD_COUNT];

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
static void UNER_App_HandleNotificationByPacket(const UNER_Packet *packet);

static const UNER_CommandSpec uner_commands[] = {
    { UNER_CMD_SET_MODE_AP, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_SET_MODE_STA, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 300u, NULL },
    { UNER_CMD_SET_CREDENTIALS, 3u, 255u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
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
    { UNER_CMD_ECHO, 4u, 4u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_SET_ENCODER_FAST, 1u, 1u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_GET_CONNECTED_USERS, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_GET_USER_INFO, 1u, 1u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 120u, NULL },
    { UNER_CMD_GET_INTERFACES_CONNECTED, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_GET_CREDENTIALS, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 120u, NULL },
    { UNER_CMD_CONNECT_WIFI, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 400u, NULL },
    { UNER_CMD_DISCONNECT_WIFI, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, NULL },
    { UNER_CMD_SET_AP_CONFIG, 2u, 255u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 120u, NULL },
    { UNER_CMD_START_AP, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 220u, NULL },
    { UNER_CMD_STOP_AP, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 120u, NULL },
    { UNER_CMD_GET_CONNECTED_USERS_MODE, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_SET_AUTO_RECONNECT, 1u, 1u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_BOOT_COMPLETE, 0u, 255u, UNER_SPEC_F_EVT, 0u, NULL },
    { UNER_CMD_NETWORK_IP, 5u, 5u, UNER_SPEC_F_EVT, 0u, NULL },
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
    { UNER_EVT_NETWORK_IP, 5u, 5u, UNER_SPEC_F_EVT, 0u, NULL },
    { UNER_EVT_BOOT_COMPLETE, 5u, 5u, UNER_SPEC_F_EVT, 0u, NULL },
};

static const UNER_CommandSpec *UNER_App_FindSpecById(uint8_t cmd)
{
    for (uint8_t i = 0u; i < (uint8_t)(sizeof(uner_commands) / sizeof(uner_commands[0])); ++i) {
        if (uner_commands[i].id == cmd) {
            return &uner_commands[i];
        }
    }
    return NULL;
}

static uint32_t UNER_App_FlagFromCmd(uint8_t cmd)
{
    switch (cmd) {
    case UNER_CMD_SET_MODE_AP: return UNER_CMD_FLAG(UF_CMD_SET_MODE_AP);
    case UNER_CMD_SET_MODE_STA: return UNER_CMD_FLAG(UF_CMD_SET_MODE_STA);
    case UNER_CMD_SET_CREDENTIALS: return UNER_CMD_FLAG(UF_CMD_SET_CREDENTIALS);
    case UNER_CMD_CLEAR_CREDENTIALS: return UNER_CMD_FLAG(UF_CMD_CLEAR_CREDENTIALS);
    case UNER_CMD_START_SCAN: return UNER_CMD_FLAG(UF_CMD_START_SCAN);
    case UNER_CMD_GET_SCAN_RESULTS: return UNER_CMD_FLAG(UF_CMD_GET_SCAN_RESULTS);
    case UNER_CMD_REBOOT_ESP: return UNER_CMD_FLAG(UF_CMD_REBOOT_ESP);
    case UNER_CMD_FACTORY_RESET: return UNER_CMD_FLAG(UF_CMD_FACTORY_RESET);
    case UNER_CMD_STOP_SCAN: return UNER_CMD_FLAG(UF_CMD_STOP_SCAN);
    case UNER_CMD_GET_STATUS: return UNER_CMD_FLAG(UF_CMD_GET_STATUS);
    case UNER_CMD_PING: return UNER_CMD_FLAG(UF_CMD_PING);
    case UNER_CMD_GET_PREFERENCES: return UNER_CMD_FLAG(UF_CMD_GET_PREFERENCES);
    case UNER_CMD_REQUEST_FIRMWARE: return UNER_CMD_FLAG(UF_CMD_REQUEST_FIRMWARE);
    case UNER_CMD_ECHO: return UNER_CMD_FLAG(UF_CMD_ECHO);
    case UNER_CMD_SET_ENCODER_FAST: return UNER_CMD_FLAG(UF_CMD_SET_ENCODER_FAST);
    case UNER_CMD_GET_CONNECTED_USERS: return UNER_CMD_FLAG(UF_CMD_GET_CONNECTED_USERS);
    case UNER_CMD_GET_USER_INFO: return UNER_CMD_FLAG(UF_CMD_GET_USER_INFO);
    case UNER_CMD_GET_INTERFACES_CONNECTED: return UNER_CMD_FLAG(UF_CMD_GET_INTERFACES_CONNECTED);
    case UNER_CMD_GET_CREDENTIALS: return UNER_CMD_FLAG(UF_CMD_GET_CREDENTIALS);
    case UNER_CMD_CONNECT_WIFI: return UNER_CMD_FLAG(UF_CMD_CONNECT_WIFI);
    case UNER_CMD_DISCONNECT_WIFI: return UNER_CMD_FLAG(UF_CMD_DISCONNECT_WIFI);
    case UNER_CMD_SET_AP_CONFIG: return UNER_CMD_FLAG(UF_CMD_SET_AP_CONFIG);
    case UNER_CMD_START_AP: return UNER_CMD_FLAG(UF_CMD_START_AP);
    case UNER_CMD_STOP_AP: return UNER_CMD_FLAG(UF_CMD_STOP_AP);
    case UNER_CMD_GET_CONNECTED_USERS_MODE: return UNER_CMD_FLAG(UF_CMD_GET_CONNECTED_USERS_MODE);
    case UNER_CMD_SET_AUTO_RECONNECT: return UNER_CMD_FLAG(UF_CMD_SET_AUTO_RECONNECT);
    default: return UNER_FLAG_NONE;
    }
}

static uint8_t UNER_App_CmdFromFlag(uint32_t flag)
{
    for (uint16_t cmd = 0u; cmd <= 0xFFu; ++cmd) {
        if (UNER_App_FlagFromCmd((uint8_t)cmd) == flag) {
            return (uint8_t)cmd;
        }
    }
    return 0u;
}

static void UNER_App_CmdFlag_Set(uint32_t mask)
{
    __disable_irq();
    uner_cmd_flags_pending |= mask;
    __enable_irq();
}

static void UNER_App_CmdFlag_Clear(uint32_t mask)
{
    __disable_irq();
    uner_cmd_flags_pending &= ~mask;
    uner_cmd_flags_inflight &= ~mask;
    __enable_irq();
}

static uint32_t UNER_App_CmdFlag_GetPending(void)
{
    uint32_t flags;
    __disable_irq();
    flags = uner_cmd_flags_pending;
    __enable_irq();
    return flags;
}

static uint32_t UNER_App_PickNextFlag(void)
{
    static const uint32_t prio[] = {
        UNER_CMD_FLAG(UF_CMD_CONNECT_WIFI),
        UNER_CMD_FLAG(UF_CMD_DISCONNECT_WIFI),
        UNER_CMD_FLAG(UF_CMD_START_AP),
        UNER_CMD_FLAG(UF_CMD_STOP_AP),
        UNER_CMD_FLAG(UF_CMD_SET_MODE_AP),
        UNER_CMD_FLAG(UF_CMD_SET_MODE_STA),
        UNER_CMD_FLAG(UF_CMD_START_SCAN),
        UNER_CMD_FLAG(UF_CMD_STOP_SCAN),
        UNER_CMD_FLAG(UF_CMD_GET_SCAN_RESULTS),
        UNER_CMD_FLAG(UF_CMD_GET_STATUS),
        UNER_CMD_FLAG(UF_CMD_GET_CONNECTED_USERS_MODE),
        UNER_CMD_FLAG(UF_CMD_GET_CONNECTED_USERS),
        UNER_CMD_FLAG(UF_CMD_GET_INTERFACES_CONNECTED),
        UNER_CMD_FLAG(UF_CMD_GET_USER_INFO),
        UNER_CMD_FLAG(UF_CMD_GET_PREFERENCES),
        UNER_CMD_FLAG(UF_CMD_GET_CREDENTIALS),
        UNER_CMD_FLAG(UF_CMD_SET_CREDENTIALS),
        UNER_CMD_FLAG(UF_CMD_SET_AP_CONFIG),
        UNER_CMD_FLAG(UF_CMD_SET_AUTO_RECONNECT),
        UNER_CMD_FLAG(UF_CMD_REQUEST_FIRMWARE),
        UNER_CMD_FLAG(UF_CMD_SET_ENCODER_FAST),
        UNER_CMD_FLAG(UF_CMD_ECHO),
        UNER_CMD_FLAG(UF_CMD_FACTORY_RESET),
        UNER_CMD_FLAG(UF_CMD_REBOOT_ESP),
        UNER_CMD_FLAG(UF_CMD_PING)
    };

    uint32_t pending = UNER_App_CmdFlag_GetPending();
    for (uint8_t i = 0u; i < (uint8_t)(sizeof(prio) / sizeof(prio[0])); ++i) {
        if ((pending & prio[i]) != 0u) {
            return prio[i];
        }
    }
    return 0u;
}

static UNER_CommandSnapshot *UNER_App_GetSnapshotByCmd(uint8_t cmd)
{
    uint32_t flag = UNER_App_FlagFromCmd(cmd);
    if (flag == 0u) {
        return NULL;
    }

    for (uint8_t i = 0u; i < UF_CMD_COUNT; ++i) {
        if (flag == UNER_CMD_FLAG(i)) {
            return &uner_snapshots[i];
        }
    }
    return NULL;
}

static uint8_t UNER_App_IsEventCmd(uint8_t cmd)
{
    return (cmd >= UNER_EVT_BOOT && cmd <= UNER_EVT_BOOT_COMPLETE) ? 1u : 0u;
}

static void UNER_App_ShowNotification(const char *line1, const char *line2)
{
    OledUtils_SetUNERNotificationText(line1, line2);
    OledUtils_ShowNotificationMs(OledUtils_RenderUNERNotification, 2500u);
}

static void UNER_App_HandleNotificationByPacket(const UNER_Packet *packet)
{
    if (!packet) {
        return;
    }

    char l1[22];
    char l2[22];
    const uint8_t status = (packet->len > 0u && packet->payload) ? packet->payload[0] : 0xFFu;

    switch (packet->cmd) {
    case UNER_CMD_ACK:
    case UNER_CMD_NACK:
        if (packet->len >= 2u && packet->payload) {
            (void)snprintf(l1, sizeof(l1), "%s 0x%02X", (packet->cmd == UNER_CMD_ACK) ? "ACK" : "NACK", packet->payload[0]);
            (void)snprintf(l2, sizeof(l2), "code 0x%02X", packet->payload[1]);
            UNER_App_ShowNotification(l1, l2);
        }
        break;
    case UNER_EVT_BOOT:
        SET_FLAG(systemFlags2, ESP_PRESENT);
        UNER_App_ShowNotification("ESP conectada", "Evento BOOT");
        break;
    case UNER_CMD_GET_SCAN_RESULTS:
        if (packet->len >= 1u && packet->payload && packet->payload[0] == 0xFEu) {
            CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
            OledUtils_ShowNotificationMs(OledUtils_RenderWiFiSearchCompleteNotification, 1500u);
            (void)UNER_App_SendCommand(UNER_CMD_GET_SCAN_RESULTS, NULL, 0u);
            break;
        }
        if (status == 1u) {
            UNER_App_ShowNotification("Scan en curso", "espere...");
        } else if (status == 2u) {
            CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
            OledUtils_ShowNotificationMs(OledUtils_RenderWiFiSearchCanceledNotification, 1500u);
        } else if (packet->len >= 2u && packet->payload) {
            networksFound = packet->payload[1];
            (void)snprintf(l2, sizeof(l2), "redes: %u", networksFound);
            UNER_App_ShowNotification("Scan listo", l2);
            menuSystem.renderFn = OledUtils_RenderWiFiSearchResults_Wrapper;
            menuSystem.renderFlag = true;
            oled_first_draw = false;
        }
        break;
    case UNER_CMD_CONNECT_WIFI:
        if (packet->len >= 5u && packet->payload && packet->payload[0] == 0xFEu) {
            (void)snprintf(l2, sizeof(l2), "IP %u.%u.%u.%u", packet->payload[1], packet->payload[2], packet->payload[3], packet->payload[4]);
            UNER_App_ShowNotification("STA conectada", l2);
        } else {
            UNER_App_ShowNotification("CONNECT_WIFI", (status == 0u) ? "intentando" : "error");
        }
        break;
    case UNER_CMD_START_AP:
        if (packet->len >= 5u && packet->payload && packet->payload[0] == 0xFEu) {
            (void)snprintf(l2, sizeof(l2), "AP %u.%u.%u.%u", packet->payload[1], packet->payload[2], packet->payload[3], packet->payload[4]);
            UNER_App_ShowNotification("AP listo", l2);
        } else {
            UNER_App_ShowNotification("START_AP", (status == 0u) ? "ok" : "error");
        }
        break;
    case UNER_CMD_GET_STATUS:
        if (packet->len >= 8u && packet->payload && status == 0u) {
            (void)snprintf(l2, sizeof(l2), "M:%u WL:%u", packet->payload[3], packet->payload[4]);
            UNER_App_ShowNotification("Estado WiFi", l2);
        }
        break;
    case UNER_CMD_PING:
        UNER_App_ShowNotification((status == 0u) ? "Conexion OK" : "Conexion fallo", "PING");
        break;
    case UNER_CMD_REQUEST_FIRMWARE:
        UNER_App_ShowNotification((status == 0u) ? "Firmware recibido" : "Firmware error", "CMD 0x41");
        break;
    case UNER_CMD_REBOOT_ESP:
        UNER_App_ShowNotification((status == 0u) ? "Reset enviado" : "Reset error", "CMD 0x16");
        break;
    case UNER_CMD_NETWORK_IP:
    case UNER_EVT_NETWORK_IP:
    case UNER_CMD_BOOT_COMPLETE:
    case UNER_EVT_BOOT_COMPLETE:
        if (packet->len >= 5u && packet->payload) {
            (void)snprintf(l2, sizeof(l2), "%u.%u.%u.%u", packet->payload[1], packet->payload[2], packet->payload[3], packet->payload[4]);
            UNER_App_ShowNotification((packet->payload[0] == 0x02u) ? "AP IP" : "STA IP", l2);
            SET_FLAG(systemFlags2, ESP_PRESENT);
        }
        break;
    case UNER_EVT_STA_CONNECTED:
        UNER_App_ShowNotification("Evento STA", "Conectada");
        break;
    case UNER_EVT_STA_DISCONNECTED:
        UNER_App_ShowNotification("Evento STA", "Desconectada");
        break;
    case UNER_EVT_AP_CLIENT_JOIN:
    case UNER_EVT_AP_CLIENT_LEAVE:
        if (packet->len >= 1u && packet->payload) {
            (void)snprintf(l2, sizeof(l2), "Total: %u", packet->payload[0]);
            UNER_App_ShowNotification((packet->cmd == UNER_EVT_AP_CLIENT_JOIN) ? "Cliente AP entro" : "Cliente AP salio", l2);
        }
        break;
    case UNER_EVT_WIFI_CONNECTED:
        UNER_App_ShowNotification("WiFi conectada", "Credenciales aplicadas");
        break;
    default:
        if (!UNER_App_IsEventCmd(packet->cmd)) {
            (void)snprintf(l1, sizeof(l1), "CMD 0x%02X", packet->cmd);
            (void)snprintf(l2, sizeof(l2), "status 0x%02X", status);
            UNER_App_ShowNotification(l1, l2);
        }
        break;
    }
}

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
            UNER_App_CmdFlag_Clear(UNER_App_FlagFromCmd(packet->payload[0]));
        }
        UNER_App_HandleNotificationByPacket(packet);
        return;
    }

    if (uner_waiting_validation && packet->cmd == uner_wait_cmd_id) {
        uner_waiting_validation = 0u;
        uner_wait_cmd_id = 0u;
        UNER_App_CmdFlag_Clear(UNER_App_FlagFromCmd(packet->cmd));
    }

    if (packet->cmd == UNER_CMD_SET_ENCODER_FAST && packet->len >= 2u && packet->payload) {
        encoder_fast_scroll_enabled = (packet->payload[1] != 0u) ? 1u : 0u;
    }

    if (packet->cmd == UNER_CMD_ECHO) {
        OledUtils_ShowNotificationMs(OledUtils_RenderCommandReceivedNotification, 2000u);
    }

    UNER_App_HandleNotificationByPacket(packet);
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

    UNER_CommandSnapshot *snap = UNER_App_GetSnapshotByCmd(cmd);
    if (!snap) {
        return UNER_ERR_LEN;
    }

    snap->cmd = cmd;
    snap->len = len;
    snap->valid = 1u;

    if (len > 0u) {
        if (!payload) {
            return UNER_ERR_LEN;
        }
        memcpy(snap->payload, payload, len);
    }

    UNER_App_CmdFlag_Set(UNER_App_FlagFromCmd(cmd));
    return UNER_OK;
}

uint8_t UNER_App_CmdScheduler_TrySendNext(void)
{
    if (uner_cmd_flags_inflight != 0u || uner_waiting_validation) {
        return 0u;
    }

    uint32_t next_flag = UNER_App_PickNextFlag();
    if (next_flag == 0u) {
        return 0u;
    }

    uint8_t cmd = UNER_App_CmdFromFlag(next_flag);
    UNER_CommandSnapshot *snap = UNER_App_GetSnapshotByCmd(cmd);
    const UNER_CommandSpec *spec = UNER_App_FindSpecById(cmd);

    if (!snap || !snap->valid || !spec) {
        UNER_App_CmdFlag_Clear(next_flag);
        return 0u;
    }

    uint8_t dst = UNER_NODE_ESP01;
    if (spec->flags & UNER_SPEC_F_EVT) {
        dst = UNER_NODE_BROADCAST;
    }

    UNER_Status st = UNER_Send(&uner_handle,
                               UNER_TR_UART1_ESP,
                               UNER_NODE_MCU,
                               dst,
                               cmd,
                               (snap->len > 0u) ? snap->payload : NULL,
                               snap->len);

    if (st == UNER_OK) {
        uner_cmd_flags_inflight = next_flag;
        uner_cmd_last_sent = cmd;
        if (spec->flags & (UNER_SPEC_F_ACK | UNER_SPEC_F_RESP)) {
            uner_waiting_validation = 1u;
            uner_wait_cmd_id = cmd;
        }
        return 1u;
    }

    return 0u;
}

uint8_t UNER_App_IsWaitingValidation(void)
{
    return uner_waiting_validation;
}

uint8_t UNER_App_GetWaitingCommandId(void)
{
    return uner_wait_cmd_id;
}

uint32_t UNER_App_GetPendingFlags(void)
{
    return UNER_App_CmdFlag_GetPending();
}

uint32_t UNER_App_GetInFlightFlags(void)
{
    return uner_cmd_flags_inflight;
}

void UNER_App_GetFlagBitmap(Byte_Flag_Struct *pending_low,
                            Byte_Flag_Struct *pending_high,
                            Byte_Flag_Struct *inflight_low,
                            Byte_Flag_Struct *inflight_high)
{
    uint32_t pending = UNER_App_CmdFlag_GetPending();
    uint32_t inflight = uner_cmd_flags_inflight;

    if (pending_low) {
        pending_low->byte = (uint8_t)(pending & 0xFFu);
    }
    if (pending_high) {
        pending_high->byte = (uint8_t)((pending >> 8) & 0xFFu);
    }
    if (inflight_low) {
        inflight_low->byte = (uint8_t)(inflight & 0xFFu);
    }
    if (inflight_high) {
        inflight_high->byte = (uint8_t)((inflight >> 8) & 0xFFu);
    }
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

    memset(uner_snapshots, 0, sizeof(uner_snapshots));
    uner_cmd_flags_pending = 0u;
    uner_cmd_flags_inflight = 0u;
    uner_cmd_last_sent = 0u;

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
    (void)UNER_App_CmdScheduler_TrySendNext();
}

void UNER_App_OnUart1TxComplete(void)
{
    UNER_TransportUart1Dma_OnTxComplete(&uner_uart1);
}

void UNER_App_NotifyUart1Rx(void)
{
    uner_uart1_rx_hint = 1u;
}

static void evt_boot_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; }
static void evt_mode_changed_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    if (!p || p->len == 0u || !p->payload) {
        return;
    }

    const uint8_t mode = p->payload[0];
    if (mode == UNER_CMD_SET_MODE_AP) {
        SET_FLAG(systemFlags2, AP_ACTIVE);
        CLEAR_FLAG(systemFlags2, WIFI_ACTIVE);
    } else if (mode == UNER_CMD_SET_MODE_STA) {
        CLEAR_FLAG(systemFlags2, AP_ACTIVE);
    }

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

static void evt_ap_client_join_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
}

static void evt_ap_client_leave_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
}

static void evt_webserver_up_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; }
static void evt_webserver_client_conn_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; }
static void evt_webserver_client_disconn_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; }
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
}

static void evt_usb_disconnected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    (void)p;
    CLEAR_FLAG(systemFlags2, USB_ACTIVE);
}

static void evt_wifi_connected_handler(void *ctx, const UNER_Packet *p)
{
    (void)ctx;
    if (p && p->len >= 3u && p->payload) {
        char l2[22];
        uint8_t ssid_len = p->payload[1];
        uint8_t max_copy = (p->len > 2u) ? (uint8_t)(p->len - 2u) : 0u;
        if (ssid_len > max_copy) {
            ssid_len = max_copy;
        }
        if (ssid_len > 10u) {
            ssid_len = 10u;
        }
        memcpy(l2, &p->payload[2], ssid_len);
        l2[ssid_len] = '\0';
    }
}
