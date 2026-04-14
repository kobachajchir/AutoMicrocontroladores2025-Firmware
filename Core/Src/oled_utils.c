/*
 * oled_utils.c
 *
 *  Created on: Jun 16, 2025
 *      Author: kobac
 */

#include "oled_utils.h"
#include "globals.h"
#include "mpu6050.h"
#include "ssd1306.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

const uint8_t text_bar_x[OLED_BAR_COUNT] = {
    4, 19, 34, 49,
    64, 79, 94, 109
};
const uint8_t bar_x[OLED_BAR_COUNT] = {
    5, 20, 35, 50,
    65, 80, 95, 110
};

static const FontDef *oled_font = &Font_7x10;
static uint8_t oled_lock_state = LOCK_STATE_LOCKED;
static char oled_wifi_connected_ssid[WIFI_SSID_MAX_LEN + 1u] = "WiFi";

static void OledUtils_SetScreenCode(ScreenCode_t screen_code, ScreenReportSource_t source)
{
    MenuSys_SetCurrentScreenCode(&menuSystem, screen_code, source);
}

static void Oled_SetFont(const FontDef *font)
{
    oled_font = font;
}

static uint8_t Oled_FontHeight(void)
{
    return oled_font->FontHeight;
}

static uint8_t Oled_FontWidth(void)
{
    return oled_font->FontWidth;
}

static void Oled_DrawStr(const char *text)
{
    __NOP(); // BREAKPOINT: impresión de texto en buffer
    ssd1306_SetColor(White);
    ssd1306_WriteString((char *)text, *oled_font);
}

static void Oled_DrawStrMax(const char *text, uint8_t maxChars)
{
    char buf[22];
    uint8_t i = 0u;

    if (!text) {
        text = "";
    }

    while (text[i] != '\0' && i < maxChars && i < (uint8_t)(sizeof(buf) - 1u)) {
        buf[i] = text[i];
        i++;
    }
    buf[i] = '\0';
    Oled_DrawStr(buf);
}

static void Oled_ClearBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    ssd1306_SetColor(Black);
    ssd1306_FillRect(x, y, w, h);
    ssd1306_SetColor(White);
}

static uint8_t Oled_IpIsValid(const IPStruct_t *ip)
{
    return ip &&
           (ip->block1 != 0u ||
            ip->block2 != 0u ||
            ip->block3 != 0u ||
            ip->block4 != 0u);
}

static void Oled_FormatIp(const IPStruct_t *ip, char *buf, size_t len)
{
    if (!buf || len == 0u) {
        return;
    }

    if (!Oled_IpIsValid(ip)) {
        snprintf(buf, len, "Sin IP");
        return;
    }

    snprintf(buf, len, "%u.%u.%u.%u",
             (unsigned)ip->block1,
             (unsigned)ip->block2,
             (unsigned)ip->block3,
             (unsigned)ip->block4);
}

static void Oled_DrawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    ssd1306_SetColor(White);
    ssd1306_FillRect(x, y, w, h);
}

static void Oled_DrawFrame(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    ssd1306_SetColor(White);
    ssd1306_DrawRect(x, y, w, h);
}

static void Oled_DrawXBM(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bits)
{
    __NOP(); // BREAKPOINT: impresión de bitmap en buffer
    ssd1306_SetColor(White);
    ssd1306_DrawBitmap(x, y, w, h, bits);
}

static void Oled_DrawXBM_MSB(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bits)
{
    __NOP(); // BREAKPOINT: impresión de bitmap en buffer
    ssd1306_SetColor(White);
    ssd1306_DrawBitmap_MSB(x, y, w, h, bits);
}

// ============================================================================
// Wrappers de render
// ============================================================================

// wrapper
void OledUtils_PrintMainBuffer_Wrapper(void) {
    ssd1306_UpdateScreen();
}

void OledUtils_PrintOverlayBuffer_Wrapper(void)
{
    ssd1306_UpdateScreen();
}

// Activa refresco continuo: permitimos que el MainTask siga solicitando render
void OledUtils_EnableContinuousRender(void) {
    menuSystem.allowPeriodicRefresh = true;
}

// Desactiva refresco continuo: bloquea el periodic
void OledUtils_DisableContinuousRender(void) {
    menuSystem.allowPeriodicRefresh = false;
}

void OledUtils_RenderLockState(uint8_t lockState)
{
    if (lockState > LOCK_STATE_ENTER_PIN) {
        lockState = LOCK_STATE_LOCKED;
    }

    oled_lock_state = lockState;
    menuSystem.renderFn = OledUtils_RenderLockScreen;
    menuSystem.renderFlag = true;
}

static void OledUtils_RenderNotificationProgress(const OledNotificationState *notif)
{
    if (!notif || notif->totalTicks == 0) {
        return;
    }

    const uint16_t elapsed = (notif->totalTicks > notif->timeoutTicks)
        ? (uint16_t)(notif->totalTicks - notif->timeoutTicks)
        : 0U;
    const uint16_t width = (uint16_t)((elapsed * SSD1306_WIDTH) / notif->totalTicks);

    Oled_ClearBox(0, 0, SSD1306_WIDTH, 1);
    ssd1306_SetColor(White);
    ssd1306_DrawHorizontalLine(0, 0, (int16_t)width);
}

static void OledUtils_ExecuteNotificationHook(OledNotificationHook hook)
{
    if (hook) {
        hook();
    }
}

void OledUtils_RenderNotification_Wrapper(void)
{
    OledNotificationState *notif = &oledHandle.notification;

    if (notif->renderFn && notif->needsFullRender) {
        notif->renderFn();
        notif->needsFullRender = false;
    }

    OledUtils_RenderNotificationProgress(notif);
}

void OledUtils_NotificationRestore(void)
{
    OledNotificationState *notif = &oledHandle.notification;

    OledUtils_ExecuteNotificationHook(notif->onHide);

    if (notif->suspended.valid) {
        if (menuSystem.clearScreen) {
            menuSystem.clearScreen();
        }
        notif->active = true;
        notif->renderFn = notif->suspended.renderFn;
        notif->timeoutTicks = notif->suspended.remainingTicks;
        notif->totalTicks = notif->suspended.remainingTicks;
        notif->needsFullRender = true;
        notif->onShow = notif->suspended.onShow;
        notif->onHide = notif->suspended.onHide;
        notif->suspended.valid = false;

        menuSystem.renderFn = OledUtils_RenderNotification_Wrapper;
        menuSystem.allowPeriodicRefresh = false;
        menuSystem.renderFlag = true;
        return;
    }

    notif->active = false;
    notif->renderFn = NULL;
    notif->timeoutTicks = 0;
    notif->totalTicks = 0;
    notif->needsFullRender = false;
    notif->onShow = NULL;
    notif->onHide = NULL;

    menuSystem.renderFn = notif->previousRenderFn;
    menuSystem.allowPeriodicRefresh = notif->previousAllowPeriodicRefresh;
    if (menuSystem.clearScreen) {
        menuSystem.clearScreen();
    }
    menuSystem.renderFlag = true;
}

void OledUtils_ShowNotificationTicks10msEx(RenderFunction renderFn,
                                          uint16_t timeout_ticks,
                                          OledNotificationHook onShow,
                                          OledNotificationHook onHide)
{
    OledNotificationState *notif = &oledHandle.notification;

    if (!renderFn) {
        return;
    }

    if (timeout_ticks == 0) {
        timeout_ticks = 1;
    }

    if (notif->active) {
        notif->suspended.valid = true;
        notif->suspended.renderFn = notif->renderFn;
        notif->suspended.remainingTicks = notif->timeoutTicks;
        notif->suspended.onShow = notif->onShow;
        notif->suspended.onHide = notif->onHide;
    } else {
        notif->previousRenderFn = menuSystem.renderFn;
        notif->previousAllowPeriodicRefresh = menuSystem.allowPeriodicRefresh;
        notif->previousRenderFlag = menuSystem.renderFlag;
    }

    notif->active = true;
    notif->renderFn = renderFn;
    notif->timeoutTicks = timeout_ticks;
    notif->totalTicks = timeout_ticks;
    notif->needsFullRender = true;
    notif->onShow = onShow;
    notif->onHide = onHide;

    OledUtils_ExecuteNotificationHook(notif->onShow);

    menuSystem.renderFn = OledUtils_RenderNotification_Wrapper;
    menuSystem.allowPeriodicRefresh = false;
    menuSystem.renderFlag = true;
}

