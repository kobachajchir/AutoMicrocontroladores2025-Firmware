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
#include "uner_app.h"
#include <string.h>

static const FontDef *oled_font = &Font_7x10;
static char oled_wifi_connected_ssid[WIFI_SSID_MAX_LEN + 1u] = "WiFi";
static uint8_t oled_wifi_credentials_result_status = UNER_WIFI_WEB_CREDENTIALS_STATUS_FAILED;

typedef enum {
    OLED_SIMPLE_LAYOUT_LEFT_ICON_LARGE_2 = 0u,
    OLED_SIMPLE_LAYOUT_NO_ICON_LARGE_2,
    OLED_SIMPLE_LAYOUT_CENTER_ICON_LARGE_2,
    OLED_SIMPLE_LAYOUT_CENTER_ICON_SMALL_2,
} OledSimpleNotificationLayout;

enum {
    OLED_SIMPLE_ICON_FLAG_NONE = 0u,
    OLED_SIMPLE_ICON_FLAG_MSB = 1u << 0,
    OLED_SIMPLE_ICON_FLAG_INVERSE = 1u << 1,
};

typedef struct {
    ScreenCode_t screenCode;
    const uint8_t *icon;
    const char *line1;
    const char *line2;
    uint8_t iconW;
    uint8_t iconH;
    uint8_t layout;
    uint8_t iconFlags;
    uint8_t strikeCount;
} OledSimpleNotificationDef;

static void OledUtils_RenderSimpleNotification(void);
static uint8_t Oled_TextPixelWidth(const char *text);
static uint8_t Oled_CenterTextX(const char *text, uint8_t areaX, uint8_t areaW);
static void Oled_DrawCenteredLine(const char *text, uint8_t areaX, uint8_t areaW, uint8_t baselineY);

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

static const OledSimpleNotificationDef oled_simple_notifications[] = {
    [OLED_SIMPLE_NOTIFICATION_NONE] = { 0 },
    [OLED_SIMPLE_NOTIFICATION_WIFI_SEARCH_CANCELED] = {
        SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_CANCELED,
        Icon_Wifi_NotConnected_bits, "Busqueda", "cancelada",
        19u, 16u, OLED_SIMPLE_LAYOUT_LEFT_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_NONE, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_COMMAND_RECEIVED] = {
        SCREEN_CODE_DIAG_COMMAND_RECEIVED,
        Icon_Wifi_100_bits, "ECHO", "recibido",
        19u, 16u, OLED_SIMPLE_LAYOUT_LEFT_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_NONE, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_PING_RECEIVED] = {
        SCREEN_CODE_DIAG_PING_RECEIVED,
        Icon_Info_bits, "PING", "recibido",
        16u, 16u, OLED_SIMPLE_LAYOUT_LEFT_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_NONE, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_ESP_BOOT_RECEIVED] = {
        SCREEN_CODE_CONNECTIVITY_ESP_BOOT_RECEIVED,
        Icon_Info_bits, "ESP", "iniciada",
        19u, 16u, OLED_SIMPLE_LAYOUT_LEFT_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_MSB, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_ESP_MODE_CHANGED] = {
        SCREEN_CODE_CONNECTIVITY_ESP_MODE_CHANGED,
        Icon_Info_bits, "Modo", "actualizado",
        16u, 16u, OLED_SIMPLE_LAYOUT_LEFT_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_NONE, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_ESP_USB_CONNECTED] = {
        SCREEN_CODE_CONNECTIVITY_USB_CONNECTED,
        Icon_Info_bits, "USB", "conectado",
        16u, 16u, OLED_SIMPLE_LAYOUT_LEFT_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_NONE, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_ESP_WEB_SERVER_UP] = {
        SCREEN_CODE_CONNECTIVITY_WEB_SERVER_UP,
        Icon_Info_bits, "WEB", "lista",
        16u, 16u, OLED_SIMPLE_LAYOUT_LEFT_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_NONE, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_ESP_WEB_CLIENT_CONNECTED] = {
        SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_CONNECTED,
        Icon_Link_bits, "Web", "conectado",
        16u, 16u, OLED_SIMPLE_LAYOUT_LEFT_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_NONE, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_ESP_WEB_CLIENT_DISCONNECTED] = {
        SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_DISCONNECTED,
        Icon_Link_bits, "Web", "desconect",
        16u, 16u, OLED_SIMPLE_LAYOUT_LEFT_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_NONE, 2u
    },
    [OLED_SIMPLE_NOTIFICATION_ESP_CHECKING_CONNECTION] = {
        SCREEN_CODE_CONNECTIVITY_ESP_CHECKING,
        NULL, "Chequeando", "conexion",
        0u, 0u, OLED_SIMPLE_LAYOUT_NO_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_NONE, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_ESP_CHECK_CONNECTION_REQUIRED] = {
        SCREEN_CODE_CONNECTIVITY_ESP_CHECK_REQUIRED,
        NULL, "Chequee", "conexion",
        0u, 0u, OLED_SIMPLE_LAYOUT_NO_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_NONE, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_PERMISSION_DENIED] = {
        SCREEN_CODE_WARNING_PERMISSION_DENIED,
        Icon_Lock_bits, "PIN no verificado", "Accion restringida",
        LOCK_ICON_W, LOCK_ICON_H, OLED_SIMPLE_LAYOUT_CENTER_ICON_SMALL_2, OLED_SIMPLE_ICON_FLAG_NONE, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_CONTROLLER_CONNECTED] = {
        SCREEN_CODE_DIAG_CONTROLLER_CONNECTED,
        Icon_Controller_bits, "Control", "conectado",
        37u, 27u, OLED_SIMPLE_LAYOUT_CENTER_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_NONE, 0u
    },
    [OLED_SIMPLE_NOTIFICATION_CONTROLLER_DISCONNECTED] = {
        SCREEN_CODE_DIAG_CONTROLLER_DISCONNECTED,
        Icon_Controller_bits, "Control", "desconectado",
        37u, 27u, OLED_SIMPLE_LAYOUT_CENTER_ICON_LARGE_2, OLED_SIMPLE_ICON_FLAG_INVERSE, 3u
    },
};

