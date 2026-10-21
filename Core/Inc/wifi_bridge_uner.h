/*
 * wifi_bridge_uner.h - UNER v2 backend for WifiBridge.
 */
#ifndef INC_WIFI_BRIDGE_UNER_H_
#define INC_WIFI_BRIDGE_UNER_H_

#include "wifi_bridge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

WifiBridge_Result WifiBridge_UNER_Init(WifiBridge_Handle *h, UART_HandleTypeDef *huart);
WifiBridge_Result WifiBridge_UNER_Start(WifiBridge_Handle *h);
void WifiBridge_UNER_Process(WifiBridge_Handle *h);
void WifiBridge_UNER_RxCheck(WifiBridge_Handle *h);
void WifiBridge_UNER_OnTxComplete(WifiBridge_Handle *h);
void WifiBridge_UNER_ResetRxPos(WifiBridge_Handle *h);

WifiBridge_Result WifiBridge_UNER_SetModeSTA(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_UNER_SetModeAP(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_UNER_SetCredentials(WifiBridge_Handle *h,
                                                 const char *ssid,
                                                 const char *pass);
WifiBridge_Result WifiBridge_UNER_Connect(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_UNER_Disconnect(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_UNER_StartScan(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_UNER_StopScan(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_UNER_GetScanResults(WifiBridge_Handle *h);
WifiBridge_Result WifiBridge_UNER_GetStatus(WifiBridge_Handle *h);

WifiBridge_Result WifiBridge_UNER_SendCommand(WifiBridge_Handle *h,
                                              uint8_t cmd,
                                              const uint8_t *payload,
                                              uint8_t len);
WifiBridge_Result WifiBridge_UNER_SendWifiCredentialRequest(WifiBridge_Handle *h,
                                                            const char *ssid);
WifiBridge_Result WifiBridge_UNER_RequestWifiNetworkDetail(WifiBridge_Handle *h,
                                                           const char *ssid);
WifiBridge_Result WifiBridge_UNER_SendEspReboot(WifiBridge_Handle *h, uint8_t boot_mode);
WifiBridge_Result WifiBridge_UNER_ReportScreenChanged(WifiBridge_Handle *h,
                                                      uint32_t screen_code,
                                                      uint8_t source);
WifiBridge_Result WifiBridge_UNER_ReportMenuSelectionChanged(WifiBridge_Handle *h,
                                                             uint32_t screen_code,
                                                             uint8_t selected_index,
                                                             uint8_t item_count,
                                                             uint8_t source);
WifiBridge_Result WifiBridge_UNER_ReportCarModeChanged(WifiBridge_Handle *h, uint8_t mode);
WifiBridge_Result WifiBridge_UNER_RequestBinaryAsset(WifiBridge_Handle *h, uint8_t asset_id);
uint8_t WifiBridge_UNER_GetBinaryAsset(WifiBridge_Handle *h,
                                       uint8_t asset_id,
                                       WifiBridge_BinaryAssetView *asset);
uint8_t WifiBridge_UNER_IsWaitingValidation(WifiBridge_Handle *h);
uint8_t WifiBridge_UNER_GetWaitingCommandId(WifiBridge_Handle *h);

#ifdef __cplusplus
}
#endif

#endif /* INC_WIFI_BRIDGE_UNER_H_ */