void OledUtils_ShowNotificationTicks10ms(RenderFunction renderFn, uint16_t timeout_ticks)
{
    OledUtils_ShowNotificationTicks10msEx(renderFn, timeout_ticks, NULL, NULL);
}

void OledUtils_ShowNotificationMsEx(RenderFunction renderFn,
                                    uint16_t timeout_ms,
                                    OledNotificationHook onShow,
                                    OledNotificationHook onHide)
{
    uint16_t ticks = (uint16_t)((timeout_ms + 9U) / 10U);
    OledUtils_ShowNotificationTicks10msEx(renderFn, ticks, onShow, onHide);
}

void OledUtils_ShowNotificationMs(RenderFunction renderFn, uint16_t timeout_ms)
{
    OledUtils_ShowNotificationMsEx(renderFn, timeout_ms, NULL, NULL);
}

bool OledUtils_IsNotificationShowing(RenderFunction renderFn)
{
    return oledHandle.notification.active &&
           oledHandle.notification.renderFn == renderFn;
}

void OledUtils_ReplaceNotificationMs(RenderFunction renderFn, uint16_t timeout_ms)
{
    OledNotificationState *notif = &oledHandle.notification;
    RenderFunction previousRenderFn = menuSystem.renderFn;
    bool previousAllowPeriodicRefresh = menuSystem.allowPeriodicRefresh;
    bool previousRenderFlag = menuSystem.renderFlag;

    if (!renderFn) {
        return;
    }

    if (notif->active || notif->suspended.valid) {
        previousRenderFn = notif->previousRenderFn ? notif->previousRenderFn : previousRenderFn;
        previousAllowPeriodicRefresh = notif->previousAllowPeriodicRefresh;
        previousRenderFlag = notif->previousRenderFlag;

        notif->active = false;
        notif->renderFn = NULL;
        notif->timeoutTicks = 0u;
        notif->totalTicks = 0u;
        notif->needsFullRender = false;
        notif->onShow = NULL;
        notif->onHide = NULL;
        notif->suspended.valid = false;
        notif->suspended.renderFn = NULL;
        notif->suspended.remainingTicks = 0u;
        notif->suspended.onShow = NULL;
        notif->suspended.onHide = NULL;

        menuSystem.renderFn = previousRenderFn;
        menuSystem.allowPeriodicRefresh = previousAllowPeriodicRefresh;
        menuSystem.renderFlag = previousRenderFlag;
    }

    OledUtils_ShowNotificationMs(renderFn, timeout_ms);
}

void OledUtils_DismissNotification(void)
{
    if (!oledHandle.notification.active && !oledHandle.notification.suspended.valid) {
        return;
    }

    oledHandle.notification.timeoutTicks = 0;
    OledUtils_NotificationRestore();
}

void OledUtils_NotificationTick10ms(void)
{
    OledNotificationState *notif = &oledHandle.notification;

    if (!notif->active) {
        return;
    }

    if (notif->timeoutTicks > 0) {
        notif->timeoutTicks--;
    }

    if (notif->timeoutTicks == 0) {
        OledUtils_NotificationRestore();
    } else {
        menuSystem.renderFlag = true;
    }
}

/**
 * @brief  Dibuja las barras del gráfico IR.
 *         Limpia la región de las barras y las redibuja.
 * @param  irValues  Array de OLED_BAR_COUNT valores (0…4095)
 */
void OledUtils_DrawIRBars(volatile uint16_t *irValues)
{
    // Cálculo de la zona de barras
    const uint8_t fh     = Oled_FontHeight();
    const uint8_t sep_y  = fh + 2;
    const uint8_t bar_y0 = sep_y + 1;
    const uint8_t maxH   = SSD1306_HEIGHT - bar_y0;

    // Dibujar cada barra IR
    for (uint8_t i = 0; i < OLED_BAR_COUNT; i++) {
        uint16_t v = irValues[i] > 4095 ? 4095 : irValues[i];
        uint8_t  h = (uint32_t)v * maxH / 4095;
        uint8_t  y0 = bar_y0 + (maxH - h);
        Oled_ClearBox(
            bar_x[i],  // posición X de la barra
            bar_y0,    // siempre desde el inicio de las barras
            BAR_WIDTH, // ancho fijo
            maxH       // altura máxima de la zona de barras
        );
        Oled_DrawBox(
            bar_x[i],  // posición X de la barra
            y0,        // posición Y calculada
            BAR_WIDTH, // ancho fijo
            h          // altura mapeada
        );
    }
    __NOP();
}

void OledUtils_RenderRadarGraph(volatile uint16_t *irValues)
{
    (void)irValues;
    ssd1306_SetColor(White);
    ssd1306_DrawCircle(63, 60, 60);
    ssd1306_DrawCircle(63, 60, 40);
    Oled_DrawXBM(40, 48, 48, 13, Icon_Car_bits);
}

void OledUtils_RenderRadarGraph_Objs(volatile uint16_t *irValues)
{
    (void)irValues;
}

/**
 * @brief  Dibuja todo el gráfico IR (leyenda + separador + barras) y arranca el envío.
 * @param  irValues  Array de OLED_BAR_COUNT valores (0…4095)
 */
void OledUtils_DrawIRGraph(volatile uint16_t *irValues)
{
    // 1) Limpiar toda la pantalla
    OledUtils_Clear();

    // 2) Leyenda "IR1"… "IR8" con fuente 8×10
    Oled_SetFont(&Font_7x10);
    const uint8_t fh    = Oled_FontHeight();
    const uint8_t sep_y = fh + 1;
    char label[5];
    for (int i = 0; i < OLED_BAR_COUNT; i++) {
        snprintf(label, sizeof(label), "IR%d", i+1);
        int16_t lx = text_bar_x[i] - (Oled_FontWidth() / 2);
        if (lx < 0) lx = 0;
        ssd1306_SetCursor(lx, 0);
        Oled_DrawStr(label);
    }

    // 3) Separador horizontal
    ssd1306_DrawLine(0, sep_y, SSD1306_WIDTH - 1, sep_y);

    // 4) Dibujar barras IR
    OledUtils_DrawIRBars(irValues);
}

void OledUtils_MotorTest_Complete(void) {
    // 1) Determinar motor seleccionado desde nibble alto
    uint8_t sel = NIBBLEH_GET_STATE(motorTask.motorData.flags);
    const char *motorStr;
    switch (sel) {
        case MOTORLEFT_SELECTED:  motorStr = "Motor Izquierdo"; break;
        case MOTORRIGHT_SELECTED: motorStr = "Motor Derecho";   break;
        case MOTORNONE_SELECTED:  motorStr = "Ambos Motores";   break;
        default:                  motorStr = "Motor ?";         break;
    }

    // 2) Obtener velocidad y dirección según selección
    uint8_t speed = 0;
    Motor_Direction_t dir = MOTOR_DIR_FORWARD;
    if (sel == MOTORLEFT_SELECTED) {
        speed = motorTask.motorData.motorLeft.motorSpeed;
        dir   = motorTask.motorData.motorLeft.motorDirection;
    }
    else if (sel == MOTORRIGHT_SELECTED) {
        speed = motorTask.motorData.motorRight.motorSpeed;
        dir   = motorTask.motorData.motorRight.motorDirection;
    }
    else { // Ambos
        uint8_t sL = motorTask.motorData.motorLeft.motorSpeed;
        uint8_t sR = motorTask.motorData.motorRight.motorSpeed;
        speed = (uint16_t)(sL + sR) / 2;
        dir   = (motorTask.motorData.motorLeft.motorDirection == motorTask.motorData.motorRight.motorDirection)
                ? motorTask.motorData.motorLeft.motorDirection
                : motorTask.motorData.motorLeft.motorDirection;
    }

    const char *dirStr = (dir == MOTOR_DIR_FORWARD) ? "Adelante" : "Atras";
    char speedStr[8];
    Oled_SetFont(&Font_7x10);

    // Línea 1: nombre del motor
    ssd1306_SetCursor(3, 11 - Oled_FontHeight());
    Oled_DrawStr(motorStr);

    // Línea 2: dirección
    ssd1306_SetCursor(41, 29 - Oled_FontHeight());
    Oled_DrawStr(dirStr);

    // Línea 3: marco de barra de velocidad
    Oled_DrawFrame(5, 34, 116, 11);

    // Línea 4: relleno de barra o estado PARADO
    if (IS_FLAG_SET(motorTask.motorData.flags, ENABLE_MOVEMENT)) {
        uint8_t barWidth = (uint16_t)speed * 116 / 255;
        Oled_DrawBox(5, 34, barWidth, 11);

        uint8_t speedPct = (uint16_t)speed * 100 / 255;
        snprintf(speedStr, sizeof(speedStr), "%3u%%", speedPct);
    } else {
        snprintf(speedStr, sizeof(speedStr), "PARADO");
    }

    // Línea 5: porcentaje o texto
    ssd1306_SetCursor(50, 57 - Oled_FontHeight());
    Oled_DrawStr(speedStr);
}

