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
#include <stdbool.h>
#include <stdint.h>

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

#endif /* INC_OLED_UTILS_H_ */