static uint8_t Oled_IpIsValid(const volatile IPStruct_t *ip)
{
    return ip &&
           (ip->block1 != 0u ||
            ip->block2 != 0u ||
            ip->block3 != 0u ||
            ip->block4 != 0u);
}

static void Oled_Terminate(char *buf, size_t len, size_t pos)
{
    if (!buf || len == 0u) {
        return;
    }

    buf[(pos < len) ? pos : (len - 1u)] = '\0';
}

static size_t Oled_AppendChar(char *buf, size_t len, size_t pos, char ch)
{
    if (buf && pos + 1u < len) {
        buf[pos] = ch;
    }

    return pos + 1u;
}

static size_t Oled_AppendText(char *buf, size_t len, size_t pos, const char *text)
{
    if (!text) {
        return pos;
    }

    while (*text != '\0') {
        pos = Oled_AppendChar(buf, len, pos, *text);
        text++;
    }

    return pos;
}

static size_t Oled_AppendUnsigned(char *buf, size_t len, size_t pos, uint32_t value)
{
    char tmp[10];
    uint8_t count = 0u;

    do {
        tmp[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && count < (uint8_t)sizeof(tmp));

    while (count > 0u) {
        pos = Oled_AppendChar(buf, len, pos, tmp[--count]);
    }

    return pos;
}

static void Oled_CopyText(char *buf, size_t len, const char *text)
{
    size_t pos = Oled_AppendText(buf, len, 0u, text);
    Oled_Terminate(buf, len, pos);
}

static void Oled_FormatUnsigned(char *buf, size_t len, uint32_t value)
{
    size_t pos = Oled_AppendUnsigned(buf, len, 0u, value);
    Oled_Terminate(buf, len, pos);
}

static void Oled_FormatSigned(char *buf, size_t len, int32_t value)
{
    size_t pos = 0u;
    uint32_t magnitude;

    if (value < 0) {
        pos = Oled_AppendChar(buf, len, pos, '-');
        magnitude = (uint32_t)(-(value + 1)) + 1u;
    } else {
        magnitude = (uint32_t)value;
    }

    pos = Oled_AppendUnsigned(buf, len, pos, magnitude);
    Oled_Terminate(buf, len, pos);
}

static void Oled_FormatWiFiSignal(char *buf, size_t len, int8_t signal_strength, uint8_t detail_valid)
{
    size_t pos = 0u;

    if (!buf || len == 0u) {
        return;
    }

    if (!detail_valid) {
        Oled_CopyText(buf, len, "Pidiendo detalle");
        return;
    }

    pos = Oled_AppendText(buf, len, pos, "RSSI ");
    if (signal_strength < 0) {
        pos = Oled_AppendChar(buf, len, pos, '-');
        pos = Oled_AppendUnsigned(buf, len, pos, (uint32_t)(-(int32_t)signal_strength));
    } else {
        pos = Oled_AppendUnsigned(buf, len, pos, (uint32_t)signal_strength);
    }
    pos = Oled_AppendText(buf, len, pos, " dBm");
    Oled_Terminate(buf, len, pos);
}

static void Oled_FormatWiFiMeta(char *buf, size_t len, const char *encryption, uint8_t channel, uint8_t detail_valid)
{
    size_t pos = 0u;

    if (!buf || len == 0u) {
        return;
    }

    if (!detail_valid) {
        Oled_CopyText(buf, len, "Sin detalle");
        return;
    }

    if (encryption && encryption[0] != '\0') {
        pos = Oled_AppendText(buf, len, pos, encryption);
    } else {
        pos = Oled_AppendText(buf, len, pos, "ENC?");
    }

    if (channel != 0u) {
        pos = Oled_AppendText(buf, len, pos, "  ch ");
        pos = Oled_AppendUnsigned(buf, len, pos, channel);
    }

    Oled_Terminate(buf, len, pos);
}

static void Oled_FormatPercent3(char *buf, size_t len, uint8_t value)
{
    size_t pos = 0u;

    if (value < 100u) {
        pos = Oled_AppendChar(buf, len, pos, ' ');
    }
    if (value < 10u) {
        pos = Oled_AppendChar(buf, len, pos, ' ');
    }

    pos = Oled_AppendUnsigned(buf, len, pos, value);
    pos = Oled_AppendChar(buf, len, pos, '%');
    Oled_Terminate(buf, len, pos);
}

static void Oled_FormatIp(const volatile IPStruct_t *ip, char *buf, size_t len)
{
    size_t pos;

    if (!buf || len == 0u) {
        return;
    }

    if (!Oled_IpIsValid(ip)) {
        Oled_CopyText(buf, len, "Sin IP");
        return;
    }

    pos = Oled_AppendUnsigned(buf, len, 0u, ip->block1);
    pos = Oled_AppendChar(buf, len, pos, '.');
    pos = Oled_AppendUnsigned(buf, len, pos, ip->block2);
    pos = Oled_AppendChar(buf, len, pos, '.');
    pos = Oled_AppendUnsigned(buf, len, pos, ip->block3);
    pos = Oled_AppendChar(buf, len, pos, '.');
    pos = Oled_AppendUnsigned(buf, len, pos, ip->block4);
    Oled_Terminate(buf, len, pos);
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

// Activa refresco continuo: permitimos que el MainTask siga solicitando render
void OledUtils_EnableContinuousRender(void) {
    menuSystem.allowPeriodicRefresh = true;
}

// Desactiva refresco continuo: bloquea el periodic
void OledUtils_DisableContinuousRender(void) {
    menuSystem.allowPeriodicRefresh = false;
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

static const OledSimpleNotificationDef *OledUtils_GetSimpleNotificationDef(uint8_t simpleId)
{
    if (simpleId == 0u || simpleId >= (uint8_t)(sizeof(oled_simple_notifications) / sizeof(oled_simple_notifications[0]))) {
        return NULL;
    }

    return &oled_simple_notifications[simpleId];
}

static void OledUtils_DrawSimpleNotificationIcon(const OledSimpleNotificationDef *def, uint8_t x, uint8_t y)
{
    if (!def || !def->icon) {
        return;
    }

    if ((def->iconFlags & OLED_SIMPLE_ICON_FLAG_INVERSE) != 0u) {
        ssd1306_SetColor(Inverse);
    }

    if ((def->iconFlags & OLED_SIMPLE_ICON_FLAG_MSB) != 0u) {
        Oled_DrawXBM_MSB(x, y, def->iconW, def->iconH, def->icon);
    } else {
        Oled_DrawXBM(x, y, def->iconW, def->iconH, def->icon);
    }

    ssd1306_SetColor(White);
}

static void OledUtils_DrawSimpleNotificationStrike(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t strikeCount)
{
    for (uint8_t i = 0u; i < strikeCount; ++i) {
        ssd1306_DrawLine((int16_t)(x + i),
                         (int16_t)y,
                         (int16_t)(x + w + i),
                         (int16_t)(y + h));
    }
}

static void OledUtils_RenderSimpleNotification(void)
{
    const OledSimpleNotificationDef *def = OledUtils_GetSimpleNotificationDef(oledHandle.notification.simpleId);
    uint8_t iconX;
    uint8_t iconY;

    if (!def) {
        return;
    }

    OledUtils_SetScreenCode(def->screenCode, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    switch ((OledSimpleNotificationLayout)def->layout) {
        case OLED_SIMPLE_LAYOUT_LEFT_ICON_LARGE_2:
            OledUtils_DrawSimpleNotificationIcon(def, 2u, 12u);
            if (def->strikeCount != 0u) {
                OledUtils_DrawSimpleNotificationStrike(2u, 12u, def->iconW, def->iconH, def->strikeCount);
            }
            Oled_SetFont(&OLED_LARGE_FONT);
            Oled_DrawCenteredLine(def->line1, 24u, (uint8_t)(SSD1306_WIDTH - 24u), 30u);
            Oled_DrawCenteredLine(def->line2, 24u, (uint8_t)(SSD1306_WIDTH - 24u), 50u);
            break;

        case OLED_SIMPLE_LAYOUT_NO_ICON_LARGE_2:
            Oled_SetFont(&OLED_LARGE_FONT);
            Oled_DrawCenteredLine(def->line1, 0u, SSD1306_WIDTH, 30u);
            Oled_DrawCenteredLine(def->line2, 0u, SSD1306_WIDTH, 50u);
            break;

        case OLED_SIMPLE_LAYOUT_CENTER_ICON_SMALL_2:
            iconX = (uint8_t)((SSD1306_WIDTH - def->iconW) / 2u);
            iconY = 3u;
            OledUtils_DrawSimpleNotificationIcon(def, iconX, iconY);
            if (def->strikeCount != 0u) {
                OledUtils_DrawSimpleNotificationStrike(iconX, iconY, def->iconW, def->iconH, def->strikeCount);
            }
            Oled_SetFont(&Font_7x10);
            Oled_DrawCenteredLine(def->line1, 0u, SSD1306_WIDTH, 34u);
            Oled_DrawCenteredLine(def->line2, 0u, SSD1306_WIDTH, 52u);
            break;

        case OLED_SIMPLE_LAYOUT_CENTER_ICON_LARGE_2:
        default:
            iconX = (uint8_t)((SSD1306_WIDTH - def->iconW) / 2u);
            iconY = 4u;
            OledUtils_DrawSimpleNotificationIcon(def, iconX, iconY);
            if (def->strikeCount != 0u) {
                OledUtils_DrawSimpleNotificationStrike(iconX, iconY, def->iconW, def->iconH, def->strikeCount);
            }
            Oled_SetFont(&OLED_LARGE_FONT);
            Oled_DrawCenteredLine(def->line1, 0u, SSD1306_WIDTH, 48u);
            Oled_DrawCenteredLine(def->line2, 0u, SSD1306_WIDTH, 63u);
            break;
    }
}

static void OledUtils_RenderNotification_Wrapper(void)
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
        notif->simpleId = notif->suspended.simpleId;
        notif->timeoutTicks = notif->suspended.remainingTicks;
        notif->totalTicks = notif->suspended.remainingTicks;
        notif->needsFullRender = true;
        notif->onShow = notif->suspended.onShow;
        notif->onHide = notif->suspended.onHide;
        notif->suspended.valid = false;
        notif->suspended.simpleId = 0u;

        menuSystem.renderFn = OledUtils_RenderNotification_Wrapper;
        menuSystem.allowPeriodicRefresh = false;
        menuSystem.renderFlag = true;
        return;
    }

    notif->active = false;
    notif->renderFn = NULL;
    notif->simpleId = 0u;
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

static void OledUtils_ShowNotificationTicks10msInternal(RenderFunction renderFn,
                                                        uint8_t simpleId,
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
        notif->suspended.simpleId = notif->simpleId;
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
    notif->simpleId = simpleId;
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

void OledUtils_ShowNotificationMsEx(RenderFunction renderFn,
                                    uint16_t timeout_ms,
                                    OledNotificationHook onShow,
                                    OledNotificationHook onHide)
{
    uint16_t ticks = (uint16_t)((timeout_ms + 9U) / 10U);
    OledUtils_ShowNotificationTicks10msInternal(renderFn, 0u, ticks, onShow, onHide);
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

void OledUtils_ShowSimpleNotificationMs(OledSimpleNotificationId notificationId, uint16_t timeout_ms)
{
    uint16_t ticks;

    if (OledUtils_GetSimpleNotificationDef((uint8_t)notificationId) == NULL) {
        return;
    }

    ticks = (uint16_t)((timeout_ms + 9U) / 10U);
    OledUtils_ShowNotificationTicks10msInternal(OledUtils_RenderSimpleNotification,
                                                (uint8_t)notificationId,
                                                ticks,
                                                NULL,
                                                NULL);
}

bool OledUtils_IsSimpleNotificationShowing(OledSimpleNotificationId notificationId)
{
    return oledHandle.notification.active &&
           oledHandle.notification.simpleId == (uint8_t)notificationId;
}

static void OledUtils_ResetNotificationState(OledNotificationState *notif,
                                             RenderFunction *previousRenderFn,
                                             bool *previousAllowPeriodicRefresh,
                                             bool *previousRenderFlag)
{
    if (!notif || !previousRenderFn || !previousAllowPeriodicRefresh || !previousRenderFlag) {
        return;
    }

    if (!(notif->active || notif->suspended.valid)) {
        return;
    }

    *previousRenderFn = notif->previousRenderFn ? notif->previousRenderFn : *previousRenderFn;
    *previousAllowPeriodicRefresh = notif->previousAllowPeriodicRefresh;
    *previousRenderFlag = notif->previousRenderFlag;

    notif->active = false;
    notif->renderFn = NULL;
    notif->simpleId = 0u;
    notif->timeoutTicks = 0u;
    notif->totalTicks = 0u;
    notif->needsFullRender = false;
    notif->onShow = NULL;
    notif->onHide = NULL;
    notif->suspended.valid = false;
    notif->suspended.renderFn = NULL;
    notif->suspended.simpleId = 0u;
    notif->suspended.remainingTicks = 0u;
    notif->suspended.onShow = NULL;
    notif->suspended.onHide = NULL;

    menuSystem.renderFn = *previousRenderFn;
    menuSystem.allowPeriodicRefresh = *previousAllowPeriodicRefresh;
    menuSystem.renderFlag = *previousRenderFlag;
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

    OledUtils_ResetNotificationState(notif,
                                     &previousRenderFn,
                                     &previousAllowPeriodicRefresh,
                                     &previousRenderFlag);

    OledUtils_ShowNotificationMs(renderFn, timeout_ms);
}

void OledUtils_ReplaceSimpleNotificationMs(OledSimpleNotificationId notificationId, uint16_t timeout_ms)
{
    OledNotificationState *notif = &oledHandle.notification;
    RenderFunction previousRenderFn = menuSystem.renderFn;
    bool previousAllowPeriodicRefresh = menuSystem.allowPeriodicRefresh;
    bool previousRenderFlag = menuSystem.renderFlag;

    if (OledUtils_GetSimpleNotificationDef((uint8_t)notificationId) == NULL) {
        return;
    }

    OledUtils_ResetNotificationState(notif,
                                     &previousRenderFn,
                                     &previousAllowPeriodicRefresh,
                                     &previousRenderFlag);

    OledUtils_ShowSimpleNotificationMs(notificationId, timeout_ms);
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
        Oled_FormatPercent3(speedStr, sizeof(speedStr), speedPct);
    } else {
        Oled_CopyText(speedStr, sizeof(speedStr), "PARADO");
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
        Oled_FormatPercent3(speedStr, sizeof(speedStr), pct);
    } else {
        Oled_CopyText(speedStr, sizeof(speedStr), "PARADO");
    }

    // Línea 5: porcentaje o estado
    Oled_ClearBox(0, 57 - Oled_FontHeight(), 128, Oled_FontHeight());
    ssd1306_SetCursor(50, 57 - Oled_FontHeight());
    Oled_DrawStr(speedStr);
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

    Oled_SetFont(&OLED_LARGE_FONT);
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

/**
 * @brief Pantalla principal de información del proyecto
 */
void OledUtils_RenderProyectScreen(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_SETTINGS_ABOUT_PROJECT, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_SetColor(White);

    // Texto principal "Proyecto Auto" con fuente grande
    Oled_SetFont(&OLED_LARGE_FONT);
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

    Oled_SetFont(&OLED_LARGE_FONT);
    ssd1306_SetCursor(4, 62 - fh_grande);
    Oled_DrawStr("2026");
}

/**
 * @brief Pantalla secundaria con QR del repositorio
 */
void OledUtils_RenderProyectInfoScreen(void)
{
    BinaryAssetView_t qr_asset;
    uint8_t qr_ready;

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

    /*
     * El QR ya no vive en flash STM. Al renderizar esta pantalla se pide a ESP
     * por UNER_CMD_ID_BINARY_ASSET_STREAM y se dibuja cuando llega completo.
     */
    (void)UNER_App_RequestBinaryAsset(UNER_BINARY_ASSET_ID_GITHUB_QR);
    qr_ready = UNER_App_GetBinaryAsset(UNER_BINARY_ASSET_ID_GITHUB_QR, &qr_asset);

    // QR Code del repositorio (64x64 píxeles)
    ssd1306_SetColor(White);
    if (qr_ready && qr_asset.width <= 64u && qr_asset.height <= 64u) {
        const uint8_t qr_x = (uint8_t)(64u + ((64u - qr_asset.width) / 2u));
        const uint8_t qr_y = (uint8_t)((64u - qr_asset.height) / 2u);
        Oled_DrawXBM_MSB(qr_x, qr_y, qr_asset.width, qr_asset.height, qr_asset.data);
    } else {
        ssd1306_SetCursor(72, 20 - fh);
        Oled_DrawStr("QR");
        ssd1306_SetCursor(68, 34 - fh);
        Oled_DrawStr("remoto");
        ssd1306_SetCursor(68, 48 - fh);
        Oled_DrawStr("ESP");
    }
}

void OledUtils_RenderModeChange_Full(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CORE_MODE_CHANGE, SCREEN_REPORT_SOURCE_RENDER);
    // Título "Modo" con fuente grande
    Oled_SetFont(&OLED_LARGE_FONT);
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
    Oled_SetFont(&OLED_LARGE_FONT);
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
    Oled_SetFont(&OLED_LARGE_FONT);
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
    Oled_FormatSigned(buf, sizeof(buf), mpu->data.accel_x_mg);
    Oled_DrawStr(buf);

    // Limpiar y actualizar AY
    Oled_ClearBox(axValueX, 42 - fh, valueWidth, fh);
    ssd1306_SetCursor(axValueX, 42 - fh);
    Oled_FormatSigned(buf, sizeof(buf), mpu->data.accel_y_mg);
    Oled_DrawStr(buf);

    // Limpiar y actualizar AZ
    Oled_ClearBox(axValueX, 52 - fh, valueWidth, fh);
    ssd1306_SetCursor(axValueX, 52 - fh);
    Oled_FormatSigned(buf, sizeof(buf), mpu->data.accel_z_mg);
    Oled_DrawStr(buf);

    // Limpiar y actualizar GX
    Oled_ClearBox(gxValueX, 33 - fh, valueWidth, fh);
    ssd1306_SetCursor(gxValueX, 33 - fh);
    Oled_FormatSigned(buf, sizeof(buf), mpu->data.gyro_x_mdps);
    Oled_DrawStr(buf);

    // Limpiar y actualizar GY
    Oled_ClearBox(gxValueX, 43 - fh, valueWidth, fh);
    ssd1306_SetCursor(gxValueX, 43 - fh);
    Oled_FormatSigned(buf, sizeof(buf), mpu->data.gyro_y_mdps);
    Oled_DrawStr(buf);

    // Limpiar y actualizar GZ
    Oled_ClearBox(gxValueX, 53 - fh, valueWidth, fh);
    ssd1306_SetCursor(gxValueX, 53 - fh);
    Oled_FormatSigned(buf, sizeof(buf), mpu->data.gyro_z_mdps);
    Oled_DrawStr(buf);
}

static uint8_t Oled_TextPixelWidth(const char *text)
{
    size_t len;
    uint16_t width;

    if (!text) {
        return 0u;
    }

    len = strlen(text);
    width = (uint16_t)len * (uint16_t)Oled_FontWidth();
    return width > 255u ? 255u : (uint8_t)width;
}

static uint8_t Oled_CenterTextX(const char *text, uint8_t areaX, uint8_t areaW)
{
    uint8_t textWidth = Oled_TextPixelWidth(text);

    if (textWidth >= areaW) {
        return areaX;
    }

    return (uint8_t)(areaX + ((areaW - textWidth) / 2u));
}

static void Oled_DrawCenteredLine(const char *text, uint8_t areaX, uint8_t areaW, uint8_t baselineY)
{
    ssd1306_SetCursor(Oled_CenterTextX(text, areaX, areaW),
                      (uint8_t)(baselineY - Oled_FontHeight()));
    Oled_DrawStr(text);
}

/**
 * @brief Dibuja la escena completa de búsqueda WiFi con temporizador (una vez)
 */
void OledUtils_RenderWiFiSearchScene(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_SetColor(White);

    // Texto "Buscando" con fuente grande
    Oled_SetFont(&OLED_LARGE_FONT);
    const uint8_t fh_grande = Oled_FontHeight();
    ssd1306_SetCursor(27, 20 - fh_grande);
    Oled_DrawStr("Buscando");

    // Texto "redes wifi"
    ssd1306_SetCursor(2, 40 - fh_grande);
    Oled_DrawStr("redes wifi");

    // Texto "Cancelar" con fuente pequeña
    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(34, 60 - Oled_FontHeight());
    Oled_DrawStr("ESP");

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
    Oled_SetFont(&OLED_LARGE_FONT);
    const uint8_t fh = Oled_FontHeight();
    const uint8_t fw = Oled_FontWidth();

    // Limpiar zona del temporizador (ancho suficiente para "99")
    const uint8_t timerWidth = fw * 2;
    Oled_ClearBox(4, 62 - fh, timerWidth, fh);

    // Dibujar nuevo valor
    char timeStr[4];
    Oled_FormatUnsigned(timeStr, sizeof(timeStr), secondsRemaining);
    ssd1306_SetCursor(4, 62 - fh);
    Oled_DrawStr(timeStr);
}

void OledUtils_ShowWifiResults(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 19, 16, Icon_Wifi_100_bits);

    Oled_SetFont(&OLED_LARGE_FONT);
    ssd1306_SetCursor(14, 30 - Oled_FontHeight());
    Oled_DrawStr("Resultados");
    ssd1306_SetCursor(36, 50 - Oled_FontHeight());
    Oled_DrawStr("WiFi");
}

/**
 * @brief Pantalla de WiFi no conectado
 */
void OledUtils_RenderWiFiNotConnected(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_NOT_CONNECTED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_SetColor(White);

    // Texto "No hay red" con fuente grande
    Oled_SetFont(&OLED_LARGE_FONT);
    const uint8_t fh = Oled_FontHeight();
    ssd1306_SetCursor(7, 40 - fh);
    Oled_DrawStr("No hay red");

    // Icono WiFi desconectado (19x16 píxeles)
    Oled_DrawXBM(54, 3, 19, 16, Icon_Wifi_NotConnected_bits);

    // Texto "conectada"
    ssd1306_SetCursor(16, 55 - fh);
    Oled_DrawStr("conectada");
}

void OledUtils_RenderWiFiDetails(const char *ssid,
                                 const char *encryption,
                                 int8_t signal_strength,
                                 uint8_t channel,
                                 uint8_t detail_valid)
{
    const char *ssid_text = (ssid && ssid[0] != '\0') ? ssid : "[SSID RED]";
    char signal_text[18];
    char meta_text[18];

    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS, SCREEN_REPORT_SOURCE_RENDER);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(3, 2, 19, 16, Icon_Wifi_100_bits);

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(25, 13 - Oled_FontHeight());
    Oled_DrawStr("Detalles");

    Oled_FormatWiFiSignal(signal_text, sizeof(signal_text), signal_strength, detail_valid);
    Oled_FormatWiFiMeta(meta_text, sizeof(meta_text), encryption, channel, detail_valid);

    ssd1306_SetCursor(2, 27 - Oled_FontHeight());
    Oled_DrawStrMax(ssid_text, 17u);

    ssd1306_SetCursor(2, 39 - Oled_FontHeight());
    Oled_DrawStrMax(signal_text, 17u);

    ssd1306_SetCursor(2, 51 - Oled_FontHeight());
    Oled_DrawStrMax(meta_text, 17u);

    Oled_DrawXBM(2, 48, 15, 16, Icon_UserBtn_bits);
    Oled_DrawXBM(20, 53, 4, 7, Arrow_Left_bits);

    ssd1306_SetCursor(53, 59 - Oled_FontHeight());
    Oled_DrawStr("Conectar");
    Oled_DrawXBM(112, 49, 13, 13, Icon_Encoder_bits);
}

void OledUtils_RenderWiFiCredentialsWebNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_WEB,
                            SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(4, 4, 19, 16, Icon_Wifi_100_bits);

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(28, 18 - Oled_FontHeight());
    Oled_DrawStr("Ingresar");

    ssd1306_SetCursor(8, 36 - Oled_FontHeight());
    Oled_DrawStr("credenciales");

    ssd1306_SetCursor(42, 54 - Oled_FontHeight());
    Oled_DrawStr("en web");
}

void OledUtils_SetWiFiCredentialsWebResultStatus(uint8_t status)
{
    oled_wifi_credentials_result_status = status;
}

void OledUtils_RenderWiFiCredentialsSucceededNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_SUCCEEDED,
                            SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(54, 4, 19, 16, Icon_Wifi_100_bits);
    Oled_DrawXBM(57, 24, 14, 16, Icon_Checked_bits);

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(24, 49 - Oled_FontHeight());
    Oled_DrawStr("WiFi conectado");
}

void OledUtils_RenderWiFiCredentialsFailedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_FAILED,
                            SCREEN_REPORT_SOURCE_NOTIFICATION);
    const char *line1 = "Conexion";
    const char *line2 = "fallida";
    uint8_t line1_x = 38u;
    uint8_t line2_x = 42u;

    if (oled_wifi_credentials_result_status == UNER_WIFI_WEB_CREDENTIALS_STATUS_TIMEOUT) {
        line1 = "Tiempo";
        line2 = "agotado";
        line1_x = 43u;
        line2_x = 39u;
    } else if (oled_wifi_credentials_result_status == UNER_WIFI_WEB_CREDENTIALS_STATUS_CANCELED) {
        line1 = "Cancelada";
        line2 = "desde web";
        line1_x = 32u;
        line2_x = 32u;
    }

    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(54, 4, 19, 16, Icon_Wifi_NotConnected_bits);
    Oled_DrawXBM(58, 25, 11, 16, Icon_Crossed_bits);

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(line1_x, 48 - Oled_FontHeight());
    Oled_DrawStr(line1);
    ssd1306_SetCursor(line2_x, 61 - Oled_FontHeight());
    Oled_DrawStr(line2);
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

static void OledUtils_RenderWiFiConnecting(const char *ssid)
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

static void OledUtils_RenderWiFiConnected(const char *ssid, const char *ipAddress)
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
        Oled_SetFont(&OLED_LARGE_FONT);
        ssd1306_SetCursor(7, 40 - Oled_FontHeight());
        Oled_DrawStr("No hay red");
        ssd1306_SetCursor(16, 55 - Oled_FontHeight());
        Oled_DrawStr("conectada");
    }
    return;
