/*
 * oled_utils.h
 *
 *  Created on: Jun 16, 2025
 *      Author: kobac
 */

#ifndef INC_OLED_UTILS_H_
#define INC_OLED_UTILS_H_

#include "menusystem.h"
#include "mpu6050.h"
#include "oled_handle.h"
#include <stdbool.h>
#include <stdint.h>

// ─── Funciones de despliegue ─────────────────────────────────────────────────

void OledUtils_RenderDashboard(void);

void OledUtils_RenderProyectScreen(void);
void OledUtils_RenderProyectInfoScreen(void);

void OledUtils_RenderModeChange_Full(void);
void OledUtils_RenderModeChange_ModeOnly(void);

void OledUtils_Clear(void);

void OledUtils_DrawItem(const MenuItem *item, uint8_t y, bool selected);

void OledUtils_MotorTest_Complete(void);

void OledUtils_MotorTest_Changes(void);

void OledUtils_EnableContinuousRender(void);
void OledUtils_DisableContinuousRender(void);

// --- Sensores IR ---
void OledUtils_RenderIRGraphScene(void);
void OledUtils_UpdateIRBars(volatile uint16_t *sensorData);

// --- Sensor MPU ---
void OledUtils_RenderMPUScene(void);
void OledUtils_UpdateMPUValues(MPU6050_Handle_t *mpu);

void OledUtils_RenderWiFiSearchScene(void);
void OledUtils_UpdateWiFiSearchTimer(uint8_t secondsRemaining);
void OledUtils_ShowWifiResults(void);
void OledUtils_RenderWiFiDetails(const char *ssid,
                                 const char *encryption,
                                 int8_t signal_strength,
                                 uint8_t channel,
                                 uint8_t detail_valid);

typedef enum {
    OLED_SIMPLE_NOTIFICATION_NONE = 0u,
    OLED_SIMPLE_NOTIFICATION_WIFI_SEARCH_CANCELED,
    OLED_SIMPLE_NOTIFICATION_COMMAND_RECEIVED,
    OLED_SIMPLE_NOTIFICATION_PING_RECEIVED,
    OLED_SIMPLE_NOTIFICATION_ESP_BOOT_RECEIVED,
    OLED_SIMPLE_NOTIFICATION_ESP_MODE_CHANGED,
    OLED_SIMPLE_NOTIFICATION_ESP_USB_CONNECTED,
    OLED_SIMPLE_NOTIFICATION_ESP_WEB_SERVER_UP,
    OLED_SIMPLE_NOTIFICATION_ESP_WEB_CLIENT_CONNECTED,
    OLED_SIMPLE_NOTIFICATION_ESP_WEB_CLIENT_DISCONNECTED,
    OLED_SIMPLE_NOTIFICATION_ESP_CHECKING_CONNECTION,
    OLED_SIMPLE_NOTIFICATION_ESP_CHECK_CONNECTION_REQUIRED,
    OLED_SIMPLE_NOTIFICATION_PERMISSION_DENIED,
    OLED_SIMPLE_NOTIFICATION_CONTROLLER_CONNECTED,
    OLED_SIMPLE_NOTIFICATION_CONTROLLER_DISCONNECTED,
} OledSimpleNotificationId;

void OledUtils_RenderWiFiCredentialsWebNotification(void);
void OledUtils_SetWiFiCredentialsWebResultStatus(uint8_t status);
void OledUtils_RenderWiFiCredentialsSucceededNotification(void);
void OledUtils_RenderWiFiCredentialsFailedNotification(void);
void OledUtils_RenderWiFiNotConnected(void);
void OledUtils_RenderWiFiStatus(void);
void OledUtils_SetWiFiConnectedSsid(const char *ssid);
void OledUtils_RenderESPWifiConnectingNotification(void);
void OledUtils_RenderESPFirmwareRequestNotification(void);
void OledUtils_RenderESPFirmwareScreen(void);
void OledUtils_RenderESPResetSentNotification(void);
void OledUtils_RenderESPWifiConnectedNotification(void);
void OledUtils_RenderESPAPStartedNotification(void);

void OledUtils_ShowNotificationMs(RenderFunction renderFn, uint16_t timeout_ms);
void OledUtils_ShowNotificationMsEx(RenderFunction renderFn, uint16_t timeout_ms, OledNotificationHook onShow, OledNotificationHook onHide);
void OledUtils_ReplaceNotificationMs(RenderFunction renderFn, uint16_t timeout_ms);
bool OledUtils_IsNotificationShowing(RenderFunction renderFn);
void OledUtils_ShowSimpleNotificationMs(OledSimpleNotificationId notificationId, uint16_t timeout_ms);
void OledUtils_ReplaceSimpleNotificationMs(OledSimpleNotificationId notificationId, uint16_t timeout_ms);
bool OledUtils_IsSimpleNotificationShowing(OledSimpleNotificationId notificationId);
void OledUtils_DismissNotification(void);
void OledUtils_NotificationTick10ms(void);

void OledUtils_NotificationRestore(void);

#endif /* INC_OLED_UTILS_H_ */