void OledUtils_MotorTest_Changes(void) {
    // 1) Determinar motor y texto
    uint8_t sel = NIBBLEH_GET_STATE(motorTask.motorData.flags);
    const char *motorStr = (sel == MOTORLEFT_SELECTED)  ? "Probando Motor 1" :
                           (sel == MOTORRIGHT_SELECTED) ? "Probando Motor 2" :
                                                          "Probando Ambos";
    // 2) Dirección
    Motor_Direction_t dir = (sel == MOTORRIGHT_SELECTED)
                            ? motorTask.motorData.motorRight.motorDirection
                            : motorTask.motorData.motorLeft.motorDirection;
    const char *dirStr = (dir == MOTOR_DIR_FORWARD) ? "Adelante" : "Atras";

    char speedStr[8];
    Oled_SetFont(&Font_7x10);

    // Línea 1: motor
    Oled_ClearBox(0, 11 - Oled_FontHeight(), 128, Oled_FontHeight());
    ssd1306_SetCursor(3,  11 - Oled_FontHeight());
    Oled_DrawStr(motorStr);

    // Línea 2: dirección
    Oled_ClearBox(0, 29 - Oled_FontHeight(), 128, Oled_FontHeight());
    ssd1306_SetCursor(41, 29 - Oled_FontHeight());
    Oled_DrawStr(dirStr);

    // Línea 3: barra
    Oled_ClearBox(5,  34, 116, 11);
    Oled_DrawFrame(5, 34, 116, 11);

    // Línea 4: relleno o PARADO
    if (IS_FLAG_SET(motorTask.motorData.flags, ENABLE_MOVEMENT)) {
        uint8_t speed = (sel == MOTORRIGHT_SELECTED)
                        ? motorTask.motorData.motorRight.motorSpeed
                        : motorTask.motorData.motorLeft.motorSpeed;
        uint8_t barW = (uint16_t)speed * 116 / 255;
        Oled_DrawBox(5, 34, barW, 11);

        uint8_t pct = (uint16_t)speed * 100 / 255;
        snprintf(speedStr, sizeof(speedStr), "%3u%%", pct);
    } else {
        snprintf(speedStr, sizeof(speedStr), "PARADO");
    }

    // Línea 5: porcentaje o estado
    Oled_ClearBox(0, 57 - Oled_FontHeight(), 128, Oled_FontHeight());
    ssd1306_SetCursor(50, 57 - Oled_FontHeight());
    Oled_DrawStr(speedStr);
}

void OledUtils_RenderVerticalMenu(MenuSystem *ms) {
    MenuSys_SetCurrentMenuScreenCode(ms);
    SubMenu *menu = ms->currentMenu;
    uint8_t idx        = menu->currentItemIndex;
    uint8_t page       = idx / MENU_VISIBLE_ITEMS;
    uint8_t first      = page * MENU_VISIBLE_ITEMS;
    uint8_t last       = first + MENU_VISIBLE_ITEMS;
    if (last > menu->itemCount) last = menu->itemCount;

    // 1) Limpiar pantalla
    OledUtils_Clear();
    Oled_SetFont(&Font_7x10);

    // 2) Dibujar los items visibles
    for (uint8_t i = first; i < last; i++) {
        uint8_t local    = i - first;          // 0..2
        int     y        = MENU_ITEM_Y0 + local * MENU_ITEM_SPACING;
        bool    selected = (i == idx);

        // 2.1) Icono
        if (menu->items[i].icon) {
            Oled_DrawXBM(
                ICON_X,
                y + ICON_Y_OFFSET,
                ICON_WIDTH,
                ICON_HEIGHT,
                menu->items[i].icon
            );
        }

        // 2.2) Texto
        ssd1306_SetCursor(
            ICON_X + ICON_WIDTH + 8,
            y
        );
        Oled_DrawStr(menu->items[i].name);

        // opcional: dibujar un recuadro si está seleccionado
        if (selected) {
            Oled_DrawFrame(0, y - 3, SSD1306_WIDTH, 20);
        }
    }

    // 3) Dibujar cursor junto al item seleccionado
    {
        uint8_t cursorPos = idx % MENU_VISIBLE_ITEMS;  // 0..2
        int     cy        = MENU_ITEM_Y0 + cursorPos * MENU_ITEM_SPACING;
        Oled_DrawXBM(CURSOR_X, cy, CURSOR_WIDTH, CURSOR_HEIGHT, Icon_Cursor_bits);
    }
}

void OledUtils_Clear(void) {
    __NOP(); // BREAKPOINT: limpieza del buffer OLED
    ssd1306_Clear();
}

/**
 * @brief  Dibuja un ítem de menú (texto + icono + cursor) directamente desde el MenuItem.
 */
void OledUtils_DrawItem(const MenuItem *item, uint8_t y, bool selected)
{
    Oled_DrawFrame(0, y - 3, 128, 20);

    if (item->icon) {
        uint8_t iconW, iconH;
        if      (item->icon == Icon_Wifi_bits)    { iconW = 19; iconH = 16; }
        else if (item->icon == Icon_Volver_bits)  { iconW = 16; iconH = 16; }
        else if (item->icon == Icon_Sensors_bits) { iconW = 14; iconH = 16; }
        else if (item->icon == Icon_Config_bits)  { iconW = 16; iconH = 16; }
        else {
            iconW = 16; iconH = 16;
        }
        Oled_DrawXBM(6, y - 1, iconW, iconH, item->icon);
    }

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(27, y+3);
    Oled_DrawStr((char*)item->name);

    const uint8_t CUR_W = 7, CUR_H = 16;
    const uint8_t CUR_X = 117, CUR_Y = y;
    if (selected) {
        Oled_DrawXBM(CUR_X, CUR_Y, CUR_W, CUR_H, Icon_Cursor_bits);
    } else {
        Oled_ClearBox(CUR_X, CUR_Y, CUR_W, CUR_H);
    }
}



/**
 * @brief  Dibuja el menú en pantalla.
 */
void displayMenuCustom(MenuSystem *system)
{
    (void)system;
}


