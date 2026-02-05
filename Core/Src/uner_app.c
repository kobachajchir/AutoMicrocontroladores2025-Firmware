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

static UNER_Handle uner_handle;
static UNER_TransportUart1Dma uner_uart1;
static UNER_Packet uner_slots[UNER_QUEUE_SLOTS];
static uint8_t uner_payload_pool[UNER_QUEUE_SLOTS * 255u];
static volatile uint8_t uner_uart1_rx_hint = 0;
static volatile uint8_t uner_tx_marked = 0;

typedef enum {
    UNER_CMD_SET_MODE_AP = 0x10u,
    UNER_CMD_SET_MODE_STA = 0x11u,
    UNER_CMD_SET_CREDENTIALS = 0x12u,
    UNER_CMD_CLEAR_CREDENTIALS = 0x13u,
    UNER_CMD_START_SCAN = 0x14u,
    UNER_CMD_GET_SCAN_RESULTS = 0x15u,
    UNER_CMD_REBOOT_ESP = 0x16u,
    UNER_CMD_FACTORY_RESET = 0x17u,
    UNER_CMD_GET_STATUS = 0x30u,
    UNER_CMD_PING = 0x31u,
    UNER_CMD_GET_PREFERENCES = 0x40u,
    UNER_CMD_ECHO = 0x42u,
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

static const UNER_CommandSpec uner_commands[] = {
    { UNER_CMD_SET_MODE_AP, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_SET_MODE_STA, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 300u, NULL },
    { UNER_CMD_SET_CREDENTIALS, 0u, 255u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_CLEAR_CREDENTIALS, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_START_SCAN, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, NULL },
    { UNER_CMD_GET_SCAN_RESULTS, 0u, 0u, UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_REBOOT_ESP, 0u, 0u, UNER_SPEC_F_ACK, 50u, NULL },
    { UNER_CMD_FACTORY_RESET, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 200u, NULL },
    { UNER_CMD_GET_STATUS, 0u, 0u, UNER_SPEC_F_RESP, 100u, NULL },
    { UNER_CMD_PING, 0u, 0u, UNER_SPEC_F_RESP, 50u, NULL },
    { UNER_CMD_GET_PREFERENCES, 0u, 0u, UNER_SPEC_F_ACK | UNER_SPEC_F_RESP, 100u, NULL },
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
    { UNER_CMD_ECHO, 4u, 4u, 0u, 0u, NULL },
};

static void UNER_App_ExecuteCommand(void *ctx, const UNER_Packet *packet)
{
    (void)ctx;
    if (!packet) {
        return;
    }

    if (packet->cmd == UNER_CMD_ECHO) {
        OledUtils_ShowNotificationMs(OledUtils_RenderCommandReceivedNotification, 2000u);
        __NOP();
    }
}

static void evt_boot_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_mode_changed_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_sta_connected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_sta_disconnected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_ap_client_join_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_ap_client_leave_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_webserver_up_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_webserver_client_conn_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_webserver_client_disconn_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_lastwifi_notfound_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_app_mpu_readings_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_app_tcrt_readings_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_app_user_connected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_app_user_disconnected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_controller_connected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_controller_disconnected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_usb_connected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }
static void evt_usb_disconnected_handler(void *ctx, const UNER_Packet *p) { (void)ctx; (void)p; __NOP(); }


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
    __NOP();
    __NOP();

    return UNER_Send(&uner_handle, UNER_TR_UART1_ESP, UNER_NODE_MCU, dst, cmd, payload, len);
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
