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

// ─── Wrappers de render ──────────────────────────────────────────────────────

/** Renderiza el menú actual con displayMenuCustom */
void renderMenu_Wrapper(void);

/** Renderiza el menú padre tras llamar a volver() */
void renderBack_Wrapper(void);

/** Renderiza la pantalla de valores IR */
void renderValoresIR_Wrapper(void);

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

/**
 * @brief  Versión básica de displayMenu: limpia con clearScreen y llama a renderFn.
 * @param  system  Puntero al MenuSystem activo
 */
void displayMenu(MenuSystem *system);

void renderDashboard(void);

void renderValoresIR(void);

void renderValoresMPUScreen(OLED_HandleTypeDef *oled, MPU6050_IntData_t *mpuData);

void OLED_DrawIRGraph(OLED_HandleTypeDef *oled, volatile uint16_t *irValues);

void OLED_DrawIRBars(OLED_HandleTypeDef *oled, volatile uint16_t *irValues);

#endif // OLED_MENU_H


#endif /* INC_OLED_UTILS_H_ */