void OledUtils_RenderDashboard(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CORE_DASHBOARD, SCREEN_REPORT_SOURCE_RENDER);
    Oled_SetFont(&Font_7x10);
    const uint8_t fh = Oled_FontHeight();
    char ipStr[20];

    if (IS_FLAG_SET(systemFlags2, ESP_PRESENT)) {
        if (IS_FLAG_SET(systemFlags2, WIFI_ACTIVE)) {
            Oled_FormatIp(&espWifiConnection.staIp, ipStr, sizeof(ipStr));
            ssd1306_SetCursor(1, 10 - fh);
            Oled_DrawStrMax(espWifiConnection.staSsid, 14u);
            ssd1306_SetCursor(1, 20 - fh);
            Oled_DrawStrMax(ipStr, 15u);
        } else if (IS_FLAG_SET(systemFlags2, AP_ACTIVE)) {
            Oled_FormatIp(&espWifiConnection.apIp, ipStr, sizeof(ipStr));
            Oled_DrawXBM(1, 2, 15, 16, Icon_APWifi_bits);
            ssd1306_SetCursor(21, 10 - fh);
            Oled_DrawStr("AP name");
            ssd1306_SetCursor(18, 20 - fh);
            Oled_DrawStrMax(ipStr, 15u);
        }
    } else {
        ssd1306_SetCursor(2, 10 - fh);
        Oled_DrawStr("No ESP");
        ssd1306_SetCursor(2, 20 - fh);
        Oled_DrawStr("Sin WiFi");
    }

    if (IS_FLAG_SET(systemFlags2, USB_ACTIVE)) {
        Oled_DrawXBM(91, 2, 16, 16, Icon_USB_bits);
    }
    if (IS_FLAG_SET(systemFlags2, RF_ACTIVE)) {
        Oled_DrawXBM(110, 2, 17, 16, Icon_RF_bits);
    }
    ssd1306_DrawHorizontalLine(1, 20, 125);

    Oled_SetFont(&Font_11x18);
    ssd1306_SetCursor(2, 42 - Oled_FontHeight());
    Oled_DrawStr("Inicio");

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(80, 38 - Oled_FontHeight());
    switch (GET_CAR_MODE()) {
		case IDLE_MODE:
		    Oled_DrawStr("IDLE");
			break;
		case FOLLOW_MODE:
		    Oled_DrawStr("FOLLOW");
			break;
		case TEST_MODE:
		    Oled_DrawStr("TEST");
			break;
		default:
			Oled_DrawStr("DEF");
			break;
	}

    Oled_DrawXBM(4, 47, 15, 16, Icon_UserBtn_bits);

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(23, 61 - Oled_FontHeight());
    Oled_DrawStr("Menu");

    ssd1306_SetCursor(80, 61 - Oled_FontHeight());
    Oled_DrawStr("Modo");

    Oled_DrawXBM(111, 48, 13, 13, Icon_Encoder_bits);
}

void OledUtils_RenderTestScreen(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_SERVICE_TEST_SCREEN, SCREEN_REPORT_SOURCE_RENDER);

}

void OledUtils_RenderStartupNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CORE_STARTUP, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_SetFont(&Font_11x18);

    Oled_DrawXBM_MSB(0, 0, 128, 64, Icon_Auto_bits);
}

/**
 * @brief Pantalla principal de información del proyecto
 */
void OledUtils_RenderProyectScreen(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_SETTINGS_ABOUT_PROJECT, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_SetColor(White);

    // Texto principal "Proyecto Auto" con fuente grande
    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(40, 19 - fh_grande);
    Oled_DrawStr("Auto");

    // Texto secundario "Microcontroladores" con fuente pequeña
    Oled_SetFont(&Font_7x10);
    const uint8_t fh_pequena = Oled_FontHeight();
    ssd1306_SetCursor(0, 30 - fh_pequena);
    Oled_DrawStr("Microcontroladores");

    // Icono de información (15x16 píxeles)
    Oled_DrawXBM(20, 2, 15, 16, Icon_Info_bits);

    // Flecha derecha (4x7 píxeles)

    // Texto "Github"
    ssd1306_SetCursor(60, 57 - fh_pequena);
    Oled_DrawStr("Github");
    Oled_DrawXBM(105, 49, 4, 7, Arrow_Right_bits);

    // Nombre "Koba"
    ssd1306_SetCursor(20, 42 - fh_pequena);
    Oled_DrawStr("Koba Chajchir");

    Oled_SetFont(&Font_11x18);
    ssd1306_SetCursor(4, 62 - fh_grande);
    Oled_DrawStr("2026");
}

/**
 * @brief Pantalla secundaria con QR del repositorio
 */
void OledUtils_RenderProyectInfoScreen(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_SETTINGS_ABOUT_REPO, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_SetColor(White);

    // Flecha izquierda (4x7 píxeles)
    Oled_DrawXBM(12, 50, 4, 7, Arrow_Left_bits);

    // Texto "Repo" con fuente pequeña
    Oled_SetFont(&Font_7x10);
    const uint8_t fh = Oled_FontHeight();
    ssd1306_SetCursor(11, 13 - fh);
    Oled_DrawStr("Repo");
    ssd1306_SetCursor(2, 25 - fh);
    Oled_DrawStr("Firmware");
    ssd1306_SetCursor(2, 37 - fh);
    Oled_DrawStr("STM32");

    // QR Code del repositorio (64x64 píxeles)
    ssd1306_SetColor(White);
    ssd1306_DrawQR_Fixed(64, 0, QRCode_Github_bits);  // Centrado en 128x64
}

void OledUtils_RenderModeChange_Full(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CORE_MODE_CHANGE, SCREEN_REPORT_SOURCE_RENDER);
    // Título "Modo" con fuente grande
    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    const uint8_t fw_grande = Oled_FontWidth();

    // Centrar título "Modo" (4 caracteres)
    uint8_t titulo_x = (SSD1306_WIDTH - (4 * fw_grande)) / 2;
    ssd1306_SetCursor(titulo_x, 20 - fh_grande);
    Oled_DrawStr("Modo");

    // Dibujar modo actual
    OledUtils_RenderModeChange_ModeOnly();

    // Flechas centradas verticalmente con el texto
    uint8_t modo_y = 42 - fh_grande;
    uint8_t arrow_y = modo_y + (fh_grande / 2) - 3;
    Oled_DrawXBM(14, arrow_y, 4, 7, Arrow_Left_bits);
    Oled_DrawXBM(110, arrow_y, 4, 7, Arrow_Right_bits);

    // Texto "Confirmar" e icono
    Oled_SetFont(&Font_7x10);
    const uint8_t fh_pequena = Oled_FontHeight();
    const uint8_t fw_pequena = Oled_FontWidth();

    const uint8_t icon_width = 13;
    const uint8_t icon_height = 13;
    const uint8_t separacion = 3;

    // "Confirmar" = 9 caracteres
    uint8_t texto_width = 9 * fw_pequena;
    uint8_t total_width = icon_width + separacion + texto_width;

    // Centrar el conjunto
    uint8_t start_x = (SSD1306_WIDTH - total_width) / 2;
    uint8_t icon_x = start_x;
    uint8_t icon_y = 50;
    uint8_t texto_x = icon_x + icon_width + separacion;
    uint8_t texto_y = icon_y + (icon_height / 2) - (fh_pequena / 2);

    // Dibujar
    Oled_DrawXBM(icon_x, icon_y, icon_width, icon_height, Icon_Encoder_bits);
    ssd1306_SetCursor(texto_x, texto_y);
    Oled_DrawStr("Confirmar");
}

void OledUtils_RenderModeChange_ModeOnly(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CORE_MODE_CHANGE, SCREEN_REPORT_SOURCE_RENDER);
    // Fuente grande para el modo
    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    const uint8_t fw_grande = Oled_FontWidth();

    // Limpiar zona del modo (usar "FOLLOW" = 6 caracteres, la más larga)
    const uint8_t max_modo_len = 6;  // "FOLLOW" es la más larga
    uint8_t max_modo_width = max_modo_len * fw_grande;
    uint8_t modo_clear_x = (SSD1306_WIDTH - max_modo_width) / 2;
    uint8_t modo_y = 42 - fh_grande;

    // Limpiar zona del texto del modo
    Oled_ClearBox(modo_clear_x, modo_y, max_modo_width, fh_grande);

    // Obtener texto del modo actual
    const char *modo_texto;
    uint8_t modo_len;
    switch(auxCarMode)
    {
        case IDLE_MODE:
            modo_texto = "IDLE";
            modo_len = 4;
            break;

        case FOLLOW_MODE:
            modo_texto = "FOLLOW";
            modo_len = 6;
            break;

        case TEST_MODE:
            modo_texto = "TEST";
            modo_len = 4;
            break;

        default:
            modo_texto = "ERROR";
            modo_len = 5;
            break;
    }

    // Dibujar texto del modo centrado
    uint8_t modo_x = (SSD1306_WIDTH - (modo_len * fw_grande)) / 2;
    ssd1306_SetCursor(modo_x, modo_y);
    Oled_DrawStr(modo_texto);
}

