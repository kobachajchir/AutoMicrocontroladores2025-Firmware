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

typedef enum {
    UNER_CMD_ECHO = 0x42u,
} UNER_AppCommand;

static const UNER_CommandSpec uner_commands[] = {
    { UNER_CMD_ECHO, 4u, 4u },
};

static void UNER_App_ExecuteCommand(void *ctx, const UNER_Packet *packet)
{
    (void)ctx;
    if (!packet) {
        return;
    }

    switch (packet->cmd) {
    case UNER_CMD_ECHO:
        OledUtils_ShowNotificationMs(OledUtils_RenderCommandReceivedNotification, 2000u);
        __NOP();
        break;
    default:
        break;
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
    UNER_TransportUart1Dma_OnTxComplete(&uner_uart1);
}

void UNER_App_NotifyUart1Rx(void)
{
    uner_uart1_rx_hint = 1u;
    __NOP();
}
