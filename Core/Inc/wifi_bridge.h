/*
 * wifi_bridge.h - backend-neutral WiFi API for app/UI code.
 */
#ifndef INC_WIFI_BRIDGE_H_
#define INC_WIFI_BRIDGE_H_

#include "wifi_bridge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern WifiBridge_Handle wifiBridge;
extern WifiBridge_Credentials wifiBridgeCredentials;

WifiBridge_Result WifiBridge_Init(WifiBridge_Handle *h, UART_HandleTypeDef *huart);
WifiBridge_Result WifiBridge_Start(WifiBridge_Handle *h);
void WifiBridge_Process(WifiBridge_Handle *h);
void WifiBridge_RxCheck(WifiBridge_Handle *h);
void WifiBridge_OnIdleIrq(WifiBridge_Handle *h);
void WifiBridge_OnTxComplete(WifiBridge_Handle *h);
void WifiBridge_OnRxHalfComplete(WifiBridge_Handle *h);
void WifiBridge_OnRxComplete(WifiBridge_Handle *h);
void WifiBridge_OnError(WifiBridge_Handle *h);
void WifiBridge_ResetRxPos(WifiBridge_Handle *h);

void WifiBridge_SetEventCallback(WifiBridge_Handle *h, WifiBridge_EventCallback cb);
void WifiBridge_EmitEvent(WifiBridge_Handle *h, const WifiBridge_Event *evt);
WifiBridge_Result WifiBridge_SetRuntimeCredentials(const char *ssid, const char *password);
WifiBridge_Result WifiBridge_SetRuntimeSsid(const char *ssid);
WifiBridge_Result WifiBridge_SetRuntimePassword(const char *password);
const WifiBridge_Credentials *WifiBridge_GetRuntimeCredentials(void);

WifiBridge_Result WifiBridge_SetModeSTA(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_SetModeAP(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_SetCredentials(WifiBridge_Handle *h,
                                            const char *ssid,
                                            const char *pass);
WifiBridge_Result WifiBridge_Connect(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_Disconnect(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_StartScan(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_StopScan(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_GetScanResults(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_GetStatus(WifiBridge_Handle *h);

WifiBridge_Result WifiBridge_SendCommand(WifiBridge_Handle *h,
                                         uint8_t cmd,
                                         const uint8_t *payload,
                                         uint8_t len);
WifiBridge_Result WifiBridge_SendWifiCredentialRequest(WifiBridge_Handle *h,
                                                       const char *ssid);
WifiBridge_Result WifiBridge_RequestWifiNetworkDetail(WifiBridge_Handle *h,
                                                      const char *ssid);
WifiBridge_Result WifiBridge_SendEspReboot(WifiBridge_Handle *h, uint8_t boot_mode);
WifiBridge_Result WifiBridge_ReportScreenChanged(WifiBridge_Handle *h,
                                                 uint32_t screen_code,
                                                 uint8_t source);
WifiBridge_Result WifiBridge_ReportMenuSelectionChanged(WifiBridge_Handle *h,
                                                        uint32_t screen_code,
                                                        uint8_t selected_index,
                                                        uint8_t item_count,
                                                        uint8_t source);
WifiBridge_Result WifiBridge_ReportCarModeChanged(WifiBridge_Handle *h, uint8_t mode);
WifiBridge_Result WifiBridge_RequestBinaryAsset(WifiBridge_Handle *h, uint8_t asset_id);
uint8_t WifiBridge_GetBinaryAsset(WifiBridge_Handle *h,
                                  uint8_t asset_id,
                                  WifiBridge_BinaryAssetView *asset);
uint8_t WifiBridge_IsWaitingValidation(WifiBridge_Handle *h);
uint8_t WifiBridge_GetWaitingCommandId(WifiBridge_Handle *h);

#ifdef __cplusplus
}
#endif

#endif /* INC_WIFI_BRIDGE_H_ */
