/*
 * notificationWrappers.c
 *
 *  Created on: Apr 8, 2026
 */

#include "notificationWrappers.h"
#include "screenWrappers.h"
#include "uner_app.h"
#include "menusystem.h"
#include "oled_utils.h"
#include "encoder.h"
#include "eventManagers.h"
#include "globals.h"
#include "permissions.h"

static void OledUtils_ShowESPNotification(RenderFunction renderFn,
                                          OledNotificationHook onShow,
                                          OledNotificationHook onHide);
static void OledUtils_ShowESPNotificationReturningTo(RenderFunction renderFn,
                                                    RenderFunction restoreFn,
                                                    OledNotificationHook onShow,
                                                    OledNotificationHook onHide);

static void OledUtils_SendFirmwareRequest(void)
{
    (void)UNER_App_SendCommand(UNER_CMD_ID_REQUEST_FIRMWARE, NULL, 0u);
}

static void OledUtils_OnHide_SendESPReset(void)
{
    (void)UNER_App_SendCommand(UNER_CMD_ID_REBOOT_ESP, NULL, 0u);
}

static void OledUtils_ShowESPResetAfterPermission(void *ctx)
{
    (void)ctx;
    OledUtils_ShowESPNotification(
        OledUtils_RenderESPResetSentNotification,
        NULL,
        OledUtils_OnHide_SendESPReset);
}

static void OledUtils_ShowESPNotification(RenderFunction renderFn,
                                          OledNotificationHook onShow,
                                          OledNotificationHook onHide)
{
    OledUtils_ShowESPNotificationReturningTo(renderFn,
                                            MenuSys_RenderMenu_Wrapper,
                                            onShow,
                                            onHide);
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
    menuSystem.renderFlag = true; //Desactivar para activar la de abajo
    //OledUtils_ShowNotificationMs(OledUtils_RenderStartupNotification, 3500); //Activar para el final
}

void OledUtils_RenderESPCheckConnection_Wrapper(void)
{
    __NOP();
    (void)UNER_App_SendCommand(UNER_CMD_ID_GET_STATUS, NULL, 0u);
    OledUtils_ShowESPNotification(OledUtils_RenderESPCheckingConnectionNotification, NULL, NULL);
}

void OledUtils_RenderESPFirmwareRequest_Wrapper(void)
{
    RenderFunction notificationFn = OledUtils_RenderESPCheckConnectionRequiredNotification;
    OledNotificationHook onShowHook = NULL;

    if (IS_FLAG_SET(systemFlags2, ESP_PRESENT)) {
        notificationFn = OledUtils_RenderESPFirmwareRequestNotification;
        menuSystem.userEventManagerFn = ClickCancelar_UserEventManager;
        onShowHook = OledUtils_SendFirmwareRequest;
        OledUtils_ShowESPNotificationReturningTo(notificationFn,
                                                OledUtils_RenderESPFirmwareScreen_Wrapper,
                                                onShowHook,
                                                NULL);
        return;
    }

    OledUtils_ShowESPNotification(notificationFn, onShowHook, NULL);
}

void OledUtils_RenderESPResetSent_Wrapper(void)
{
    RenderFunction notificationFn = OledUtils_RenderESPCheckConnectionRequiredNotification;

    if (IS_FLAG_SET(systemFlags2, ESP_PRESENT)) {
        menuSystem.userEventManagerFn = ClickCancelar_UserEventManager;
        (void)Permission_Request(
            PERMISSION_ESP_RESET,
            OledUtils_ShowESPResetAfterPermission,
            NULL);
        return;
    }

    OledUtils_ShowESPNotification(notificationFn, NULL, NULL);
}