#if 0
	    // Texto "No hay red" con fuente grande
	    Oled_SetFont(&OLED_LARGE_FONT);
	    const uint8_t fh = Oled_FontHeight();
	    ssd1306_SetCursor(19, 38 - fh);
	    Oled_DrawStr("Red");

	    // Icono WiFi desconectado (19x16 píxeles)
	    Oled_DrawXBM(54, 8, 19, 16, Icon_Wifi_100_bits);
	}else{
		ssd1306_SetColor(White);

		char ipStr[20];

		Oled_SetFont(&OLED_LARGE_FONT);
		const uint8_t fh = Oled_FontHeight();
		ssd1306_SetCursor(19, 20 - fh);
		Oled_DrawStr("Red");

		Oled_DrawXBM(54, 4, 19, 16, Icon_Wifi_100_bits);

		Oled_SetFont(&Font_7x10);
		Oled_FormatIp(&espStaIp, ipStr, sizeof(ipStr));
		ssd1306_SetCursor(8, 40 - Oled_FontHeight());
		Oled_DrawStr(ipStr);

		ssd1306_SetCursor(3, 58 - Oled_FontHeight());
		Oled_DrawStr(espFirmwareVersion);
	}

#endif
}

void OledUtils_RenderESPWifiConnectedNotification(void){
	char ipStr[20];

	Oled_FormatIp(&espWifiConnection.staIp, ipStr, sizeof(ipStr));
    OledUtils_RenderWiFiConnected(oled_wifi_connected_ssid, ipStr);
}

