/*
 * oled_utils.h
 *
 *  Created on: Jun 16, 2025
 *      Author: kobac
 */

#ifndef INC_OLED_UTILS_H_
#define INC_OLED_UTILS_H_

// oled_menu.h
#ifndef OLED_MENU_H
#define OLED_MENU_H

#include "menusystem.h"
#include "oled_ssd1306_dma.h"
#include "oled_utils.h"      // para OLED_DrawIRGraph, etc.
#include "globals.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief  Wrapper para la pantalla de Valores MPU.
 */
void renderValoresMPU_Wrapper(void);

/** Renderiza la pantalla principal (dashboard: "HOLA MUNDO" centrado) */
void renderDashboard_Wrapper(void);

// ─── Funciones de despliegue ─────────────────────────────────────────────────

/**
 * @brief  Dibuja el menú en pantalla superpuesta (estilo u8g2) usando oledTask.
 * @param  system  Puntero al MenuSystem activo
 */
void displayMenuCustom(MenuSystem *system);

void OledUtils_RenderVerticalMenu(OLED_HandleTypeDef *oled, MenuSystem *ms);

void OledUtils_RenderDashboard(OLED_HandleTypeDef *oled);

void OledUtils_Clear(OLED_HandleTypeDef *oled, bool is_overlay);

void OledUtils_DrawItem(OLED_HandleTypeDef *oled, const MenuItem *item, uint8_t y, bool selected);

void OledUtils_RenderValoresMPUScreen(OLED_HandleTypeDef *oled, MPU6050_Handle_t *mpu);

void OledUtils_DrawIRGraph(OLED_HandleTypeDef *oled, volatile uint16_t *irValues);

void OledUtils_DrawIRBars(OLED_HandleTypeDef *oled, volatile uint16_t *irValues);

void OledUtils_MotorTest_Complete(OLED_HandleTypeDef *oled);

void OledUtils_MotorTest_Changes(OLED_HandleTypeDef *oled);

void OledUtils_RenderLockScreen(OLED_HandleTypeDef *oled);

#endif // OLED_MENU_H


#endif /* INC_OLED_UTILS_H_ */
