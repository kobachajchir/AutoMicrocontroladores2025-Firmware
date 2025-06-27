/*
 * oled_utils.c
 *
 *  Created on: Jun 16, 2025
 *      Author: kobac
 */

#include "oled_utils.h"
#include "mpu6050.h"
#include "globals.h"

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

void OledUtils_RenderLockState(OLED_HandleTypeDef *oled, uint8_t lockState)
{
	__NOP();
    if (lockState) {
        // pintar icono en overlay
        OLED_ActivateOverlay(oled, 0);
        OLED_DrawXBM(oled, LOCK_ICON_X, LOCK_ICON_Y, LOCK_ICON_W, LOCK_ICON_H, Icon_Lock_bits, true);
    } else {
        // sólo ocultar esa zona de overlay y restaurar el MAIN
        OLED_HideOverlayNow(oled, LOCK_ICON_X, LOCK_ICON_Y, LOCK_ICON_W, LOCK_ICON_H);
    }
}

/**
 * @brief  Dibuja las barras del gráfico IR.
 *         Limpia la región de las barras y las redibuja.
 * @param  oled      Puntero al handle del driver OLED
 * @param  irValues  Array de OLED_BAR_COUNT valores (0…4095)
 */
void OledUtils_DrawIRBars(OLED_HandleTypeDef *oled, volatile uint16_t *irValues)
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

/**
 * @brief  Dibuja todo el gráfico IR (leyenda + separador + barras) y arranca el envío.
 * @param  oled      Puntero al handle del driver OLED
 * @param  irValues  Array de OLED_BAR_COUNT valores (0…4095)
 */
void OledUtils_DrawIRGraph(OLED_HandleTypeDef *oled, volatile uint16_t *irValues)
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
    OledUtils_DrawIRBars(oled, irValues);
}

/**
 * @brief  Wrapper para la pantalla de Valores IR.
 */
void OledUtils_RenderValoresIR_Wrapper(void)
{
	if(!oledTask.first_Fn_Draw){
		menuSystem.allowPeriodicRefresh = true;
		encoder.allowEncoderInput = false;
		oledTask.pages_to_send    = OLED_PAGES_TO_SEND_NO_LIMIT;
		oledTask.pages_sent_count = 0;
		OledUtils_DrawIRGraph(&oledTask, sensor_raw_data); // La primera vez renderizo todo
		oledTask.first_Fn_Draw = true;
	}else{
		__NOP();
		OledUtils_DrawIRBars(&oledTask, sensor_raw_data); //La siguiente solo las barras
	}
}

/**
 * @brief  Wrapper para la pantalla de Valores MPU.
 */
void OledUtils_RenderValoresMPU_Wrapper(void)
{
	menuSystem.allowPeriodicRefresh = true;
	encoder.allowEncoderInput = false;
	oledTask.pages_to_send    = OLED_PAGES_TO_SEND_NO_LIMIT;
	oledTask.pages_sent_count = 0;
	OledUtils_RenderValoresMPUScreen(&oledTask, &mpuTask.data);
}


void OledUtils_RenderVerticalMenu(OLED_HandleTypeDef *oled, MenuSystem *ms) {
    SubMenu *menu = ms->currentMenu;
    uint8_t idx        = menu->currentItemIndex;
    uint8_t page       = idx / MENU_VISIBLE_ITEMS;
    uint8_t first      = page * MENU_VISIBLE_ITEMS;
    uint8_t last       = first + MENU_VISIBLE_ITEMS;
    if (last > menu->itemCount) last = menu->itemCount;

    // 1) Limpiar pantalla
    OLED_ClearBuffer(oled, false);
    OLED_SetFont(&oledTask, &Font_6x12_Min);

    // 2) Dibujar los items visibles
    for (uint8_t i = first; i < last; i++) {
        uint8_t local    = i - first;          // 0..2
        int     y        = MENU_ITEM_Y0 + local * MENU_ITEM_SPACING;
        bool    selected = (i == idx);

        // 2.1) Icono
        if (menu->items[i].icon) {
            OLED_DrawXBM(oled,
                         ICON_X,
                         y + ICON_Y_OFFSET,
                         ICON_WIDTH,
                         ICON_HEIGHT,
                         menu->items[i].icon,
                         false);
        }

        // 2.2) Texto
        // Texto a la derecha del icono
        OLED_SetCursor(oled,
                       ICON_X + ICON_WIDTH + 8,  // ajusta separación si quieres
                       y);
        OLED_DrawStr(oled,
                     menu->items[i].name,
                     false);

        // opcional: dibujar un recuadro si está seleccionado
        if (selected) {
            OLED_DrawFrame(oled,
                          0, y - 3,           // ajusta si tu fontHeight difiere
                          OLED_WIDTH, 20,
                          false);
        }
    }

    // 3) Dibujar cursor junto al item seleccionado
    {
        uint8_t cursorPos = idx % MENU_VISIBLE_ITEMS;  // 0..2
        int     cy        = MENU_ITEM_Y0 + cursorPos * MENU_ITEM_SPACING;
        OLED_DrawXBM(oled,
                     CURSOR_X,
                     cy,
                     CURSOR_WIDTH,
                     CURSOR_HEIGHT,
                     Icon_Cursor_bits,
                     false);
    }
}

void OledUtils_Clear(OLED_HandleTypeDef *oled, bool is_overlay) {
    OLED_ClearBuffer(oled, is_overlay);
}

