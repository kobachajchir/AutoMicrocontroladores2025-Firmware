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

// wrapper
void OledUtils_PrintMainBuffer_Wrapper(void) {
    OLED_PrintBuffer(&oledTask, false);
    OLED_SendBuffer(&oledTask);
}

void OledUtils_PrintOverlayBuffer_Wrapper(void)
{
    OLED_PrintBuffer(&oledTask, true);
    OLED_SendBuffer(&oledTask);
}

// Activa refresco continuo: nunca llamamos al callback
// y permitimos que el MainTask siga solicitando render
void OledUtils_EnableContinuousRender(OLED_HandleTypeDef *oled) {
    menuSystem.allowPeriodicRefresh = true;
    OLED_SetRenderCompleteCb(oled, NULL);
}

// Desactiva refresco continuo: bloquea el periodic y
// fija el callback para que se dispare al terminar
void OledUtils_DisableContinuousRender(OLED_HandleTypeDef *oled,
		On_OLED_RenderPagesComplete cb) {
    menuSystem.allowPeriodicRefresh = false;
    OLED_SetRenderCompleteCb(oled, cb);
}

void OledUtils_RenderLockState(OLED_HandleTypeDef *oled, uint8_t lockState)
{
    if (lockState) {
        // → Enter lock mode: turn on overlay for the full screen,
        //   then schedule the lock-screen render
        OLED_ActivateOverlay(oled, 0);
        menuSystem.renderFn   = OledUtils_RenderLockScreen;
        menuSystem.renderFlag = true;
    }
    else {
        // → Exit lock mode: hide overlay *entirely*
        // clear any residual overlay pixels
        OLED_ClearBuffer(oled, true);
        oled->overlay_hide_now = true;

        // schedule one final main-buffer flush
        menuSystem.renderFn   = OledUtils_PrintMainBuffer_Wrapper;
        menuSystem.renderFlag = true;
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

void OledUtils_RenderRadarGraph(OLED_HandleTypeDef *oled, volatile uint16_t *irValues)
{
	OLED_DrawCircle(oled, 63, 60, 60, false);
	OLED_DrawCircle(oled, 63, 60, 40, false);
	OLED_DrawXBM(oled, 40, 48, 48, 13, Icon_Car_bits, false);
}

void OledUtils_RenderRadarGraph_Objs(OLED_HandleTypeDef *oled, volatile uint16_t *irValues)
{
	/**/
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

void OledUtils_MotorTest_Complete(OLED_HandleTypeDef *oled){
	OLED_SetFont(oled, &Font_5x10_Min);
	OLED_SetCursor(oled, 3, 11-oled->font->FontHeight);
	OLED_DrawStr(oled, "Probando Motor 1", false);
	OLED_SetCursor(oled, 41, 29-oled->font->FontHeight);
	OLED_DrawStr(oled, "Adelante", false);
	OLED_DrawFrame(oled, 5, 34, 116, 11, false);
	OLED_DrawBox(oled, 5, 34, 100, 11, false);
	OLED_SetCursor(oled, 56, 57-oled->font->FontHeight);
	OLED_DrawStr(oled, "100", false);
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
    // 1) dibujar el marco alrededor del ítem
    OLED_DrawFrame(oled, 0, y - 3, 128, 20, false);

    // 2) dibujar el icono (si lo hay)
    if (item->icon) {
        uint8_t iconW, iconH;
        if      (item->icon == Icon_Wifi_bits)    { iconW = 19; iconH = 16; }
        else if (item->icon == Icon_Volver_bits)  { iconW = 18; iconH = 15; }
        else if (item->icon == Icon_Sensors_bits) { iconW = 14; iconH = 16; }
        else if (item->icon == Icon_Config_bits)  { iconW = 16; iconH = 16; }
        else {
            iconW = 16; iconH = 16;
        }
        OLED_DrawXBM(oled, 6, y - 1, iconW, iconH, item->icon, false);
    }

    // 3) dibujar el texto
    OLED_SetCursor(oled, 31, y);
    OLED_DrawStr(oled, (char*)item->name, false);

    // 4) dibujar o borrar el cursor
    const uint8_t CUR_W = 7, CUR_H = 16;
    const uint8_t CUR_X = 117, CUR_Y = y;
    if (selected) {
        OLED_DrawXBM(oled, CUR_X, CUR_Y, CUR_W, CUR_H, Icon_Cursor_bits, false);
    } else {
        // limpia el área anterior del cursor
        OLED_ClearBox(oled, CUR_X, CUR_Y, CUR_W, CUR_H, false);
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
/**
 * @brief  Dibuja todos los valores del MPU (raw y ángulos) alineados en 3 filas.
 */
void OledUtils_RenderValoresMPUScreen(OLED_HandleTypeDef *oled, MPU6050_Handle_t *mpu)
{
    char buf[8];
    const uint8_t fw = oled->font->FontWidth;
    const uint8_t fh = oled->font->FontHeight;
    const uint8_t val_width  = fw * 5;  // espacio para “-16384” o “-180”
    const uint8_t val_height = fh;
    // Coordenadas de las tres columnas de valores:
    const uint8_t colA = 20;
    const uint8_t colG = 50;
    const uint8_t colD = 100;
    // Ancho total desde columna A hasta el final de DEG:
    const uint8_t total_width = (colD + val_width) - colA;

    // 1) Título “Sensor MPU”
    OLED_SetFont(oled, &Font_6x12_Min);
    OLED_SetCursor(oled, 5, 14 - fh);
    OLED_DrawStr(oled, "Sensor MPU", false);

    // 2) Estado de calibración
    // Limpiamos toda la zona donde se mostrará el estado
    OLED_ClearBox(oled,
                  70,                  // x
                  14 - fh,             // y = baseline – fontHeight
                  57,   // ancho fijo 70+57 = 127 = ancho total de la fila
                  fh,                  // alto = fontHeight
                  false);
    // Ahora pintamos el estado
    OLED_SetCursor(oled, 70, 14 - fh);
    OLED_DrawStr(oled, IS_FLAG_SET(mpu->flags, CALIBRATED)
            ? "Calib."
            : "No calib.", false);

    // 3) Cabeceras de columna: A (acel), G (gyro), DEG (ángulo)
    OLED_SetCursor(oled, colA, 28 - fh); OLED_DrawStr(oled, "A",   false);
    OLED_SetCursor(oled, colG, 28 - fh); OLED_DrawStr(oled, "G",   false);
    OLED_SetCursor(oled, colD, 28 - fh); OLED_DrawStr(oled, "DEG", false);

    // 4) Etiquetas de fila X/Y/Z (columna etiquetas)
    OLED_SetCursor(oled, 5,  38 - fh); OLED_DrawStr(oled, "X", false);
    OLED_SetCursor(oled, 5,  48 - fh); OLED_DrawStr(oled, "Y", false);
    OLED_SetCursor(oled, 5,  58 - fh); OLED_DrawStr(oled, "Z", false);

    // 5) Para cada fila (X, Y, Z) limpiamos toda la zona de valores y luego dibujamos A, G y DEG

    // fila X
    OLED_ClearBox(oled, colA, 38 - fh, total_width, val_height, false);
    //   Aceleración X
    OLED_SetCursor(oled, colA, 38 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.accel_x_mg);
    OLED_DrawStr(oled, buf, false);
    //   Giroscopio X
    OLED_SetCursor(oled, colG, 38 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.gyro_x_mdps);
    OLED_DrawStr(oled, buf, false);
    //   Ángulo X
    {
      int32_t ang = (mpu->angle_x_md/1000) % 360;
      if (ang > 180) ang -= 360;
      if (ang < -180) ang += 360;
      OLED_SetCursor(oled, colD, 38 - fh);
      snprintf(buf, sizeof(buf), "%d", ang);
      OLED_DrawStr(oled, buf, false);
    }

    // fila Y
    OLED_ClearBox(oled, colA, 48 - fh, total_width, val_height, false);
    OLED_SetCursor(oled, colA, 48 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.accel_y_mg);
    OLED_DrawStr(oled, buf, false);
    OLED_SetCursor(oled, colG, 48 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.gyro_y_mdps);
    OLED_DrawStr(oled, buf, false);
    {
      int32_t ang = (mpu->angle_y_md/1000) % 360;
      if (ang > 180) ang -= 360;
      if (ang < -180) ang += 360;
      OLED_SetCursor(oled, colD, 48 - fh);
      snprintf(buf, sizeof(buf), "%d", ang);
      OLED_DrawStr(oled, buf, false);
    }

    // fila Z
    OLED_ClearBox(oled, colA, 58 - fh, total_width, val_height, false);
    OLED_SetCursor(oled, colA, 58 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.accel_z_mg);
    OLED_DrawStr(oled, buf, false);
    OLED_SetCursor(oled, colG, 58 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.gyro_z_mdps);
    OLED_DrawStr(oled, buf, false);
    {
      int32_t ang = (mpu->angle_z_md/1000) % 360;
      if (ang > 180) ang -= 360;
      if (ang < -180) ang += 360;
      OLED_SetCursor(oled, colD, 58 - fh);
      snprintf(buf, sizeof(buf), "%d", ang);
      OLED_DrawStr(oled, buf, false);
    }
}

void OledUtils_RenderLockScreen(OLED_HandleTypeDef *oled) {
    if (!oled) return;

    // 1) Activar overlay sin timeout (dura hasta que lo escondamos)
    OLED_ActivateOverlay(oled, 0);

    // 2) Limpiar sólo la capa OVERLAY
    OLED_ClearBuffer(oled, true);

    // 3) Texto “Pantalla” (fuente 14×17, dibuja desde esquina superior,
    //    así que restamos la altura para que la línea base quede a y=42)
    OLED_SetFont(oled, &Font_14x17_Min);
    OLED_SetCursor(oled,
                   28,
                   42 - oled->font->FontHeight);
    OLED_DrawStr(oled, "Pantalla", true);

    // 4) Texto “bloqueada” (misma fuente, línea base a y=56)
    OLED_SetCursor(oled,
                   23,
                   56 - oled->font->FontHeight);
    OLED_DrawStr(oled, "bloqueada", true);

    // 5) Icono de candado en overlay
    OLED_DrawXBM(oled,
                 LOCK_ICON_X, LOCK_ICON_Y,
                 LOCK_ICON_W, LOCK_ICON_H,
                 Icon_Lock_bits,
                 true);
}


