/*
 * wifi_bridge.c - compile-time dispatch for the selected WiFi backend.
 */
#include "wifi_bridge.h"

#if defined(WIFI_BRIDGE_USE_ESP_AT)
#include "wifi_bridge_at.h"
#elif defined(WIFI_BRIDGE_USE_UNER)
#include "wifi_bridge_uner.h"
#endif

WifiBridge_Handle wifiBridge;
WifiBridge_Credentials wifiBridgeCredentials = {
    .flags.raw = ((sizeof(WIFI_BRIDGE_AT_DEFAULT_PASSWORD) > 1u) ? 0x02u : 0x00u),
    .ssid = "",
    .password = WIFI_BRIDGE_AT_DEFAULT_PASSWORD,
};

static WifiBridge_Result WifiBridge_CopyRuntimeText(char *dst,
                                                    size_t dst_len,
                                                    const char *src)
{
    size_t i = 0u;
    size_t len = 0u;

    if ((dst == NULL) || (dst_len == 0u) || (src == NULL)) {
        return WIFI_BRIDGE_BAD_ARG;
    }

    while (src[len] != '\0') {
        if (len + 1u >= dst_len) {
            return WIFI_BRIDGE_BAD_ARG;
        }
        len++;
    }

    while (i < len) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
    return WIFI_BRIDGE_OK;
}

WifiBridge_Result WifiBridge_Init(WifiBridge_Handle *h, UART_HandleTypeDef *huart)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_Init(h, huart);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_Init(h, huart);
#endif
}

