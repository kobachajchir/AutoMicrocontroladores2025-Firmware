/*
 * wifi_bridge_ui_adapter.h - common WifiBridge event to OLED/UI adapter.
 */
#ifndef INC_WIFI_BRIDGE_UI_ADAPTER_H_
#define INC_WIFI_BRIDGE_UI_ADAPTER_H_

#include "wifi_bridge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void WifiBridge_UI_OnEvent(const WifiBridge_Event *evt);

#ifdef __cplusplus
}
#endif

#endif /* INC_WIFI_BRIDGE_UI_ADAPTER_H_ */
