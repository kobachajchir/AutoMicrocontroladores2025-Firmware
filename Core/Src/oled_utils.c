/*
 * oled_utils.c
 *
 *  Created on: Jun 16, 2025
 *      Author: kobac
 */

#include "oled_utils.h"

#define OLED_BAR_COUNT 8
#define BAR_WIDTH 12
#define Y_OFFSET    5    // margen superior
#define Y_SPACING   22   // separación vertical entre ítems
#define ITEM_HEIGHT 21   // altura del recuadro de selección

const uint8_t bar_x[OLED_BAR_COUNT] = {
    0*BAR_WIDTH, 1*BAR_WIDTH, 2*BAR_WIDTH, 3*BAR_WIDTH,
    4*BAR_WIDTH, 5*BAR_WIDTH, 6*BAR_WIDTH, 7*BAR_WIDTH
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
    menuSystem.renderFlag = true;
}

/**
 * @brief  Wrapper para la pantalla de Valores IR.
 */
void renderValoresIR_Wrapper(void)
{
    OLED_DrawIRGraph(&oledTask, sensor_raw_data);
    menuSystem.renderFlag = true;
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
    OLED_SetFont(&oledTask, &Font_11x18);

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

    // 8) enviamos la pantalla
    OLED_SendBuffer(&oledTask);
}


void renderDashboard(void)
{
    // 2) Selecciona fuente y centra texto
    OLED_SetFont(&oledTask, &Font_11x18);
    const char *msg = "HOLA MUNDO";
    uint16_t w = strlen(msg) * oledTask.font->FontWidth;
    uint8_t  x = (OLED_WIDTH  - w) / 2;
    uint8_t  y = (OLED_HEIGHT - oledTask.font->FontHeight) / 2;
    OLED_SetCursor(&oledTask, x, y);
    OLED_DrawStr(&oledTask, msg, false);
}

void renderValoresIR(void)
{
    // 1) Limpia el main buffer
    OLED_ClearBuffer(&oledTask, false);
    // 2) Dibuja el gráfico IR
    OLED_DrawIRGraph(&oledTask, sensor_raw_data);
}

/**
 * @brief  Dibuja todo el gráfico IR (leyenda + separador + barras) y envía el buffer.
 * @param  oled      Puntero al handle del driver OLED
 * @param  irValues  Array de OLED_BAR_COUNT valores (0…4095)
 */
void OLED_DrawIRGraph(OLED_HandleTypeDef *oled, volatile uint16_t *irValues)
{
    // 1) pantalla limpia y modos
    OLED_ClearBuffer(oled, false);
    OLED_SetBitmapMode(oled, true);
    OLED_SetFontMode(oled,   true);

    // 2) leyenda en 8x10
    OLED_SetFont(oled, &Font_8x10);
    const uint8_t fh    = oled->font->FontHeight;
    const uint8_t sep_y = fh + 1;

    // 3) "IR1"… "IR8"
    char label[5];
    for (int i = 0; i < OLED_BAR_COUNT; i++) {
        snprintf(label, sizeof(label), "IR%d", i+1);
        int16_t lx = bar_x[i] - (oled->font->FontWidth / 2);
        if (lx < 0) lx = 0;
        OLED_SetCursor(oled, lx, 0);
        OLED_DrawStr(oled, label, false);
    }

    // 4) separador
    OLED_DrawLineXY(oled,
                    0,           sep_y,
                    OLED_WIDTH-1, sep_y,
                    false);

    // 5) barras
    OLED_DrawIRBars(oled, irValues);

}

/**
 * @brief  Dibuja las barras del gráfico IR.
 * @param  oled      Puntero al handle del driver OLED
 * @param  irValues  Array de OLED_BAR_COUNT valores (0…4095)
 */
void OLED_DrawIRBars(OLED_HandleTypeDef *oled, volatile uint16_t *irValues)
{
    // Altura de la fuente y separación vertical
    const uint8_t fh     = oled->font->FontHeight;
    const uint8_t sep_y  = fh + 1;
    const uint8_t bar_y0 = sep_y + 1;
    const uint8_t maxH   = OLED_HEIGHT - bar_y0;

    for (int i = 0; i < OLED_BAR_COUNT; i++) {
        // 1) Clamp del valor IR a 0…4095
        uint16_t v = irValues[i] > 4095 ? 4095 : irValues[i];
        // 2) Mapeo lineal a la altura disponible
        uint8_t h = (uint32_t)v * maxH / 4095;
        // 3) Y de inicio (invertido para crecer hacia arriba)
        uint8_t y0 = bar_y0 + (maxH - h);

        // 4) Dibujar la barra
        OLED_DrawBox(oled,
                    bar_x[i],   // posición X pre-calculada
                    y0,         // posición Y
                    BAR_WIDTH,  // ancho de cada barra
                    h,          // alto mapeado
                    false);     // false = main buffer
    }
}