/**
 * @brief  Dibuja todos los valores del MPU (raw y ángulos) alineados en 3 filas.
 */
void OledUtils_RenderValoresMPUScreen(MPU6050_Handle_t *mpu)
{
    OledUtils_SetScreenCode(SCREEN_CODE_SENSORS_MPU_VALUES, SCREEN_REPORT_SOURCE_RENDER);
    char buf[8];
    Oled_SetFont(&Font_7x10);
    const uint8_t fw = Oled_FontWidth();
    const uint8_t fh = Oled_FontHeight();
    const uint8_t val_width  = fw * 5;  // espacio para “-16384” o “-180”
    const uint8_t val_height = fh;
    const uint8_t colA = 20;
    const uint8_t colG = 50;
    const uint8_t colD = 100;
    const uint8_t total_width = (colD + val_width) - colA;

    // 1) Título “Sensor MPU”
    ssd1306_SetCursor(5, 14 - fh);
    Oled_DrawStr("Sensor MPU");

    // 2) Estado de calibración
    Oled_ClearBox(70, 14 - fh, 57, fh);
    ssd1306_SetCursor(70, 14 - fh);
    Oled_DrawStr(IS_FLAG_SET(mpu->flags, CALIBRATED)
            ? "Calib."
            : "No calib.");

    // 3) Cabeceras de columna: A (acel), G (gyro), DEG (ángulo)
    ssd1306_SetCursor(colA, 28 - fh); Oled_DrawStr("A");
    ssd1306_SetCursor(colG, 28 - fh); Oled_DrawStr("G");
    ssd1306_SetCursor(colD, 28 - fh); Oled_DrawStr("DEG");

    // 4) Etiquetas de fila X/Y/Z (columna etiquetas)
    ssd1306_SetCursor(5,  38 - fh); Oled_DrawStr("X");
    ssd1306_SetCursor(5,  48 - fh); Oled_DrawStr("Y");
    ssd1306_SetCursor(5,  58 - fh); Oled_DrawStr("Z");

    // fila X
    Oled_ClearBox(colA, 38 - fh, total_width, val_height);
    ssd1306_SetCursor(colA, 38 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.accel_x_mg);
    Oled_DrawStr(buf);
    ssd1306_SetCursor(colG, 38 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.gyro_x_mdps);
    Oled_DrawStr(buf);
    {
      int32_t ang = (mpu->angle_x_md/1000) % 360;
      if (ang > 180) ang -= 360;
      if (ang < -180) ang += 360;
      ssd1306_SetCursor(colD, 38 - fh);
      snprintf(buf, sizeof(buf), "%" PRId32, ang);
      Oled_DrawStr(buf);
    }

    // fila Y
    Oled_ClearBox(colA, 48 - fh, total_width, val_height);
    ssd1306_SetCursor(colA, 48 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.accel_y_mg);
    Oled_DrawStr(buf);
    ssd1306_SetCursor(colG, 48 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.gyro_y_mdps);
    Oled_DrawStr(buf);
    {
      int32_t ang = (mpu->angle_y_md/1000) % 360;
      if (ang > 180) ang -= 360;
      if (ang < -180) ang += 360;
      ssd1306_SetCursor(colD, 48 - fh);
      snprintf(buf, sizeof(buf), "%" PRId32, ang);
      Oled_DrawStr(buf);
    }

    // fila Z
    Oled_ClearBox(colA, 58 - fh, total_width, val_height);
    ssd1306_SetCursor(colA, 58 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.accel_z_mg);
    Oled_DrawStr(buf);
    ssd1306_SetCursor(colG, 58 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.gyro_z_mdps);
    Oled_DrawStr(buf);
    {
      int32_t ang = (mpu->angle_z_md/1000) % 360;
      if (ang > 180) ang -= 360;
      if (ang < -180) ang += 360;
      ssd1306_SetCursor(colD, 58 - fh);
      snprintf(buf, sizeof(buf), "%" PRId32, ang);
      Oled_DrawStr(buf);
    }
}

void OledUtils_RenderLockScreen(void)
{
    ScreenCode_t screen_code = SCREEN_CODE_WARNING_LOCKED;
    const char *message = "Pantalla bloqueada";
    uint8_t posX = 10u;

    switch (oled_lock_state) {
        case LOCK_STATE_PIN_INCORRECT:
            screen_code = SCREEN_CODE_WARNING_PIN_INCORRECT;
            message = "PIN incorrecto";
            posX = 22u;
            break;

        case LOCK_STATE_PIN_MODIFIED:
            screen_code = SCREEN_CODE_WARNING_PIN_MODIFIED;
            message = "PIN modificado";
            posX = 22u;
            break;

        case LOCK_STATE_ENTER_PIN:
            screen_code = SCREEN_CODE_WARNING_PIN_ENTRY;
            message = "Ingrese PIN";
            posX = 28u;
            break;

        case LOCK_STATE_LOCKED:
        default:
            break;
    }

    OledUtils_SetScreenCode(screen_code, SCREEN_REPORT_SOURCE_RENDER);
    OledUtils_Clear();
    ssd1306_SetColor(White);
    Oled_DrawXBM(57, 17, LOCK_ICON_W, LOCK_ICON_H, Icon_Lock_bits);

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(posX, 48 - Oled_FontHeight());
    Oled_DrawStr(message);
}

/**
 * @brief Pantalla de conexión ESP exitosa
 */
void OledUtils_ESPConnSucceeded(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_DIAG_ESP_CONN_SUCCEEDED, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_SetColor(White);

    // Texto "Conexion" con fuente grande
    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 36 - fh_grande);
    Oled_DrawStr("Conexion");

    // Texto "exitosa"
    ssd1306_SetCursor(32, 49 - fh_grande);
    Oled_DrawStr("exitosa");

    // Botón OK (13x13 píxeles)
    Oled_DrawXBM(111, 47, 13, 13, Icon_Encoder_bits);

    // Icono de check/tilde (14x16 píxeles)
    Oled_DrawXBM(57, 6, 14, 16, Icon_Checked_bits);
}

/**
 * @brief Pantalla de conexión ESP fallida
 */
void OledUtils_ESPConnFailed(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_DIAG_ESP_CONN_FAILED, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_SetColor(White);

    // Texto "Conexion" con fuente grande
    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 36 - fh_grande);
    Oled_DrawStr("Conexion");

    // Texto "fallida"
    ssd1306_SetCursor(32, 49 - fh_grande);
    Oled_DrawStr("fallida");

    // Botón OK (13x13 píxeles)
    Oled_DrawXBM(111, 47, 13, 13, Icon_Encoder_bits);

    // Icono de cruz/X (11x16 píxeles) - centrado igual que el check
    Oled_DrawXBM(58, 9, 11, 16, Icon_Crossed_bits);
}

/**
 * @brief Dibuja la escena completa del gráfico IR (leyenda + separador + barras vacías)
 *        Se llama solo una vez al entrar a la pantalla
 */
void OledUtils_RenderIRGraphScene(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_SENSORS_IR_VALUES, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_SetColor(White);

    // Línea separadora horizontal
    ssd1306_DrawLine(0, 9, 127, 9);

    // Leyenda "IR1" a "IR8" con fuente pequeña
    Oled_SetFont(&Font_7x10);
    const uint8_t fh = Oled_FontHeight();

    ssd1306_SetCursor(0, 7 - fh);
    Oled_DrawStr("IR1");

    ssd1306_SetCursor(16, 7 - fh);
    Oled_DrawStr("IR2");

    ssd1306_SetCursor(32, 7 - fh);
    Oled_DrawStr("IR3");

    ssd1306_SetCursor(48, 7 - fh);
    Oled_DrawStr("IR4");

    ssd1306_SetCursor(63, 7 - fh);
    Oled_DrawStr("IR5");

    ssd1306_SetCursor(79, 7 - fh);
    Oled_DrawStr("IR6");

    ssd1306_SetCursor(95, 7 - fh);
    Oled_DrawStr("IR7");

    ssd1306_SetCursor(111, 7 - fh);
    Oled_DrawStr("IR8");
}

