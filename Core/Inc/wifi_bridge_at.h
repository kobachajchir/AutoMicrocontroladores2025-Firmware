/*
 * wifi_bridge_at.h - ESP-01 AT backend for WifiBridge.
 */
#ifndef INC_WIFI_BRIDGE_AT_H_
#define INC_WIFI_BRIDGE_AT_H_

#include "wifi_bridge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

WifiBridge_Result WifiBridge_AT_Init(WifiBridge_Handle *h, UART_HandleTypeDef *huart);
WifiBridge_Result WifiBridge_AT_Start(WifiBridge_Handle *h);
void WifiBridge_AT_Process(WifiBridge_Handle *h);
void WifiBridge_AT_RxCheck(WifiBridge_Handle *h);
void WifiBridge_AT_OnTxComplete(WifiBridge_Handle *h);
void WifiBridge_AT_ResetRxPos(WifiBridge_Handle *h);

WifiBridge_Result WifiBridge_AT_SetModeSTA(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_AT_SetModeAP(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_AT_SetCredentials(WifiBridge_Handle *h,
                                               const char *ssid,
                                               const char *pass);
WifiBridge_Result WifiBridge_AT_Connect(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_AT_Disconnect(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_AT_StartScan(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_AT_StopScan(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_AT_GetScanResults(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_AT_GetStatus(WifiBridge_Handle *h);

WifiBridge_Result WifiBridge_AT_SendCommand(WifiBridge_Handle *h,
                                            uint8_t cmd,
                                            const uint8_t *payload,
                                            uint8_t len);
WifiBridge_Result WifiBridge_AT_SendWifiCredentialRequest(WifiBridge_Handle *h,
                                                          const char *ssid);
WifiBridge_Result WifiBridge_AT_RequestWifiNetworkDetail(WifiBridge_Handle *h,
                                                         const char *ssid);
WifiBridge_Result WifiBridge_AT_SendEspReboot(WifiBridge_Handle *h, uint8_t boot_mode);
WifiBridge_Result WifiBridge_AT_ReportScreenChanged(WifiBridge_Handle *h,
                                                    uint32_t screen_code,
                                                    uint8_t source);
WifiBridge_Result WifiBridge_AT_ReportMenuSelectionChanged(WifiBridge_Handle *h,
                                                           uint32_t screen_code,
                                                           uint8_t selected_index,
                                                           uint8_t item_count,
                                                           uint8_t source);
WifiBridge_Result WifiBridge_AT_ReportCarModeChanged(WifiBridge_Handle *h, uint8_t mode);
WifiBridge_Result WifiBridge_AT_RequestBinaryAsset(WifiBridge_Handle *h, uint8_t asset_id);
uint8_t WifiBridge_AT_GetBinaryAsset(WifiBridge_Handle *h,
                                     uint8_t asset_id,
                                     WifiBridge_BinaryAssetView *asset);
uint8_t WifiBridge_AT_IsWaitingValidation(WifiBridge_Handle *h);
uint8_t WifiBridge_AT_GetWaitingCommandId(WifiBridge_Handle *h);

#ifdef __cplusplus
}
#endif

#endif /* INC_WIFI_BRIDGE_AT_H_ */