/**
 * @brief  Dibuja un ítem de menú (texto + icono + cursor) directamente desde el MenuItem.
 */
void OledUtils_DrawItem(OLED_HandleTypeDef *oled, const MenuItem *item, uint8_t y, bool selected) {
    // 1) icono (directo del MenuItem)
	OLED_DrawFrame(oled, 0, y-3, 128, 20, false);
	uint8_t iconHeight;
	uint8_t iconWidth;
    if (item->icon) {
    	if      (item->icon == Icon_Wifi_bits)   { iconWidth = 19; iconHeight = 16; }
    	else if (item->icon == Icon_Volver_bits) { iconWidth = 18; iconHeight = 15; }
    	else if (item->icon == Icon_Sensors_bits){ iconWidth = 14; iconHeight = 16; }
    	else if (item->icon == Icon_Config_bits) { iconWidth = 16; iconHeight = 16; }
    	else {
    	    // valores por defecto si no coincide ningún icono
    	    iconWidth = 16; iconHeight = 16;
    	}
    	// luego lo dibujas:
    	OLED_DrawXBM(oled, 6, y, iconWidth, iconHeight, item->icon, false);
    }

    // 2) texto
    OLED_SetCursor(oled, 31, y);
    OLED_DrawStr(oled, (char*)item->name, false);

    // 3) cursor si está seleccionado
    if (selected) {
		iconWidth = 7;
		iconHeight = 16;
        OLED_DrawXBM(oled, 117, y, iconWidth, iconHeight, Icon_Cursor_bits, false);
    }
}

/**
 * @brief  Dibuja el menú en pantalla superpuesta (como en u8g2).
 */
void displayMenuCustom(MenuSystem *system)
{

}


void OledUtils_RenderDashboard(OLED_HandleTypeDef *oled)
{
    OLED_SetFont(oled, &Font_6x12_Min);
	if(IS_FLAG_SET(systemFlags2, WIFI_ACTIVE)){
	    OLED_DrawXBM(oled,  1,  2, 19, 16, Icon_Wifi_bits,   false);
	    OLED_SetCursor(oled,
	                   21,
	                   10  - oled->font->FontHeight);
	    OLED_DrawStr(oled, "Wifi name", false);

	    /* Texto “192.168.1.1” (fuente 6×12, baseline y=18) */
	    OLED_SetCursor(oled,
	                   18,
	                   20 - oled->font->FontHeight);
	    OLED_DrawStr(oled, "192.168.1.1", false);
	}else if(IS_FLAG_SET(systemFlags2, AP_ACTIVE)){
		OLED_DrawXBM(oled,	1,  2, 15, 16, Icon_APWifi_bits, false);
	    OLED_SetCursor(oled,
	                   21,
	                   10  - oled->font->FontHeight);
	    OLED_DrawStr(oled, "AP name", false);

	    /* Texto “192.168.1.1” (fuente 6×12, baseline y=18) */
	    OLED_SetCursor(oled,
	                   18,
	                   20 - oled->font->FontHeight);
	    OLED_DrawStr(oled, "10.100.100.1", false);
	}
	if(IS_FLAG_SET(systemFlags2, USB_ACTIVE)){
		OLED_DrawXBM(oled, 91,  2, 16, 16, Icon_USB_bits,    false);
	}
	if(IS_FLAG_SET(systemFlags2, RF_ACTIVE)){
		OLED_DrawXBM(oled, 110,  2, 17, 16, Icon_RF_bits,    false);
	}
    /* Texto “Wifi name” (fuente 6×12, baseline y=8) */
    /* Línea horizontal en y=20, de x=1 a x=125 */
    OLED_DrawHLine(oled,
                   1,    /* x */
                   20,   /* y */
                   125,  /* width = 125 pixels (1..125 inclusive) */
                   false);
    /* Texto “Inicio” (fuente 14×17, baseline y=37) */
    OLED_SetFont(oled, &Font_14x17_Min);
    OLED_SetCursor(oled,
                   25,
                   42 - oled->font->FontHeight);
    OLED_DrawStr(oled, "Inicio", false);
    /* Icono “UserBtn” (15×16 px) */
    OLED_DrawXBM(oled,
                 4,    /* x */
                 44,   /* y */
                 15,   /* width */
                 16,   /* height */
                 Icon_UserBtn_bits,
                 false);

    /* Texto “Menu” (fuente 6×12, baseline y=55) */
    OLED_SetFont(oled, &Font_6x12_Min);
    OLED_SetCursor(oled,
                   24,        /* x */
                   58 - oled->font->FontHeight /* y = baseline - font height */);
    OLED_DrawStr(oled, "Menu", false);

    /* Texto “Modo” (misma fuente, baseline y=55) */
    OLED_SetCursor(oled,
                   83,
                   58 - oled->font->FontHeight);
    OLED_DrawStr(oled, "Modo", false);

    /* Icono “Encoder” (13×13 px) */
    OLED_DrawXBM(oled,
                 111,
                 45,
                 13,
                 13,
                 Icon_Encoder_bits,
                 false);
}


/**
 * @brief  Dibuja “VALORES MPU” centrado tanto horizontal como verticalmente.
 * @param  oled     Puntero al handle del driver OLED
 * @param  mpuData  (no usado aquí, pero se deja por consistencia)
 */
void OledUtils_RenderValoresMPUScreen(OLED_HandleTypeDef *oled, MPU6050_IntData_t *mpuData)
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



