/*
 * oled_utils.c
 *
 *  Created on: Jun 16, 2025
 *      Author: kobac
 */

#include "oled_utils.h"
#include "mpu6050.h"
#include "globals.h"

#define OLED_BAR_COUNT 8
#define BAR_WIDTH 12
#define Y_OFFSET    5    // margen superior
#define Y_SPACING   22   // separación vertical entre ítems
#define ITEM_HEIGHT 21   // altura del recuadro de selección

const uint8_t text_bar_x[OLED_BAR_COUNT] = {
    4, 19, 34, 49,
    64, 79, 94, 109
};
const uint8_t bar_x[OLED_BAR_COUNT] = {
    5, 20, 35, 50,
    65, 80, 95, 110
};

// ============================================================================
// Wrappers de render
// ============================================================================

/**
 * @brief  Wrapper genérico para renderizar el menú actual.
 *         Llama a displayMenuCustom() con menuSystem.
 */
void renderMenu_Wrapper(void)
{
    displayMenuCustom(&menuSystem);
}

/**
 * @brief  Wrapper para la pantalla de Valores IR.
 */
void renderValoresIR_Wrapper(void)
{
	if(!oledTask.first_Fn_Draw){
		OLED_DrawIRGraph(&oledTask, sensor_raw_data); // La primera vez renderizo todo
		oledTask.first_Fn_Draw = true;
	}else{
		__NOP();
		OLED_DrawIRBars(&oledTask, sensor_raw_data); //La siguiente solo las barras
	}
}

/**
 * @brief  Wrapper para la pantalla de Valores MPU.
 */
void renderValoresMPU_Wrapper(void)
{
	renderValoresMPUScreen(&oledTask, &mpuTask.data);
}

/**
 * @brief  Wrapper para la pantalla principal (dashboard).
 */
void renderDashboard_Wrapper(void)
{
    renderDashboard();
}

// ============================================================================
// displayMenuCustom: versión “u8g2 style” con nuestro driver
// ============================================================================

/**
 * @brief  Dibuja el menú en pantalla superpuesta (como en u8g2).
 */
void displayMenuCustom(MenuSystem *system)
{
    if (!system) return;
    SubMenu *menu = system->currentMenu;
    uint8_t  idx  = menu->currentItemIndex;

    // 1) limpia buffer principal y modos
    OLED_ClearBuffer(&oledTask, false);
    OLED_SetBitmapMode(&oledTask, true);
    OLED_SetFontMode(&oledTask,   true);
    OLED_SetFont(&oledTask, &Font_6x12_Min);

    // 2) calculo dinámico de Y para anterior, actual y siguiente
    int16_t yPrev = Y_OFFSET + (idx - 1)*Y_SPACING;
    int16_t yCurr = Y_OFFSET +    idx *Y_SPACING;
    int16_t yNext = Y_OFFSET + (idx + 1)*Y_SPACING;

    // 3) ítem anterior (si existe)
    if (idx > 0) {
        // centramos verticalmente restando la mitad de la altura de la fuente
        OLED_SetCursor(&oledTask,
                       23,
                       yPrev - (oledTask.font->FontHeight/2));
        OLED_DrawStr(&oledTask, menu->items[idx-1].name, false);
        if (menu->items[idx-1].icon) {
            OLED_DrawXBM(&oledTask,
                         4,
                         yPrev - 8,    // icono de 16px ligeramente por encima
                         16, 16,
                         menu->items[idx-1].icon,
                         false);
        }
    }

    // 4) recuadro de selección centrado sobre el ítem actual
    int16_t frameY = yCurr - (ITEM_HEIGHT/2);
    OLED_DrawFrame(&oledTask,
                   1, frameY,
                   119, ITEM_HEIGHT,
                   false);

    // 5) ítem actual
    OLED_SetCursor(&oledTask,
                   23,
                   yCurr - (oledTask.font->FontHeight/2));
    OLED_DrawStr(&oledTask, menu->items[idx].name, false);
    if (menu->items[idx].icon) {
        OLED_DrawXBM(&oledTask,
                     4,
                     yCurr - 8,
                     16, 16,
                     menu->items[idx].icon,
                     false);
    }

    // 6) ítem siguiente (si existe)
    if (idx + 1 < menu->itemCount) {
        OLED_SetCursor(&oledTask,
                       23,
                       yNext - (oledTask.font->FontHeight/2));
        OLED_DrawStr(&oledTask,
                     menu->items[idx+1].name,
                     false);
        if (menu->items[idx+1].icon) {
            OLED_DrawXBM(&oledTask,
                         4,
                         yNext - 8,
                         16, 16,
                         menu->items[idx+1].icon,
                         false);
        }
    }

    // 7) indicador lateral a la altura del ítem actual
    OLED_DrawBox(&oledTask,
                124,
                yCurr - 3,
                2,
                6,
                false);
}


