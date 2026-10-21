/*
 * wifi_bridge_ui_adapter.c - one place to translate WiFi events to UI state.
 */
#include "wifi_bridge_ui_adapter.h"

#include <string.h>

#include "globals.h"
#include "menusystem.h"
#include "oled_utils.h"
#include "screenWrappers.h"
#include "utils/macros_utils.h"

static void WifiBridge_UI_SetIp(volatile IPStruct_t *dst, const uint8_t ip[WIFI_BRIDGE_IP_LEN])
{
    if ((dst == NULL) || (ip == NULL)) {
        return;
    }

    dst->block1 = ip[0];
    dst->block2 = ip[1];
    dst->block3 = ip[2];
    dst->block4 = ip[3];
}

static uint8_t WifiBridge_UI_HasIp(const uint8_t ip[WIFI_BRIDGE_IP_LEN])
{
    return (ip != NULL) &&
           ((ip[0] != 0u) || (ip[1] != 0u) || (ip[2] != 0u) || (ip[3] != 0u));
}

static void WifiBridge_UI_RequestNetworkRefresh(void)
{
    const ScreenCode_t screen_code = MenuSys_GetCurrentScreenCode(&menuSystem);

    if ((screen_code == SCREEN_CODE_CORE_DASHBOARD) ||
        (screen_code == SCREEN_CODE_CONNECTIVITY_WIFI_STATUS)) {
        menuSystem.renderFlag = true;
    }
}

void WifiBridge_UI_OnEvent(const WifiBridge_Event *evt)
{
    if (evt == NULL) {
        return;
    }

    switch (evt->id) {
        case WIFI_BRIDGE_EVT_BOOT:
            SET_FLAG(systemFlags2, ESP_PRESENT);
            OledUtils_ReplaceSimpleNotificationMs(OLED_SIMPLE_NOTIFICATION_ESP_BOOT_RECEIVED, 2000u);
            break;

        case WIFI_BRIDGE_EVT_MODE_CHANGED:
            SET_FLAG(systemFlags2, ESP_PRESENT);
            if (evt->mode == WIFI_BRIDGE_MODE_STA) {
                CLEAR_FLAG(systemFlags2, AP_ACTIVE);
            } else if (evt->mode == WIFI_BRIDGE_MODE_AP) {
                SET_FLAG(systemFlags2, AP_ACTIVE);
            }
            OledUtils_ReplaceSimpleNotificationMs(OLED_SIMPLE_NOTIFICATION_ESP_MODE_CHANGED, 2200u);
            WifiBridge_UI_RequestNetworkRefresh();
            break;

        case WIFI_BRIDGE_EVT_STA_CONNECTED:
            SET_FLAG(systemFlags2, ESP_PRESENT);
            SET_FLAG(systemFlags2, WIFI_ACTIVE);
            OledUtils_SetWiFiConnectedSsid(evt->ssid);
            WifiBridge_UI_RequestNetworkRefresh();
            break;

        case WIFI_BRIDGE_EVT_GOT_IP:
            SET_FLAG(systemFlags2, ESP_PRESENT);
            SET_FLAG(systemFlags2, WIFI_ACTIVE);
            CLEAR_FLAG(systemFlags2, AP_ACTIVE);
            if (WifiBridge_UI_HasIp(evt->ip)) {
                WifiBridge_UI_SetIp(&espStaIp, evt->ip);
                WifiBridge_UI_SetIp(&espWifiConnection.staIp, evt->ip);
                espWifiConnection.staIpValid = 1u;
            }
            OledUtils_SetWiFiConnectedSsid(evt->ssid);
            OledUtils_ReplaceNotificationMs(OledUtils_RenderESPWifiConnectedNotification, 2500u);
            WifiBridge_UI_RequestNetworkRefresh();
            break;

        case WIFI_BRIDGE_EVT_STA_DISCONNECTED:
            CLEAR_FLAG(systemFlags2, WIFI_ACTIVE);
            espWifiConnection.staIpValid = 0u;
            memset((void *)&espStaIp, 0, sizeof(espStaIp));
            memset(&espWifiConnection.staIp, 0, sizeof(espWifiConnection.staIp));
            WifiBridge_UI_RequestNetworkRefresh();
            break;

        case WIFI_BRIDGE_EVT_SCAN_DONE:
            CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
            wifiScanSessionActive = 0u;
            wifiScanResultsPending = 1u;
            if (MenuSys_GetCurrentScreenCode(&menuSystem) == SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING) {
                menuSystem.renderFn = OledUtils_RenderWiFiSearchResults_Wrapper;
                menuSystem.renderFlag = true;
                oled_first_draw = true;
            }
            break;

        case WIFI_BRIDGE_EVT_AP_STARTED:
            SET_FLAG(systemFlags2, ESP_PRESENT);
            SET_FLAG(systemFlags2, AP_ACTIVE);
            CLEAR_FLAG(systemFlags2, WIFI_ACTIVE);
            if (WifiBridge_UI_HasIp(evt->ip)) {
                WifiBridge_UI_SetIp(&espApIp, evt->ip);
                WifiBridge_UI_SetIp(&espWifiConnection.apIp, evt->ip);
                espWifiConnection.apIpValid = 1u;
            }
            OledUtils_ReplaceNotificationMs(OledUtils_RenderESPAPStartedNotification, 2500u);
            WifiBridge_UI_RequestNetworkRefresh();
            break;

        case WIFI_BRIDGE_EVT_AP_STOPPED:
            CLEAR_FLAG(systemFlags2, AP_ACTIVE);
            espWifiConnection.apIpValid = 0u;
            memset((void *)&espApIp, 0, sizeof(espApIp));
            memset(&espWifiConnection.apIp, 0, sizeof(espWifiConnection.apIp));
            WifiBridge_UI_RequestNetworkRefresh();
            break;

        case WIFI_BRIDGE_EVT_ERROR:
            CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
            wifiScanSessionActive = 0u;
            WifiBridge_UI_RequestNetworkRefresh();
            break;

        case WIFI_BRIDGE_EVT_NONE:
        default:
            break;
    }
}