/**
 * @brief Actualiza solo las barras del gráfico IR según los valores del sensor
 * @param sensorData Puntero al array de 8 valores de sensores IR (0-4095)
 */
void OledUtils_UpdateIRBars(volatile uint16_t *sensorData)
{
    const uint8_t barX[8] = {0, 16, 32, 48, 64, 80, 96, 112};
    const uint8_t barWidth = 14;
    const uint8_t barMaxHeight = 52;
    const uint8_t barBaseY = 12;  // Desde donde empiezan las barras

    ssd1306_SetColor(White);

    for (uint8_t i = 0; i < TCRT5000_NUM_SENSORS; i++) {
        // Limitar valor a 4095
        uint16_t value = (sensorData[i] > 4095) ? 4095 : sensorData[i];

        // Calcular altura de la barra proporcional al valor (0-4095 → 0-52 píxeles)
        uint8_t barHeight = (uint32_t)value * barMaxHeight / 4095;

        // Calcular Y inicial (desde abajo hacia arriba)
        uint8_t barY = barBaseY + (barMaxHeight - barHeight);

        // Limpiar toda la columna de la barra
        ssd1306_SetColor(Black);
        ssd1306_FillRect(barX[i], barBaseY, barWidth, barMaxHeight);

        // Dibujar la barra con la altura calculada
        ssd1306_SetColor(White);
        ssd1306_FillRect(barX[i], barY, barWidth, barHeight);
    }
}

/**
 * @brief Dibuja la escena completa del MPU (título + etiquetas)
 *        Se llama solo una vez al entrar a la pantalla
 */
void OledUtils_RenderMPUScene(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_SENSORS_MPU_VALUES, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_SetColor(White);

    // Título "Sensor MPU" con fuente grande
    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(5, 16 - fh_grande);
    Oled_DrawStr("Sensor MPU");

    // Etiquetas de acelerómetro y giroscopio con fuente pequeña
    Oled_SetFont(&Font_7x10);
    const uint8_t fh_pequena = Oled_FontHeight();

    // Columna izquierda: AX, AY, AZ
    ssd1306_SetCursor(5, 32 - fh_pequena);
    Oled_DrawStr("AX");

    ssd1306_SetCursor(5, 42 - fh_pequena);
    Oled_DrawStr("AY");

    ssd1306_SetCursor(5, 52 - fh_pequena);
    Oled_DrawStr("AZ");

    // Columna derecha: GX, GY, GZ
    ssd1306_SetCursor(68, 33 - fh_pequena);
    Oled_DrawStr("GX");

    ssd1306_SetCursor(68, 43 - fh_pequena);
    Oled_DrawStr("GY");

    ssd1306_SetCursor(68, 53 - fh_pequena);
    Oled_DrawStr("GZ");
}

/**
 * @brief Actualiza solo los valores del MPU en pantalla
 * @param mpu Puntero a la estructura del MPU con los datos
 */
void OledUtils_UpdateMPUValues(MPU6050_Handle_t *mpu)
{
    char buf[8];
    ssd1306_SetColor(White);

    Oled_SetFont(&Font_7x10);
    const uint8_t fh = Oled_FontHeight();
    const uint8_t fw = Oled_FontWidth();
    const uint8_t valueWidth = fw * 6;  // Ancho aproximado para "-32768"

    // Posiciones X para los valores
    const uint8_t axValueX = 24;
    const uint8_t gxValueX = 88;

    // Limpiar y actualizar AX
    Oled_ClearBox(axValueX, 32 - fh, valueWidth, fh);
    ssd1306_SetCursor(axValueX, 32 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.accel_x_mg);
    Oled_DrawStr(buf);

    // Limpiar y actualizar AY
    Oled_ClearBox(axValueX, 42 - fh, valueWidth, fh);
    ssd1306_SetCursor(axValueX, 42 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.accel_y_mg);
    Oled_DrawStr(buf);

    // Limpiar y actualizar AZ
    Oled_ClearBox(axValueX, 52 - fh, valueWidth, fh);
    ssd1306_SetCursor(axValueX, 52 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.accel_z_mg);
    Oled_DrawStr(buf);

    // Limpiar y actualizar GX
    Oled_ClearBox(gxValueX, 33 - fh, valueWidth, fh);
    ssd1306_SetCursor(gxValueX, 33 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.gyro_x_mdps);
    Oled_DrawStr(buf);

    // Limpiar y actualizar GY
    Oled_ClearBox(gxValueX, 43 - fh, valueWidth, fh);
    ssd1306_SetCursor(gxValueX, 43 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.gyro_y_mdps);
    Oled_DrawStr(buf);

    // Limpiar y actualizar GZ
    Oled_ClearBox(gxValueX, 53 - fh, valueWidth, fh);
    ssd1306_SetCursor(gxValueX, 53 - fh);
    snprintf(buf, sizeof(buf), "%d", mpu->data.gyro_z_mdps);
    Oled_DrawStr(buf);
}

/**
 * @brief Pantalla de configuración de tiempo de avisos
 * @param seconds Tiempo en segundos a mostrar
 */
void OledUtils_RenderWarningTimeConfig(uint8_t seconds)
{
    OledUtils_SetScreenCode(SCREEN_CODE_SETTINGS_WARNING_TIME, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_SetColor(White);

    // Fuente grande para el contenido
    Oled_SetFont(&Font_11x18);
    const uint8_t fh = Oled_FontHeight();

    // Texto "Tiempo de"
    ssd1306_SetCursor(21, 17 - fh);
    Oled_DrawStr("Tiempo de");

    // Texto "avisos"
    ssd1306_SetCursor(36, 33 - fh);
    Oled_DrawStr("avisos");

    // Texto con el valor de segundos (ejemplo: "10 segs.")
    char timeStr[12];
    snprintf(timeStr, sizeof(timeStr), "%u segs.", seconds);
    ssd1306_SetCursor(27, 53 - fh);
    Oled_DrawStr(timeStr);

    // Botón OK (13x13 píxeles)
    Oled_DrawXBM(112, 48, 13, 13, Icon_Encoder_bits);
}

/**
 * @brief Dibuja la escena completa de búsqueda WiFi con temporizador (una vez)
 */
void OledUtils_RenderWiFiSearchScene(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_SetColor(White);

    // Texto "Buscando" con fuente grande
    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 20 - fh_grande);
    Oled_DrawStr("Buscando");

    // Texto "redes wifi"
    ssd1306_SetCursor(2, 40 - fh_grande);
    Oled_DrawStr("redes wifi");

    // Texto "Cancelar" con fuente pequeña
    Oled_SetFont(&Font_7x10);
    const uint8_t fh_pequena = Oled_FontHeight();
    ssd1306_SetCursor(50, 60 - fh_pequena);
    Oled_DrawStr("Cancelar");

    // Botón OK/Encoder (13x13 píxeles)
    Oled_DrawXBM(112, 48, 13, 13, Icon_Encoder_bits);

    // Icono WiFi (19x16 píxeles)
    Oled_DrawXBM(2, 2, 19, 16, Icon_Wifi_100_bits);
}

/**
 * @brief Actualiza solo el temporizador de búsqueda WiFi
 * @param secondsRemaining Segundos restantes de búsqueda
 */
void OledUtils_UpdateWiFiSearchTimer(uint8_t secondsRemaining)
{
    Oled_SetFont(&Font_11x18);
    const uint8_t fh = Oled_FontHeight();
    const uint8_t fw = Oled_FontWidth();

    // Limpiar zona del temporizador (ancho suficiente para "99")
    const uint8_t timerWidth = fw * 2;
    Oled_ClearBox(4, 62 - fh, timerWidth, fh);

    // Dibujar nuevo valor
    char timeStr[4];
    snprintf(timeStr, sizeof(timeStr), "%u", secondsRemaining);
    ssd1306_SetCursor(4, 62 - fh);
    Oled_DrawStr(timeStr);
}

/**
 * @brief Pantalla de WiFi no conectado
 */