void renderDashboard(void)
{
    /* 1) Icono (15×16 px) */
    OLED_DrawXBM(&oledTask,           /* destino      */
                 4, 44,               /* x , y        */
                 15, 16,              /* w , h        */
                 userBtn_Icon,
                 false);              /* sin overlay  */

    /* 2) Texto “Menu” (fuente 6×12) */
    OLED_SetFont(&oledTask, &Font_6x12_Min);
    OLED_SetCursor(&oledTask, 24, 47);
    OLED_DrawStr(&oledTask, "Menu", false);

    /* 3) Texto “Inicio” (fuente 14×17) */
    OLED_SetFont(&oledTask, &Font_16x29_Min);
    OLED_SetCursor(&oledTask, 22, 10);
    OLED_DrawStr(&oledTask, "Inicio", false);
}

/**
 * @brief  Dibuja “VALORES MPU” centrado tanto horizontal como verticalmente.
 * @param  oled     Puntero al handle del driver OLED
 * @param  mpuData  (no usado aquí, pero se deja por consistencia)
 */
void renderValoresMPUScreen(OLED_HandleTypeDef *oled, MPU6050_IntData_t *mpuData)
{
    if (!oled) return;

    // 1) Limpiar toda la pantalla MAIN
    OLED_ClearBuffer(oled, false);

    // 2) Seleccionar fuente
    OLED_SetFont(oled, &Font_6x12_Min);

    // 3) Calcular posición para centrar
    const char *msg = "VALORES MPU";
    uint16_t str_w  = strlen(msg) * oled->font->FontWidth;
    uint8_t  str_h  = oled->font->FontHeight;
    uint8_t  x      = (OLED_WIDTH  - str_w) / 2;
    uint8_t  y      = (OLED_HEIGHT - str_h) / 2;

    // 4) Dibujar el texto
    OLED_SetCursor(oled, x, y);
    OLED_DrawStr(oled, msg, false);

    // 5) Iniciar envío de páginas pendientes
    OLED_SendBuffer(oled);
}


/**
 * @brief  Dibuja todo el gráfico IR (leyenda + separador + barras) y arranca el envío.
 * @param  oled      Puntero al handle del driver OLED
 * @param  irValues  Array de OLED_BAR_COUNT valores (0…4095)
 */
void OLED_DrawIRGraph(OLED_HandleTypeDef *oled, volatile uint16_t *irValues)
{
    if (!oled) return;

    // 1) Limpiar toda la pantalla MAIN
    OLED_ClearBuffer(oled, false);

    // 2) Ajustar modos de dibujo
    OLED_SetBitmapMode(oled, true);
    OLED_SetFontMode(oled,   true);

    // 3) Leyenda "IR1"… "IR8" con fuente 8×10
    OLED_SetFont(oled, &Font_5x10_Min);
    const uint8_t fh    = oled->font->FontHeight;
    const uint8_t sep_y = fh + 1;
    char label[5];
    for (int i = 0; i < OLED_BAR_COUNT; i++) {
        snprintf(label, sizeof(label), "IR%d", i+1);
        int16_t lx = text_bar_x[i] - (oled->font->FontWidth / 2);
        if (lx < 0) lx = 0;
        OLED_SetCursor(oled, lx, 0);
        OLED_DrawStr(oled, label, false);
    }

    // 4) Separador horizontal
    OLED_DrawLineXY(oled,
                    0,           sep_y,
                    OLED_WIDTH-1, sep_y,
                    false);

    // 5) Dibujar y enviar las barras IR
    OLED_DrawIRBars(oled, irValues);
}

/**
 * @brief  Dibuja las barras del gráfico IR.
 *         Limpia la región de las barras y las redibuja.
 * @param  oled      Puntero al handle del driver OLED
 * @param  irValues  Array de OLED_BAR_COUNT valores (0…4095)
 */
void OLED_DrawIRBars(OLED_HandleTypeDef *oled, volatile uint16_t *irValues)
{
    if (!oled) return;

    // Cálculo de la zona de barras
    const uint8_t fh     = oled->font->FontHeight;
    const uint8_t sep_y  = fh + 2;
    const uint8_t bar_y0 = sep_y + 1;
    const uint8_t maxH   = OLED_HEIGHT - bar_y0;

    // 1) Limpiar todo el área de barras (ancho completo, desde bar_y0 hasta abajo)

    // 2) Dibujar cada barra IR
    for (uint8_t i = 0; i < OLED_BAR_COUNT; i++) {
        uint16_t v = irValues[i] > 4095 ? 4095 : irValues[i];
        uint8_t  h = (uint32_t)v * maxH / 4095;
        uint8_t  y0 = bar_y0 + (maxH - h);
        OLED_ClearBox(oled,
                    bar_x[i],  // posición X de la barra
                    bar_y0,    // siempre desde el inicio de las barras
                    BAR_WIDTH, // ancho fijo
                    maxH,      // altura máxima de la zona de barras
                    false      // MAIN
                );
        OLED_DrawBox(oled,
                     bar_x[i],  // posición X de la barra
                     y0,        // posición Y calculada
                     BAR_WIDTH, // ancho fijo
                     h,         // altura mapeada
                     false);    // MAIN
    }
    __NOP();
}