void OledUtils_RenderESPAPStartedNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_AP_STARTED, SCREEN_REPORT_SOURCE_NOTIFICATION);
    char ipStr[20];

    ssd1306_Clear();
    ssd1306_SetColor(White);

    Oled_DrawXBM(2, 12, 19, 16, Icon_Wifi_100_bits);

    Oled_SetFont(&OLED_LARGE_FONT);
    ssd1306_SetCursor(27, 30 - Oled_FontHeight());
    Oled_DrawStr("AP OK");

    Oled_SetFont(&Font_7x10);
    Oled_FormatIp(&espApIp, ipStr, sizeof(ipStr));
    ssd1306_SetCursor(8, 60 - Oled_FontHeight());
    Oled_DrawStr(ipStr);
}

void OledUtils_RenderESPFirmwareRequestNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_REQUEST, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    // Texto "Solicitando" con fuente grande
    Oled_SetFont(&OLED_LARGE_FONT);
    const uint8_t fh_grande = Oled_FontHeight();

    ssd1306_SetCursor(32, 24 - fh_grande);
    Oled_DrawStr("Pedir");

    // Texto "firmware"
    ssd1306_SetCursor(32, 44 - fh_grande);
    Oled_DrawStr("firmware");

    // Texto "Cancelar" con fuente pequeña
    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(34, 60 - Oled_FontHeight());
    Oled_DrawStr("ESP");

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

    Oled_SetFont(&OLED_LARGE_FONT);
    ssd1306_SetCursor(34, 22 - Oled_FontHeight());
    Oled_DrawStr("FW ESP01");

    Oled_SetFont(&Font_7x10);
    ssd1306_SetCursor(6, 43 - Oled_FontHeight());
    Oled_DrawStrMax(fw_text, 17u);

    ssd1306_SetCursor(33, 62 - Oled_FontHeight());
    Oled_DrawStr("Volver");
    Oled_DrawXBM(33, 44, 16, 16, Icon_Encoder_bits);
}

void OledUtils_RenderESPResetSentNotification(void)
{
    OledUtils_SetScreenCode(SCREEN_CODE_CONNECTIVITY_ESP_RESET_SENT, SCREEN_REPORT_SOURCE_NOTIFICATION);
    ssd1306_Clear();
    ssd1306_SetColor(White);

    // Texto "Reset" con fuente grande
    Oled_SetFont(&OLED_LARGE_FONT);
    const uint8_t fh_grande = Oled_FontHeight();

    ssd1306_SetCursor(23, 24 - fh_grande);
    Oled_DrawStr("Reset");

    // Texto "enviado"
    ssd1306_SetCursor(14, 44 - fh_grande);
    Oled_DrawStr("enviado");

    // Texto "Cancelar" con fuente pequeña
    Oled_SetFont(&Font_7x10);
    const uint8_t fh_pequena = Oled_FontHeight();
    ssd1306_SetCursor(34, 60 - fh_pequena);
    Oled_DrawStr("ESP");

    // Botón OK / Encoder (13x13 px)
    // Icono Refrescar (esquina superior izquierda)
    Oled_DrawXBM(8, 16, 16, 16, Icon_Refrescar_bits);
}
