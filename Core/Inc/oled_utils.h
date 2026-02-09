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

typedef struct {
    uint8_t block[4];
} IPStruct_t;

/**
 * @brief  Wrapper para la pantalla de Valores MPU.
 */
void renderValoresMPU_Wrapper(void);

/** Renderiza la pantalla principal (dashboard: "HOLA MUNDO" centrado) */
void renderDashboard_Wrapper(void);
void renderTestScreen_Wrapper(void);

// ─── Funciones de despliegue ─────────────────────────────────────────────────

/**
 * @brief  Dibuja el menú en pantalla (estilo u8g2).
 * @param  system  Puntero al MenuSystem activo
 */
void displayMenuCustom(MenuSystem *system);

void OledUtils_RenderVerticalMenu(MenuSystem *ms);

void OledUtils_RenderDashboard(void);
void OledUtils_RenderTestScreen(void);

void OledUtils_RenderProyectScreen(void);
void OledUtils_RenderProyectInfoScreen(void);

void OledUtils_RenderModeChange_Full(void);
void OledUtils_RenderModeChange_ModeOnly(void);

void OledUtils_Clear(void);

void OledUtils_DrawItem(const MenuItem *item, uint8_t y, bool selected);

void OledUtils_RenderValoresMPUScreen(MPU6050_Handle_t *mpu);

void OledUtils_DrawIRGraph(volatile uint16_t *irValues);

void OledUtils_DrawIRBars(volatile uint16_t *irValues);

void OledUtils_RenderRadarGraph(volatile uint16_t *irValues);
void OledUtils_RenderRadarGraph_Objs(volatile uint16_t *irValues);

void OledUtils_MotorTest_Complete(void);

void OledUtils_MotorTest_Changes(void);

void OledUtils_EnableContinuousRender(void);
void OledUtils_DisableContinuousRender(void);

void OledUtils_RenderLockScreen(void);
void OledUtils_RenderLockState(uint8_t lockState);

void OledUtils_ESPConnFailed(void);
void OledUtils_ESPConnSucceeded(void);

// --- Sensores IR ---
void OledUtils_RenderIRGraphScene(void);
void OledUtils_UpdateIRBars(volatile uint16_t *sensorData);

// --- Sensor MPU ---
void OledUtils_RenderMPUScene(void);
void OledUtils_UpdateMPUValues(MPU6050_Handle_t *mpu);

void OledUtils_RenderWiFiSearchScene(void);
void OledUtils_UpdateWiFiSearchTimer(uint8_t secondsRemaining);
void OledUtils_ShowWifiResults();
void OledUtils_RenderWiFiSearchCompleteNotification(void);
void OledUtils_RenderWiFiSearchCanceledNotification(void);
void OledUtils_RenderWiFiNotConnected(void);
void OledUtils_RenderWiFiStatus(void);
void OledUtils_RenderESPCheckingConnectionNotification(void);
void OledUtils_RenderESPFirmwareRequestNotification(void);
void OledUtils_RenderESPResetSentNotification(void);
void OledUtils_RenderESPCheckConnectionRequiredNotification(void);
void OledUtils_RenderESPCheckConnectionOkNotification(void);
void OledUtils_RenderESPFirmwareOkNotification(void);
void OledUtils_RenderESPResetOkNotification(void);
void OledUtils_RenderESPBootNotification(void);
void OledUtils_SetEspFirmwareAscii(const uint8_t *ascii, uint8_t len);
void OledUtils_SetDisplayIP(const IPStruct_t *ip);
void OledUtils_RenderCommandReceivedNotification(void);
void OledUtils_RenderControllerConnected(void);
void OledUtils_RenderControllerDisconnected(void);
void OledUtils_RenderStartupNotification(void);

void OledUtils_ShowNotificationMs(RenderFunction renderFn, uint16_t timeout_ms);
void OledUtils_ShowNotificationTicks10ms(RenderFunction renderFn, uint16_t timeout_ticks);
void OledUtils_ShowNotificationTicks10msEx(RenderFunction renderFn, uint16_t timeout_ticks, OledNotificationHook onShow, OledNotificationHook onHide);
void OledUtils_ShowNotificationMsEx(RenderFunction renderFn, uint16_t timeout_ms, OledNotificationHook onShow, OledNotificationHook onHide);
void OledUtils_DismissNotification(void);
void OledUtils_NotificationTick10ms(void);

void OledUtils_NotificationRestore(void);

#endif /* INC_OLED_UTILS_H_ */
