/*
 * notificationWrappers.c
 *
 *  Created on: Apr 8, 2026
 */

#include "notificationWrappers.h"
#include "menu_definitions.h"
#include "screenWrappers.h"
#include "uner_app.h"
#include "menusystem.h"
#include "oled_utils.h"
#include "fonts.h"
#include "encoder.h"
#include "eventManagers.h"
#include "globals.h"
#include "permissions.h"

static void OledUtils_ShowESPNotificationReturningTo(RenderFunction renderFn,
                                                    RenderFunction restoreFn,
                                                    OledNotificationHook onShow,
                                                    OledNotificationHook onHide);
static void OledUtils_ShowESPResetModeAfterPermission(void *ctx);
static void OledUtils_OpenESPResetModeMenu(void);
static void OledUtils_ReturnToESPMenu(void);
static void OledUtils_SendESPResetWithMode(uint8_t boot_mode);
static void OledUtils_SendESPResetNormal(void);
static void OledUtils_SendESPResetApMode(void);

static MenuItem espResetModeItems[] = {
    { "Normal", OledUtils_SendESPResetNormal, NULL, Icon_Refrescar_bits, NULL, NULL, NULL, SCREEN_CODE_CONNECTIVITY_ESP_REBOOT_MODE },
    { "Modo AP", OledUtils_SendESPResetApMode, NULL, Icon_Wifi_bits, NULL, NULL, NULL, SCREEN_CODE_CONNECTIVITY_ESP_REBOOT_MODE },
    { "Volver", MenuSys_GoBack_Wrapper, &submenuESP, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager, NULL, SCREEN_CODE_CONNECTIVITY_ESP_MENU }
};

static SubMenu espResetModeMenu = {
    .name = "Reinicio ESP",
    .items = espResetModeItems,
    .itemCount = sizeof(espResetModeItems) / sizeof(espResetModeItems[0]),
    .currentItemIndex = 0,
    .lastSelectedItemIndex = -1,
    .firstVisibleItem = 0,
    .lastVisibleItem = -1,
    .parent = &submenuESP,
    .icon = NULL,
    .screen_code = SCREEN_CODE_CONNECTIVITY_ESP_REBOOT_MODE
};

static void OledUtils_SendFirmwareRequest(void)
{
    (void)UNER_App_SendCommand(UNER_CMD_ID_REQUEST_FIRMWARE, NULL, 0u);
}

static void OledUtils_ReturnToESPMenu(void)
{
    menuSystem.currentMenu = &submenuESP;
    menuSystem.userEventManagerFn = menuEventManager;
    menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
    encoder.allowEncoderInput = true;
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 1;
    }
    inside_menu_flag = true;
    oled_first_draw = true;
}

static void OledUtils_SendESPResetWithMode(uint8_t boot_mode)
{
    if (UNER_App_SendEspReboot(boot_mode) != UNER_OK) {
        return;
    }

    OledUtils_ReturnToESPMenu();
    OledUtils_ShowNotificationMs(OledUtils_RenderESPResetSentNotification, 2000u);
}

static void OledUtils_SendESPResetNormal(void)
{
    OledUtils_SendESPResetWithMode(UNER_ESP_REBOOT_BOOT_MODE_NORMAL);
}

static void OledUtils_SendESPResetApMode(void)
{
    OledUtils_SendESPResetWithMode(UNER_ESP_REBOOT_BOOT_MODE_AP);
}

static void OledUtils_OpenESPResetModeMenu(void)
{
    espResetModeMenu.currentItemIndex = 0;
    espResetModeMenu.firstVisibleItem = 0;
    espResetModeMenu.lastSelectedItemIndex = -1;
    espResetModeMenu.lastVisibleItem = -1;
    espResetModeMenu.parent = &submenuESP;

    menuSystem.currentMenu = &espResetModeMenu;
    menuSystem.userEventManagerFn = menuEventManager;
    menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
    encoder.allowEncoderInput = true;
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 1;
    }
    inside_menu_flag = true;
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static void OledUtils_ShowESPResetModeAfterPermission(void *ctx)
{
    (void)ctx;
    OledUtils_OpenESPResetModeMenu();
}

static void OledUtils_ShowESPNotificationReturningTo(RenderFunction renderFn,
                                                    RenderFunction restoreFn,
                                                    OledNotificationHook onShow,
                                                    OledNotificationHook onHide)
{
    OledUtils_DisableContinuousRender();
    encoder.allowEncoderInput = false;
    menuSystem.userEventManagerFn = menuEventManager;

    menuSystem.renderFn = restoreFn ? restoreFn : MenuSys_RenderMenu_Wrapper;
    oled_first_draw = true;
    OledUtils_ShowNotificationMsEx(renderFn, 2000u, onShow, onHide);
}

void OledUtils_RenderStartupNotification_Wrapper(void)
{
    MenuSys_SetCurrentScreenCode(&menuSystem,
                                 SCREEN_CODE_CORE_STARTUP,
                                 SCREEN_REPORT_SOURCE_SYSTEM);
    OledUtils_DisableContinuousRender();
    menuSystem.userEventManagerFn = ReadOnly_UserEventManager;
    inside_menu_flag = false;
    encoder.allowEncoderInput = false;

    menuSystem.renderFn = OledUtils_RenderDashboard_Wrapper;
    menuSystem.renderFlag = true;
}

void OledUtils_RenderESPCheckConnection_Wrapper(void)
{
    __NOP();
    (void)UNER_App_SendCommand(UNER_CMD_ID_GET_STATUS, NULL, 0u);
    OledUtils_DisableContinuousRender();
    encoder.allowEncoderInput = false;
    menuSystem.userEventManagerFn = menuEventManager;
    menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
    oled_first_draw = true;
    OledUtils_ShowSimpleNotificationMs(OLED_SIMPLE_NOTIFICATION_ESP_CHECKING_CONNECTION, 2000u);
}

void OledUtils_RenderESPFirmwareRequest_Wrapper(void)
{
    OledNotificationHook onShowHook = NULL;

    if (IS_FLAG_SET(systemFlags2, ESP_PRESENT)) {
        menuSystem.userEventManagerFn = ClickCancelar_UserEventManager;
        onShowHook = OledUtils_SendFirmwareRequest;
        OledUtils_ShowESPNotificationReturningTo(OledUtils_RenderESPFirmwareRequestNotification,
                                                OledUtils_RenderESPFirmwareScreen_Wrapper,
                                                onShowHook,
                                                NULL);
        return;
    }

    OledUtils_DisableContinuousRender();
    encoder.allowEncoderInput = false;
    menuSystem.userEventManagerFn = menuEventManager;
    menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
    oled_first_draw = true;
    OledUtils_ShowSimpleNotificationMs(OLED_SIMPLE_NOTIFICATION_ESP_CHECK_CONNECTION_REQUIRED, 2000u);
}

void OledUtils_RenderESPResetMode_Wrapper(void)
{
    if (IS_FLAG_SET(systemFlags2, ESP_PRESENT)) {
        (void)Permission_Request(
            PERMISSION_ESP_RESET,
            OledUtils_ShowESPResetModeAfterPermission,
            NULL);
        return;
    }

    OledUtils_DisableContinuousRender();
    encoder.allowEncoderInput = false;
    menuSystem.userEventManagerFn = menuEventManager;
    menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
    oled_first_draw = true;
    OledUtils_ShowSimpleNotificationMs(OLED_SIMPLE_NOTIFICATION_ESP_CHECK_CONNECTION_REQUIRED, 2000u);
}
