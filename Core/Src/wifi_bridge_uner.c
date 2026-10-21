/*
 * wifi_bridge_uner.c - thin adapter over the existing UNER v2 app layer.
 */
#include "wifi_bridge_uner.h"
#include "wifi_bridge.h"

#if !defined(WIFI_BRIDGE_USE_UNER)
#error "wifi_bridge_uner.c requires WIFI_BRIDGE_USE_UNER"
#endif

#include <string.h>

#include "uner_app.h"

static WifiBridge_Result WifiBridge_UNER_Result(UNER_Status status)
{
    switch (status) {
        case UNER_OK:
            return WIFI_BRIDGE_OK;
        case UNER_ERR_BUSY:
            return WIFI_BRIDGE_BUSY;
        case UNER_ERR_QUEUE_FULL:
            return WIFI_BRIDGE_QUEUE_FULL;
        case UNER_ERR_LEN:
            return WIFI_BRIDGE_BAD_ARG;
        default:
            return WIFI_BRIDGE_ERROR;
    }
}

WifiBridge_Result WifiBridge_UNER_Init(WifiBridge_Handle *h, UART_HandleTypeDef *huart)
{
    if (h == NULL) {
        return WIFI_BRIDGE_BAD_ARG;
    }

    memset(h, 0, sizeof(*h));
    h->huart = huart;
    UNER_App_Init();
    return WIFI_BRIDGE_OK;
}

WifiBridge_Result WifiBridge_UNER_Start(WifiBridge_Handle *h)
{
    (void)h;
    return WIFI_BRIDGE_OK;
}

void WifiBridge_UNER_Process(WifiBridge_Handle *h)
{
    (void)h;
    UNER_App_Poll();
}

void WifiBridge_UNER_RxCheck(WifiBridge_Handle *h)
{
    (void)h;
    UNER_App_NotifyUart1Rx();
}

void WifiBridge_UNER_OnTxComplete(WifiBridge_Handle *h)
{
    (void)h;
    UNER_App_OnUart1TxComplete();
}

void WifiBridge_UNER_ResetRxPos(WifiBridge_Handle *h)
{
    (void)h;
    UNER_App_ResetUart1RxPos();
}

WifiBridge_Result WifiBridge_UNER_SendCommand(WifiBridge_Handle *h,
                                              uint8_t cmd,
                                              const uint8_t *payload,
                                              uint8_t len)
{
    (void)h;
    return WifiBridge_UNER_Result(UNER_App_SendCommand(cmd, payload, len));
}

WifiBridge_Result WifiBridge_UNER_SetModeSTA(WifiBridge_Handle *h)
{
    return WifiBridge_UNER_SendCommand(h, WIFI_BRIDGE_CMD_ID_SET_MODE_STA, NULL, 0u);
}

WifiBridge_Result WifiBridge_UNER_SetModeAP(WifiBridge_Handle *h)
{
    return WifiBridge_UNER_SendCommand(h, WIFI_BRIDGE_CMD_ID_SET_MODE_AP, NULL, 0u);
}

WifiBridge_Result WifiBridge_UNER_SetCredentials(WifiBridge_Handle *h,
                                                 const char *ssid,
                                                 const char *pass)
{
    uint8_t payload[2u + WIFI_BRIDGE_SSID_MAX_LEN + WIFI_BRIDGE_PASS_MAX_LEN];
    uint8_t ssid_len = 0u;
    uint8_t pass_len = 0u;
    uint8_t pos = 0u;

    if ((ssid == NULL) || (pass == NULL)) {
        return WIFI_BRIDGE_BAD_ARG;
    }

    (void)WifiBridge_SetRuntimeCredentials(ssid, pass);

    while (ssid[ssid_len] != '\0' && ssid_len < WIFI_BRIDGE_SSID_MAX_LEN) {
        ssid_len++;
    }
    while (pass[pass_len] != '\0' && pass_len < WIFI_BRIDGE_PASS_MAX_LEN) {
        pass_len++;
    }

    payload[pos++] = ssid_len;
    memcpy(&payload[pos], ssid, ssid_len);
    pos = (uint8_t)(pos + ssid_len);
    payload[pos++] = pass_len;
    memcpy(&payload[pos], pass, pass_len);
    pos = (uint8_t)(pos + pass_len);

    return WifiBridge_UNER_SendCommand(h, WIFI_BRIDGE_CMD_ID_SET_CREDENTIALS, payload, pos);
}

WifiBridge_Result WifiBridge_UNER_Connect(WifiBridge_Handle *h)
{
    return WifiBridge_UNER_SendCommand(h, WIFI_BRIDGE_CMD_ID_CONNECT_WIFI, NULL, 0u);
}