WifiBridge_Result WifiBridge_Start(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_Start(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_Start(h);
#endif
}

void WifiBridge_Process(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    WifiBridge_AT_Process(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    WifiBridge_UNER_Process(h);
#endif
}

void WifiBridge_RxCheck(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    WifiBridge_AT_RxCheck(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    WifiBridge_UNER_RxCheck(h);
#endif
}

void WifiBridge_OnIdleIrq(WifiBridge_Handle *h)
{
    WifiBridge_RxCheck(h);
}

void WifiBridge_OnTxComplete(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    WifiBridge_AT_OnTxComplete(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    WifiBridge_UNER_OnTxComplete(h);
#endif
}

void WifiBridge_OnRxHalfComplete(WifiBridge_Handle *h)
{
    WifiBridge_RxCheck(h);
}

void WifiBridge_OnRxComplete(WifiBridge_Handle *h)
{
    WifiBridge_RxCheck(h);
}

void WifiBridge_OnError(WifiBridge_Handle *h)
{
    if (h != NULL) {
        WIFI_FLAG_CLEAR(h, WIFI_FLAG_BUSY);
        WIFI_FLAG_CLEAR(h, WIFI_FLAG_TX_BUSY);
        WIFI_FLAG_SET(h, WIFI_FLAG_GOT_ERROR);
    }
}

void WifiBridge_ResetRxPos(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    WifiBridge_AT_ResetRxPos(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    WifiBridge_UNER_ResetRxPos(h);
#endif
}

void WifiBridge_SetEventCallback(WifiBridge_Handle *h, WifiBridge_EventCallback cb)
{
    if (h != NULL) {
        h->on_event = cb;
    }
}

void WifiBridge_EmitEvent(WifiBridge_Handle *h, const WifiBridge_Event *evt)
{
    if ((h == NULL) || (evt == NULL)) {
        return;
    }

    h->last_event = *evt;
    if (h->on_event != NULL) {
        h->on_event(evt);
    }
}

WifiBridge_Result WifiBridge_SetRuntimeCredentials(const char *ssid, const char *password)
{
    WifiBridge_Result status;

    status = WifiBridge_SetRuntimeSsid(ssid);
    if (status != WIFI_BRIDGE_OK) {
        return status;
    }

    return WifiBridge_SetRuntimePassword(password);
}

WifiBridge_Result WifiBridge_SetRuntimeSsid(const char *ssid)
{
    WifiBridge_Result status = WifiBridge_CopyRuntimeText(wifiBridgeCredentials.ssid,
                                                          sizeof(wifiBridgeCredentials.ssid),
                                                          ssid);
    if (status == WIFI_BRIDGE_OK) {
        wifiBridgeCredentials.flags.bits.ssid_set = (ssid[0] != '\0') ? 1u : 0u;
    }

    return status;
}

WifiBridge_Result WifiBridge_SetRuntimePassword(const char *password)
{
    WifiBridge_Result status = WifiBridge_CopyRuntimeText(wifiBridgeCredentials.password,
                                                          sizeof(wifiBridgeCredentials.password),
                                                          password);
    if (status == WIFI_BRIDGE_OK) {
        wifiBridgeCredentials.flags.bits.password_set = (password[0] != '\0') ? 1u : 0u;
    }

    return status;
}

const WifiBridge_Credentials *WifiBridge_GetRuntimeCredentials(void)
{
    return &wifiBridgeCredentials;
}

WifiBridge_Result WifiBridge_SetModeSTA(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_SetModeSTA(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_SetModeSTA(h);
#endif
}

WifiBridge_Result WifiBridge_SetModeAP(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_SetModeAP(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_SetModeAP(h);
#endif
}

WifiBridge_Result WifiBridge_SetCredentials(WifiBridge_Handle *h,
                                            const char *ssid,
                                            const char *pass)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_SetCredentials(h, ssid, pass);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_SetCredentials(h, ssid, pass);
#endif
}

WifiBridge_Result WifiBridge_Connect(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_Connect(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_Connect(h);
#endif
}

WifiBridge_Result WifiBridge_Disconnect(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_Disconnect(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_Disconnect(h);
#endif
}

WifiBridge_Result WifiBridge_StartScan(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_StartScan(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_StartScan(h);
#endif
}

WifiBridge_Result WifiBridge_StopScan(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_StopScan(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_StopScan(h);
#endif
}

WifiBridge_Result WifiBridge_GetScanResults(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_GetScanResults(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_GetScanResults(h);
#endif
}

WifiBridge_Result WifiBridge_GetStatus(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_GetStatus(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_GetStatus(h);
#endif
}

WifiBridge_Result WifiBridge_SendCommand(WifiBridge_Handle *h,
                                         uint8_t cmd,
                                         const uint8_t *payload,
                                         uint8_t len)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_SendCommand(h, cmd, payload, len);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_SendCommand(h, cmd, payload, len);
#endif
}

WifiBridge_Result WifiBridge_SendWifiCredentialRequest(WifiBridge_Handle *h,
                                                       const char *ssid)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_SendWifiCredentialRequest(h, ssid);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_SendWifiCredentialRequest(h, ssid);
#endif
}

WifiBridge_Result WifiBridge_RequestWifiNetworkDetail(WifiBridge_Handle *h,
                                                      const char *ssid)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_RequestWifiNetworkDetail(h, ssid);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_RequestWifiNetworkDetail(h, ssid);
#endif
}

WifiBridge_Result WifiBridge_SendEspReboot(WifiBridge_Handle *h, uint8_t boot_mode)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_SendEspReboot(h, boot_mode);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_SendEspReboot(h, boot_mode);
#endif
}

WifiBridge_Result WifiBridge_ReportScreenChanged(WifiBridge_Handle *h,
                                                 uint32_t screen_code,
                                                 uint8_t source)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_ReportScreenChanged(h, screen_code, source);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_ReportScreenChanged(h, screen_code, source);
#endif
}

WifiBridge_Result WifiBridge_ReportMenuSelectionChanged(WifiBridge_Handle *h,
                                                        uint32_t screen_code,
                                                        uint8_t selected_index,
                                                        uint8_t item_count,
                                                        uint8_t source)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_ReportMenuSelectionChanged(h, screen_code, selected_index, item_count, source);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_ReportMenuSelectionChanged(h, screen_code, selected_index, item_count, source);
#endif
}

WifiBridge_Result WifiBridge_ReportCarModeChanged(WifiBridge_Handle *h, uint8_t mode)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_ReportCarModeChanged(h, mode);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_ReportCarModeChanged(h, mode);
#endif
}

WifiBridge_Result WifiBridge_RequestBinaryAsset(WifiBridge_Handle *h, uint8_t asset_id)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_RequestBinaryAsset(h, asset_id);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_RequestBinaryAsset(h, asset_id);
#endif
}

uint8_t WifiBridge_GetBinaryAsset(WifiBridge_Handle *h,
                                  uint8_t asset_id,
                                  WifiBridge_BinaryAssetView *asset)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_GetBinaryAsset(h, asset_id, asset);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_GetBinaryAsset(h, asset_id, asset);
#endif
}

uint8_t WifiBridge_IsWaitingValidation(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_IsWaitingValidation(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_IsWaitingValidation(h);
#endif
}

uint8_t WifiBridge_GetWaitingCommandId(WifiBridge_Handle *h)
{
#if defined(WIFI_BRIDGE_USE_ESP_AT)
    return WifiBridge_AT_GetWaitingCommandId(h);
#elif defined(WIFI_BRIDGE_USE_UNER)
    return WifiBridge_UNER_GetWaitingCommandId(h);
#endif
}