void OledUtils_RenderWiFiNotConnected(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_NOT_CONNECTED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_SetColor(White);

    // Texto "No hay red" con fuente grande
    Oled_SetFont(&Font_11x18);
    const uint8_t fh = Oled_FontHeight();
    ssd1306_SetCursor(7, 40 - fh);
    Oled_DrawStr("No hay red");

    // Icono WiFi desconectado (19x16 píxeles)
    Oled_DrawXBM(54, 3, 19, 16, Icon_Wifi_NotConnected_bits);

    // Texto "conectada"
    ssd1306_SetCursor(16, 55 - fh);
    Oled_DrawStr("conectada");
}

void OledUtils_SetWiFiConnectedSsid(const char *ssid)
{
    if (!ssid || ssid[0] == '\0') {
        strncpy(oled_wifi_connected_ssid, "WiFi", sizeof(oled_wifi_connected_ssid));
    } else {
        strncpy(oled_wifi_connected_ssid, ssid, sizeof(oled_wifi_connected_ssid) - 1u);
        oled_wifi_connected_ssid[sizeof(oled_wifi_connected_ssid) - 1u] = '\0';
    }

    strncpy(espWifiConnection.staSsid,
            oled_wifi_connected_ssid,
            sizeof(espWifiConnection.staSsid) - 1u);
    espWifiConnection.staSsid[sizeof(espWifiConnection.staSsid) - 1u] = '\0';
}

void OledUtils_RenderWiFiConnecting(const char *ssid)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTING, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(53, 14, 19, 16, Icon_Wifi_100_bits);

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(24, 42 - Oled_FontHeight());
    Oled_DrawStr("Conectando a");

    ssd1306_SetCursor(24, 54 - Oled_FontHeight());
    Oled_DrawStrMax(ssid, 14u);
}

void OledUtils_RenderWiFiConnected(const char *ssid, const char *ipAddress)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(53, 6, 19, 16, Icon_Wifi_100_bits);

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(24, 31 - Oled_FontHeight());
    Oled_DrawStr("Conectado a");

    ssd1306_SetCursor(24, 43 - Oled_FontHeight());
    Oled_DrawStrMax(ssid, 14u);

    ssd1306_SetCursor(16, 59 - Oled_FontHeight());
    Oled_DrawStr("IP:");

    ssd1306_SetCursor(34, 59 - Oled_FontHeight());
    Oled_DrawStrMax(ipAddress, 15u);
}

void OledUtils_RenderESPWifiConnectingNotification(void)
{
    OledUtils_RenderWiFiConnecting(oled_wifi_connected_ssid);
}

void OledUtils_RenderWiFiSearchCompleteNotification()
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_COMPLETE, SCREEN_REPORT_SOURCE_NOTIFICATION);
	ssd1306_Clear();
    ssd1306_SetColor(White);
    // Icono WiFi (19x16 píxeles) - misma posición que en búsqueda
    Oled_DrawXBM(2, 12, 19, 16, Icon_Wifi_100_bits);

    // Texto "Busqueda" con fuente grande
    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 30 - fh_grande);
    Oled_DrawStr("Busqueda");

    // Texto "completada"
    ssd1306_SetCursor(2, 50 - fh_grande);
    Oled_DrawStr("completada");
}


void OledUtils_RenderWiFiSearchCanceledNotification()
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_CANCELED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 19, 16, Icon_Wifi_NotConnected_bits);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 30 - fh_grande);
    Oled_DrawStr("Busqueda");

    ssd1306_SetCursor(10, 50 - fh_grande);
    Oled_DrawStr("cancelada");
}

void OledUtils_RenderControllerConnected(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_DIAG_CONTROLLER_CONNECTED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(46, 4, 37, 27, Icon_Controller_bits);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(28, 48 - fh_grande);
    Oled_DrawStr("Control");

    ssd1306_SetCursor(16, 63 - fh_grande);
    Oled_DrawStr("conectado");
}

void OledUtils_RenderControllerDisconnected(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_DIAG_CONTROLLER_DISCONNECTED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(28, 48 - fh_grande);
    Oled_DrawStr("Control");

    ssd1306_SetCursor(8, 63 - fh_grande);
    Oled_DrawStr("desconectado");

    ssd1306_DrawLine(46, 2, 84, 30);
    ssd1306_DrawLine(47, 2, 85, 30);
    ssd1306_DrawLine(47, 1, 85, 29);

    ssd1306_SetColor(Inverse);
    Oled_DrawXBM(46, 4, 37, 27, Icon_Controller_bits);
    ssd1306_SetColor(White);
}

void OledUtils_RenderCommandReceivedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_DIAG_COMMAND_RECEIVED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 19, 16, Icon_Wifi_100_bits);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 30 - fh_grande);
    Oled_DrawStr("ECHO");

    ssd1306_SetCursor(2, 50 - fh_grande);
    Oled_DrawStr("recibido");
}

void OledUtils_RenderWiFiStatus(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_STATUS, SCREEN_REPORT_SOURCE_RENDER);
    char ipStr[20];

    ssd1306_Clear();
    ssd1306_SetColor(White);

    if (IS_FLAG_SET(systemFlags2, WIFI_ACTIVE)) {
        Oled_FormatIp(&espWifiConnection.staIp, ipStr, sizeof(ipStr));

        Oled_DrawXBM(54, 4, 19, 16, Icon_Wifi_100_bits);
        Oled_SetFont(&Font_7x10);
        ssd1306_SetCursor(23, 31 - Oled_FontHeight());
        Oled_DrawStr("Conectado a");
        ssd1306_SetCursor(10, 44 - Oled_FontHeight());
        Oled_DrawStrMax(espWifiConnection.staSsid, 16u);
        ssd1306_SetCursor(8, 60 - Oled_FontHeight());
        Oled_DrawStrMax(ipStr, 16u);
    } else if (IS_FLAG_SET(systemFlags2, AP_ACTIVE)) {
        Oled_FormatIp(&espWifiConnection.apIp, ipStr, sizeof(ipStr));

        Oled_DrawXBM(54, 4, 15, 16, Icon_APWifi_bits);
        Oled_SetFont(&Font_7x10);
        ssd1306_SetCursor(28, 31 - Oled_FontHeight());
        Oled_DrawStr("AP activo");
        ssd1306_SetCursor(8, 48 - Oled_FontHeight());
        Oled_DrawStrMax(ipStr, 16u);
    } else {
        Oled_DrawXBM(54, 3, 19, 16, Icon_Wifi_NotConnected_bits);
        Oled_SetFont(&Font_11x18);
        ssd1306_SetCursor(7, 40 - Oled_FontHeight());
        Oled_DrawStr("No hay red");
        ssd1306_SetCursor(16, 55 - Oled_FontHeight());
        Oled_DrawStr("conectada");
    }
    return;
#if 0
	    // Texto "No hay red" con fuente grande
	    Oled_SetFont(&Font_11x18);
	    const uint8_t fh = Oled_FontHeight();
	    ssd1306_SetCursor(19, 38 - fh);
	    Oled_DrawStr("Red");

	    // Icono WiFi desconectado (19x16 píxeles)
	    Oled_DrawXBM(54, 8, 19, 16, Icon_Wifi_100_bits);
	}else{
		ssd1306_SetColor(White);

		char ipStr[20];

		Oled_SetFont(&Font_11x18);
		const uint8_t fh = Oled_FontHeight();
		ssd1306_SetCursor(19, 20 - fh);
		Oled_DrawStr("Red");

		Oled_DrawXBM(54, 4, 19, 16, Icon_Wifi_100_bits);

		Oled_SetFont(&Font_7x10);
		snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u",
				 (unsigned)espStaIp.block1,
				 (unsigned)espStaIp.block2,
				 (unsigned)espStaIp.block3,
				 (unsigned)espStaIp.block4);
		ssd1306_SetCursor(8, 40 - Oled_FontHeight());
		Oled_DrawStr(ipStr);

		ssd1306_SetCursor(3, 58 - Oled_FontHeight());
		Oled_DrawStr(espFirmwareVersion);
	}

#endif
}