WifiBridge_Result WifiBridge_UNER_Disconnect(WifiBridge_Handle *h)
{
    return WifiBridge_UNER_SendCommand(h, WIFI_BRIDGE_CMD_ID_DISCONNECT_WIFI, NULL, 0u);
}

WifiBridge_Result WifiBridge_UNER_StartScan(WifiBridge_Handle *h)
{
    return WifiBridge_UNER_SendCommand(h, WIFI_BRIDGE_CMD_ID_START_SCAN, NULL, 0u);
}

WifiBridge_Result WifiBridge_UNER_StopScan(WifiBridge_Handle *h)
{
    return WifiBridge_UNER_SendCommand(h, WIFI_BRIDGE_CMD_ID_STOP_SCAN, NULL, 0u);
}

WifiBridge_Result WifiBridge_UNER_GetScanResults(WifiBridge_Handle *h)
{
    return WifiBridge_UNER_SendCommand(h, WIFI_BRIDGE_CMD_ID_GET_SCAN_RESULTS, NULL, 0u);
}

WifiBridge_Result WifiBridge_UNER_GetStatus(WifiBridge_Handle *h)
{
    return WifiBridge_UNER_SendCommand(h, WIFI_BRIDGE_CMD_ID_GET_STATUS, NULL, 0u);
}

WifiBridge_Result WifiBridge_UNER_SendWifiCredentialRequest(WifiBridge_Handle *h,
                                                            const char *ssid)
{
    (void)h;
    (void)WifiBridge_SetRuntimeSsid(ssid);
    return WifiBridge_UNER_Result(UNER_App_SendWifiCredentialRequest(ssid));
}

WifiBridge_Result WifiBridge_UNER_RequestWifiNetworkDetail(WifiBridge_Handle *h,
                                                           const char *ssid)
{
    (void)h;
    return WifiBridge_UNER_Result(UNER_App_RequestWifiNetworkDetail(ssid));
}

WifiBridge_Result WifiBridge_UNER_SendEspReboot(WifiBridge_Handle *h, uint8_t boot_mode)
{
    (void)h;
    return WifiBridge_UNER_Result(UNER_App_SendEspReboot(boot_mode));
}

WifiBridge_Result WifiBridge_UNER_ReportScreenChanged(WifiBridge_Handle *h,
                                                      uint32_t screen_code,
                                                      uint8_t source)
{
    (void)h;
    return WifiBridge_UNER_Result(UNER_App_ReportScreenChanged(screen_code, source));
}

WifiBridge_Result WifiBridge_UNER_ReportMenuSelectionChanged(WifiBridge_Handle *h,
                                                             uint32_t screen_code,
                                                             uint8_t selected_index,
                                                             uint8_t item_count,
                                                             uint8_t source)
{
    (void)h;
    return WifiBridge_UNER_Result(UNER_App_ReportMenuSelectionChanged(screen_code,
                                                                      selected_index,
                                                                      item_count,
                                                                      source));
}

WifiBridge_Result WifiBridge_UNER_ReportCarModeChanged(WifiBridge_Handle *h, uint8_t mode)
{
    (void)h;
    return WifiBridge_UNER_Result(UNER_App_ReportCarModeChanged(mode));
}

WifiBridge_Result WifiBridge_UNER_RequestBinaryAsset(WifiBridge_Handle *h, uint8_t asset_id)
{
    (void)h;
    return WifiBridge_UNER_Result(UNER_App_RequestBinaryAsset(asset_id));
}

uint8_t WifiBridge_UNER_GetBinaryAsset(WifiBridge_Handle *h,
                                       uint8_t asset_id,
                                       WifiBridge_BinaryAssetView *asset)
{
    BinaryAssetView_t uner_asset;
    uint8_t ready;

    (void)h;
    if (asset == NULL) {
        return 0u;
    }

    ready = UNER_App_GetBinaryAsset(asset_id, &uner_asset);
    if (ready) {
        asset->asset_id = uner_asset.asset_id;
        asset->width = uner_asset.width;
        asset->height = uner_asset.height;
        asset->len = uner_asset.len;
        asset->data = uner_asset.data;
    }

    return ready;
}

uint8_t WifiBridge_UNER_IsWaitingValidation(WifiBridge_Handle *h)
{
    (void)h;
    return UNER_App_IsWaitingValidation();
}

uint8_t WifiBridge_UNER_GetWaitingCommandId(WifiBridge_Handle *h)
{
    (void)h;
    return UNER_App_GetWaitingCommandId();
}