void OledUtils_RenderESPBootReceivedNotification(void){
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_BOOT_RECEIVED, SCREEN_REPORT_SOURCE_NOTIFICATION);
	ssd1306_Clear();
	ssd1306_SetColor(White);

	Oled_DrawXBM_MSB(2, 12, 19, 16, Icon_Info_bits);

	Oled_SetFont(&Font_11x18);
	const uint8_t fh_grande = Oled_FontHeight();
	ssd1306_SetCursor(27, 30 - fh_grande);
	Oled_DrawStr("ESP");

	ssd1306_SetCursor(2, 50 - fh_grande);
	Oled_DrawStr("iniciada");
}


void OledUtils_RenderESPWifiConnectedNotification(void){
	char ipStr[20];

	Oled_FormatIp(&espWifiConnection.staIp, ipStr, sizeof(ipStr));
    OledUtils_RenderWiFiConnected(oled_wifi_connected_ssid, ipStr);
}



void OledUtils_RenderESPModeChangedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_MODE_CHANGED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 16, 16, Icon_Info_bits);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 30 - fh_grande);
    Oled_DrawStr("Modo");

    ssd1306_SetCursor(8, 50 - fh_grande);
    Oled_DrawStr("actualizado");
}

void OledUtils_RenderESPUsbConnectedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_USB_CONNECTED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 16, 16, Icon_Info_bits);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 30 - fh_grande);
    Oled_DrawStr("USB");

    ssd1306_SetCursor(2, 50 - fh_grande);
    Oled_DrawStr("conectado");
}

void OledUtils_RenderESPUsbDisconnectedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_USB_DISCONNECTED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 16, 16, Icon_Info_bits);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 30 - fh_grande);
    Oled_DrawStr("USB");

    ssd1306_SetCursor(8, 50 - fh_grande);
    Oled_DrawStr("desconect");
}

void OledUtils_RenderESPWebServerUpNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WEB_SERVER_UP, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 16, 16, Icon_Info_bits);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 30 - fh_grande);
    Oled_DrawStr("WEB");

    ssd1306_SetCursor(2, 50 - fh_grande);
    Oled_DrawStr("lista");
}

void OledUtils_RenderESPWebClientConnectedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_CONNECTED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 16, 16, Icon_Link_bits);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(37, 30 - fh_grande);
    Oled_DrawStr("Web");

    ssd1306_SetCursor(20, 50 - fh_grande);
    Oled_DrawStr("conectado");
}

void OledUtils_RenderESPWebClientDisconnectedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_DISCONNECTED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 16, 16, Icon_Link_bits);
    ssd1306_DrawLine(2, 12, 18, 28);
    ssd1306_DrawLine(3, 12, 19, 28);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(37, 30 - fh_grande);
    Oled_DrawStr("Web");

    ssd1306_SetCursor(20, 50 - fh_grande);
    Oled_DrawStr("desconect");
}

void OledUtils_RenderESPAPStartedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_AP_STARTED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    char ipStr[20];

    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 19, 16, Icon_Wifi_100_bits);

    Oled_SetFont(&Font_11x18);
    ssd1306_SetCursor(27, 30 - Oled_FontHeight());
    Oled_DrawStr("AP OK");

    Oled_SetFont(&Font_7x10);
    snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u",
    (unsigned)espApIp.block1,
    (unsigned)espApIp.block2,
    (unsigned)espApIp.block3,
    (unsigned)espApIp.block4);
    ssd1306_SetCursor(8, 60 - Oled_FontHeight());
    Oled_DrawStr(ipStr);
}

void OledUtils_RenderPingReceivedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_DIAG_PING_RECEIVED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 16, 16, Icon_Info_bits);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 30 - fh_grande);
    Oled_DrawStr("PING");

    ssd1306_SetCursor(2, 50 - fh_grande);
    Oled_DrawStr("recibido");
}

void OledUtils_RenderESPCheckingConnectionNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_CHECKING, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();

    ssd1306_SetCursor(2, 30 - fh_grande);
    Oled_DrawStr("Chequeando");

    ssd1306_SetCursor(10, 50 - fh_grande);
    Oled_DrawStr("conexion");
}

void OledUtils_RenderESPFirmwareRequestNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_REQUEST, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    // Texto "Solicitando" con fuente grande
    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();

    ssd1306_SetCursor(32, 24 - fh_grande);
    Oled_DrawStr("Pedir");

    // Texto "firmware"
    ssd1306_SetCursor(32, 44 - fh_grande);
    Oled_DrawStr("firmware");

    // Texto "Cancelar" con fuente pequeña
    Oled_SetFont(&Font_7x10);
    const uint8_t fh_pequena = Oled_FontHeight();
    ssd1306_SetCursor(50, 60 - fh_pequena);
    Oled_DrawStr("Cancelar");

    // Botón OK / Encoder (13x13 px)
    Oled_DrawXBM(112, 48, 13, 13, Icon_Encoder_bits);

    // Icono Info (esquina superior izquierda)
    Oled_DrawXBM(8, 16, 16, 16, Icon_Info_bits);
}

void OledUtils_RenderESPFirmwareScreen(void)
{
	__NOP();
    const char *fw_text = (espFirmwareVersion[0] != '\0') ? espFirmwareVersion : "Sin datos";

    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_RECEIVED, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(8, 5, 16, 16, Icon_Info_bits);

    Oled_SetFont(&Font_11x18);
    ssd1306_SetCursor(34, 22 - Oled_FontHeight());
    Oled_DrawStr("FW ESP01");

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(6, 43 - Oled_FontHeight());
    Oled_DrawStrMax(fw_text, 17u);

    ssd1306_SetCursor(33, 62 - Oled_FontHeight());
    Oled_DrawStr("Volver");
    Oled_DrawXBM(33, 44, 16, 16, Icon_Encoder_bits);
}

void OledUtils_RenderESPFirmwareReceivedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_RECEIVED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(8, 16, 16, 16, Icon_Info_bits);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(34, 24 - fh_grande);
    Oled_DrawStr("FW ESP");

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(6, 48 - Oled_FontHeight());
    Oled_DrawStrMax(espFirmwareVersion, 17u);
}

void OledUtils_RenderPermissionDeniedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_WARNING_PERMISSION_DENIED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(57, 3, LOCK_ICON_W, LOCK_ICON_H, Icon_Lock_bits);

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(13, 34 - Oled_FontHeight());
    Oled_DrawStr("PIN no verificado");
    ssd1306_SetCursor(4, 52 - Oled_FontHeight());
    Oled_DrawStr("Accion restringida");
}

void OledUtils_RenderESPResetSentNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_RESET_SENT, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    // Texto "Reset" con fuente grande
    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();

    ssd1306_SetCursor(32, 24 - fh_grande);
    Oled_DrawStr("Enviar");

    // Texto "enviado"
    ssd1306_SetCursor(32, 44 - fh_grande);
    Oled_DrawStr("reset");

    // Texto "Cancelar" con fuente pequeña
    Oled_SetFont(&Font_7x10);
    const uint8_t fh_pequena = Oled_FontHeight();
    ssd1306_SetCursor(50, 60 - fh_pequena);
    Oled_DrawStr("Cancelar");

    // Botón OK / Encoder (13x13 px)
    Oled_DrawXBM(112, 48, 13, 13, Icon_Encoder_bits);

    // Icono Refrescar (esquina superior izquierda)
    Oled_DrawXBM(8, 16, 16, 16, Icon_Refrescar_bits);
}


void OledUtils_RenderESPCheckConnectionRequiredNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_CHECK_REQUIRED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh_grande = Oled_FontHeight();

    ssd1306_SetCursor(16, 30 - fh_grande);
    Oled_DrawStr("Chequee");

    ssd1306_SetCursor(10, 50 - fh_grande);
    Oled_DrawStr("conexion");
}

void OledUtils_ShowWifiResults()
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_SetColor(White);

    Oled_SetFont(&Font_11x18);
    const uint8_t fh = Oled_FontHeight();
    ssd1306_SetCursor(14, 20 - fh);
    Oled_DrawStr("Resultados");
    ssd1306_SetCursor(36, 38 - fh);
    Oled_DrawStr("WiFi");

    Oled_SetFont(&Font_7x10);
    const uint8_t fh_small = Oled_FontHeight();
    ssd1306_SetCursor(4, 60 - fh_small);
    Oled_DrawStr("OK: volver");

    Oled_DrawXBM(112, 48, 13, 13, Icon_Encoder_bits);
}
