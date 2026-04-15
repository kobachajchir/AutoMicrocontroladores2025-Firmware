/*
 * eventManagers.c
 *
 *  Created on: Jul 23, 2025
 *      Author: kobac
 */
#include "uner_app.h"
#include "eventManagers.h"
#include "screenWrappers.h"
#include "oled_utils.h"
#include "globals.h"
#include "permissions.h"

// ============================================================================
// CALLBACKS PARA DASHBOARD (CAMBIO DE MODO)
// ============================================================================

static bool dashboardModeCanConfirm = false;

static void Dashboard_ApplyPendingMode(void *ctx)
{
    (void)ctx;
    SET_CAR_MODE(auxCarMode);
    CLEAR_FLAG(systemFlags2, MODIFYING_CARMODE);
    dashboardModeCanConfirm = false;
    menuSystem.renderFn = OledUtils_RenderDashboard_Wrapper;
    menuSystem.clearScreen();
    menuSystem.renderFlag = true;
}

static void Dashboard_OnRotateCW(void)
{
    if (!IS_FLAG_SET(systemFlags2, MODIFYING_CARMODE)) {
        // Primera rotación
        auxCarMode = GET_CAR_MODE();
        menuSystem.clearScreen();
        SET_FLAG(systemFlags2, MODIFYING_CARMODE);
        auxCarMode = (auxCarMode + 1) % CAR_MODE_MAX;
        menuSystem.renderFn = OledUtils_RenderModeChange_Full;
    } else {
        // Rotaciones subsiguientes
        auxCarMode = NEXT_CAR_MODE();
        menuSystem.renderFn = OledUtils_RenderModeChange_ModeOnly;
    }
    dashboardModeCanConfirm = true;
    menuSystem.renderFlag = true;
}

static void Dashboard_OnRotateCCW(void)
{
    if (!IS_FLAG_SET(systemFlags2, MODIFYING_CARMODE)) {
        // Primera rotación
        auxCarMode = GET_CAR_MODE();
        menuSystem.clearScreen();
        SET_FLAG(systemFlags2, MODIFYING_CARMODE);
        auxCarMode = (auxCarMode + CAR_MODE_MAX - 1) % CAR_MODE_MAX;
        menuSystem.renderFn = OledUtils_RenderModeChange_Full;
    } else {
        // Rotaciones subsiguientes
        auxCarMode = PREV_CAR_MODE();
        menuSystem.renderFn = OledUtils_RenderModeChange_ModeOnly;
    }
    dashboardModeCanConfirm = true;
    menuSystem.renderFlag = true;
}

static void Dashboard_OnShortPress(void)
{
    if (dashboardModeCanConfirm && IS_FLAG_SET(systemFlags2, MODIFYING_CARMODE)) {
        if (auxCarMode == TEST_MODE && GET_CAR_MODE() != TEST_MODE) {
            (void)Permission_Request(
                PERMISSION_CAR_MODE_TEST,
                Dashboard_ApplyPendingMode,
                NULL);
            return;
        }

        Dashboard_ApplyPendingMode(NULL);
    }
    // Si no estamos modificando modo, no hacer nada
}

static void Dashboard_OnUserButton(void)
{
    dashboardModeCanConfirm = false;
    CLEAR_FLAG(systemFlags2, MODIFYING_CARMODE);
    MenuSys_NavigateToMain(&menuSystem);
    menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
    menuSystem.clearScreen();
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 1;
    }
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static void Dashboard_OnLongPress(void)
{
    dashboardModeCanConfirm = false;
    CLEAR_FLAG(systemFlags2, MODIFYING_CARMODE);
    if (menuSystem.dashboardRender) {
        menuSystem.renderFn = menuSystem.dashboardRender;
    }
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 0;
    }
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static const EventCallbacks_t dashboardCallbacks = {
    .onRotateCW     = Dashboard_OnRotateCW,
    .onRotateCCW    = Dashboard_OnRotateCCW,
    .onShortPress   = Dashboard_OnShortPress,
    .onLongPress    = Dashboard_OnLongPress,
    .onUserButton   = Dashboard_OnUserButton,
    .onEncLongPress = NULL
};

// ============================================================================
// CALLBACKS PARA MENÚ
// ============================================================================

static void Menu_OnRotateCW(void)
{
    MenuSys_MoveCursorUp(&menuSystem);
    menuSystem.renderFlag = true;
}

static void Menu_OnRotateCCW(void)
{
    MenuSys_MoveCursorDown(&menuSystem);
    menuSystem.renderFlag = true;
}

static void Menu_OnShortPress(void)
{
    MenuSys_HandleClick(&menuSystem);
}

static void Menu_OnEncLongPress(void)
{
    MenuSys_NavigateBack(&menuSystem);
}

static void Menu_OnUserButton(void)
{
    if (menuSystem.dashboardRender) {
        menuSystem.renderFn = menuSystem.dashboardRender;
    }
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 0;
    }
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static void Menu_OnLongPress(void)
{

}

static const EventCallbacks_t menuCallbacks = {
    .onRotateCW     = Menu_OnRotateCW,
    .onRotateCCW    = Menu_OnRotateCCW,
    .onShortPress   = Menu_OnShortPress,
    .onLongPress    = Menu_OnLongPress,
    .onUserButton   = Menu_OnUserButton,
    .onEncLongPress = Menu_OnEncLongPress
};

// ============================================================================
// CALLBACKS PARA ABOUT/PROYECTO
// ============================================================================

static void About_OnRotate(void)
{
    // Toggle de la flag
    if (IS_FLAG_SET(systemFlags2, SHOWSECONDSCREEN)) {
        CLEAR_FLAG(systemFlags2, SHOWSECONDSCREEN);
    } else {
        SET_FLAG(systemFlags2, SHOWSECONDSCREEN);
    }

    // Actualizar render con el wrapper
    menuSystem.clearScreen();
    menuSystem.renderFn = OledUtils_About_Wrapper;
    menuSystem.renderFlag = true;
}

static void About_OnShortPress(void)
{
    CLEAR_FLAG(systemFlags2, SHOWSECONDSCREEN);
    MenuSys_NavigateToMain(&menuSystem);
    menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
    menuSystem.clearScreen();
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 1;
    }
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static void About_OnLongPress(void)
{
    CLEAR_FLAG(systemFlags2, SHOWSECONDSCREEN);
    if (menuSystem.dashboardRender) {
        menuSystem.renderFn = menuSystem.dashboardRender;
    }
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 0;
    }
    menuSystem.clearScreen();
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static const EventCallbacks_t aboutCallbacks = {
    .onRotateCW     = About_OnRotate,
    .onRotateCCW    = About_OnRotate,
    .onShortPress   = About_OnShortPress,
    .onLongPress    = About_OnLongPress,
    .onUserButton   = NULL,
    .onEncLongPress = NULL
};

// ============================================================================
// CALLBACKS PARA MOTOR TEST
// ============================================================================

static void MotorTest_OnRotateCW(void)
{
    uint8_t sel = NIBBLEH_GET_STATE(motorTask.motorData.flags);
    Motor_Config_t *cfg = NULL;

    if (sel == MOTORLEFT_SELECTED) {
        cfg = &motorTask.motorData.motorLeft;
    } else if (sel == MOTORRIGHT_SELECTED) {
        cfg = &motorTask.motorData.motorRight;
    }

    if (cfg && cfg->motorSpeed <= 245) {
        cfg->motorSpeed += 10;
    }
    menuSystem.renderFlag = true;
}

static void MotorTest_OnRotateCCW(void)
{
    uint8_t sel = NIBBLEH_GET_STATE(motorTask.motorData.flags);
    Motor_Config_t *cfg = NULL;

    if (sel == MOTORLEFT_SELECTED) {
        cfg = &motorTask.motorData.motorLeft;
    } else if (sel == MOTORRIGHT_SELECTED) {
        cfg = &motorTask.motorData.motorRight;
    }

    if (cfg && cfg->motorSpeed >= 10) {
        cfg->motorSpeed -= 10;
    }
    menuSystem.renderFlag = true;
}

static void MotorTest_OnShortPress(void)
{
    uint8_t sel = NIBBLEH_GET_STATE(motorTask.motorData.flags);
    Motor_Config_t *cfg = NULL;

    if (sel == MOTORLEFT_SELECTED) {
        cfg = &motorTask.motorData.motorLeft;
    } else if (sel == MOTORRIGHT_SELECTED) {
        cfg = &motorTask.motorData.motorRight;
    }

    if (cfg) {
        cfg->motorDirection = (cfg->motorDirection == MOTOR_DIR_FORWARD)
                              ? MOTOR_DIR_BACKWARD
                              : MOTOR_DIR_FORWARD;
    }
    menuSystem.renderFlag = true;
}

static void MotorTest_OnEncLongPress(void)
{
    uint8_t sel = NIBBLEH_GET_STATE(motorTask.motorData.flags);
    sel = (sel + 1) % 3;
    NIBBLEH_SET_STATE(motorTask.motorData.flags, sel);
    menuSystem.renderFlag = true;
}

static void MotorTest_OnUserButton(void)
{
    if (IS_FLAG_SET(motorTask.motorData.flags, ENABLE_MOVEMENT)) {
        CLEAR_FLAG(motorTask.motorData.flags, ENABLE_MOVEMENT);
    } else {
        SET_FLAG(motorTask.motorData.flags, ENABLE_MOVEMENT);
    }
    menuSystem.renderFlag = true;
}

static void MotorTest_OnLongPress(void)
{
    if (menuSystem.dashboardRender) {
        menuSystem.renderFn = menuSystem.dashboardRender;
    }
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 0;
    }
    CLEAR_FLAG(motorTask.motorData.flags, ENABLE_MOVEMENT);
    menuSystem.clearScreen();
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static const EventCallbacks_t motorTestCallbacks = {
    .onRotateCW     = MotorTest_OnRotateCW,
    .onRotateCCW    = MotorTest_OnRotateCCW,
    .onShortPress   = MotorTest_OnShortPress,
    .onLongPress    = MotorTest_OnLongPress,
    .onUserButton   = MotorTest_OnUserButton,
    .onEncLongPress = MotorTest_OnEncLongPress
};

static void WiFiResults_BackToWifiMenu(void)
{
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 1;
    }

    MenuSys_NavigateBack(&menuSystem);
    menuSystem.clearScreen();
    menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static void WiFiResults_RestartScan(void)
{
    wifiSearchingTimeout = WIFIDEFAULTSEARCHTIMEOUT;
    networksFound = 0u;
    wifiScanSessionActive = 1u;
    wifiScanResultsPending = 0u;
    CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);

    menuSystem.renderFn = OledUtils_RenderWiFiSearching_Wrapper;
    menuSystem.clearScreen();
    menuSystem.renderFlag = true;
    oled_first_draw = false;
}

// ============================================================================
// CALLBACKS PARA BÚSQUEDA WIFI
// ============================================================================

/**
 * @brief Cancela la búsqueda WiFi y regresa al dashboard.
 */
static void WiFiSearch_Cancel(void)
{
    (void)UNER_App_SendCommand(UNER_CMD_ID_STOP_SCAN, NULL, 0u);

    CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
    wifiSearchingTimeout = WIFIDEFAULTSEARCHTIMEOUT;
    wifiScanSessionActive = 0u;
    wifiScanResultsPending = 0u;

    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 0;
    }

    menuSystem.renderFn = menuSystem.dashboardRender ? menuSystem.dashboardRender
                                                      : OledUtils_RenderDashboard_Wrapper;

    menuSystem.clearScreen();
    OledUtils_ShowNotificationMs(OledUtils_RenderWiFiSearchCanceledNotification, 2000u);

    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static void WiFiSearch_OnShortPress(void)
{
    WiFiSearch_Cancel();
}

static void WiFiSearch_OnLongPress(void)
{
    // Cancelar y volver al dashboard
    // TODO: Implementar stopWiFiScan() cuando esté disponible
    CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
    wifiScanSessionActive = 0u;
    wifiScanResultsPending = 0u;
    if (menuSystem.dashboardRender) {
        menuSystem.renderFn = menuSystem.dashboardRender;
    }
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 0;
    }
    menuSystem.clearScreen();
    wifiSearchingTimeout = WIFIDEFAULTSEARCHTIMEOUT;
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static const EventCallbacks_t wifiSearchCallbacks = {
    .onRotateCW     = NULL,  // No hacer nada al rotar durante búsqueda
    .onRotateCCW    = NULL,
    .onShortPress   = WiFiSearch_OnShortPress,
    .onLongPress    = WiFiSearch_OnLongPress,
    .onUserButton   = NULL,
    .onEncLongPress = NULL
};

// ============================================================================
// CALLBACKS PARA RESULTADOS WIFI
// ============================================================================

static void WiFiResults_OnRotateCW(void)
{
    MenuSys_MoveCursorUp(&menuSystem);
    menuSystem.renderFlag = true;
}

static void WiFiResults_OnRotateCCW(void)
{
    MenuSys_MoveCursorDown(&menuSystem);
    menuSystem.renderFlag = true;
}

static void WiFiResults_OnShortPress(void)
{
    SubMenu *menu = menuSystem.currentMenu;
    int8_t idx = (menu != NULL) ? menu->currentItemIndex : -1;

    if (!menu || idx < 0 || idx >= menu->itemCount || menu->itemCount < 2) {
        return;
    }

    if (idx == (menu->itemCount - 2)) {
        WiFiResults_RestartScan();
        return;
    }

    if (idx == (menu->itemCount - 1)) {
        WiFiResults_BackToWifiMenu();
    }
}

static void WiFiResults_OnEncLongPress(void)
{
    WiFiResults_BackToWifiMenu();
}

static void WiFiResults_OnUserButton(void)
{
    if (menuSystem.dashboardRender) {
        menuSystem.renderFn = menuSystem.dashboardRender;
    }
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 0;
    }
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static const EventCallbacks_t wifiResultsCallbacks = {
    .onRotateCW     = WiFiResults_OnRotateCW,
    .onRotateCCW    = WiFiResults_OnRotateCCW,
    .onShortPress   = WiFiResults_OnShortPress,
    .onLongPress    = NULL,
    .onUserButton   = WiFiResults_OnUserButton,
    .onEncLongPress = WiFiResults_OnEncLongPress
};

// ============================================================================
// CALLBACKS PARA PANTALLAS DE SOLO LECTURA (IR, MPU, Radar)
// ============================================================================

static void ReadOnly_OnShortPress(void)
{
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 1;
    }
    MenuSys_NavigateBack(&menuSystem);
    menuSystem.clearScreen();
    menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

void ReadOnly_OnLongPress(void)
{
    if (menuSystem.dashboardRender) {
        menuSystem.renderFn = menuSystem.dashboardRender;
    }
    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = 0;
    }
    menuSystem.clearScreen();
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

const EventCallbacks_t readOnlyCallbacks = {
    .onRotateCW     = NULL,  // No hacer nada al rotar (solo lectura)
    .onRotateCCW    = NULL,
    .onShortPress   = ReadOnly_OnShortPress,
    .onLongPress    = ReadOnly_OnLongPress,
    .onUserButton   = NULL,
    .onEncLongPress = NULL
};

void NotificationDismiss_OnShortPress(void)
{
    /* Cerrar notificación y restaurar lo anterior */
    OledUtils_NotificationRestore();
    oled_first_draw = true;
}

void NotificationDismiss_OnLongPress(void)
{
    /* Misma lógica: long press también descarta */
    OledUtils_NotificationRestore();
    oled_first_draw = true;
}

/* Callbacks para cuando hay una notificación activa y queremos permitir “dismiss” */
static const EventCallbacks_t notificationDismissCallbacks = {
    .onRotateCW     = NULL,
    .onRotateCCW    = NULL,
    .onShortPress   = NotificationDismiss_OnShortPress,
    .onLongPress    = NULL,
    .onUserButton   = NULL,
    .onEncLongPress = NULL
};

// ============================================================================
// MANAGER GENÉRICO DE EVENTOS
// ============================================================================

void GenericEventManager(UserEvent_t ev, const EventCallbacks_t *callbacks)
{
    if (!callbacks) return;

    switch (ev)
    {
        case UE_ROTATE_CW:
            if (callbacks->onRotateCW) {
                callbacks->onRotateCW();
            }
            break;

        case UE_ROTATE_CCW:
            if (callbacks->onRotateCCW) {
                callbacks->onRotateCCW();
            }
            break;

        case UE_ENC_SHORT_PRESS:
            if (callbacks->onShortPress) {
                callbacks->onShortPress();
            }
            break;

        case UE_ENC_LONG_PRESS:
            if (callbacks->onEncLongPress) {
                callbacks->onEncLongPress();
            }
            break;

        case UE_SHORT_PRESS:
            if (callbacks->onUserButton) {
                callbacks->onUserButton();
            }
            break;

        case UE_LONG_PRESS:
            if (callbacks->onLongPress) {
                callbacks->onLongPress();
            }
            break;

        default:
            break;
    }
}

void Dashboard_ResetModeConfirmState(void)
{
    dashboardModeCanConfirm = false;
    CLEAR_FLAG(systemFlags2, MODIFYING_CARMODE);
}

// ============================================================================
// FUNCIONES PÚBLICAS DE EVENT MANAGERS
// ============================================================================

void dashboardEventManager(UserEvent_t ev)
{
    __NOP();
    GenericEventManager(ev, &dashboardCallbacks);
}

void menuEventManager(UserEvent_t ev)
{
    __NOP();
    GenericEventManager(ev, &menuCallbacks);
}

void About_UserEventManager(UserEvent_t ev)
{
    GenericEventManager(ev, &aboutCallbacks);
}

void motorTestEventManager(UserEvent_t ev)
{
    GenericEventManager(ev, &motorTestCallbacks);
}

void WiFiSearch_UserEventManager(UserEvent_t ev)
{
    GenericEventManager(ev, &wifiSearchCallbacks);
}

void wifiEventManager(UserEvent_t ev)
{
    GenericEventManager(ev, &wifiResultsCallbacks);
}

void ReadOnly_UserEventManager(UserEvent_t ev)
{
    GenericEventManager(ev, &readOnlyCallbacks);
}

void ClickCancelar_UserEventManager(UserEvent_t ev)
{
    GenericEventManager(ev, &notificationDismissCallbacks);
}

// ============================================================================
// LEGACY: ItemEventManager (mantener por compatibilidad si se usa en otro lado)
// ============================================================================

void ItemEventManager(UserEvent_t ev, RenderWrapperFn wrapper)
{
    switch (ev)
    {
        case UE_ROTATE_CW:
        case UE_ROTATE_CCW:
            if (IS_FLAG_SET(systemFlags2, SHOWSECONDSCREEN)) {
                CLEAR_FLAG(systemFlags2, SHOWSECONDSCREEN);
            } else {
                SET_FLAG(systemFlags2, SHOWSECONDSCREEN);
            }

            if (wrapper) {
                menuSystem.clearScreen();
                menuSystem.renderFn = wrapper;
                menuSystem.renderFlag = true;
            } else {
                menuSystem.renderFlag = true;
                oled_first_draw = true;
            }
            break;

        case UE_ENC_SHORT_PRESS:
            CLEAR_FLAG(systemFlags2, SHOWSECONDSCREEN);
            MenuSys_NavigateToMain(&menuSystem);
            menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
            menuSystem.clearScreen();
            if (menuSystem.insideMenuFlag) {
                *menuSystem.insideMenuFlag = 1;
            }
            menuSystem.renderFlag = true;
            oled_first_draw = true;
            break;

        case UE_ENC_LONG_PRESS:
            break;

        case UE_LONG_PRESS:
            CLEAR_FLAG(systemFlags2, SHOWSECONDSCREEN);
            if (menuSystem.dashboardRender) {
                menuSystem.renderFn = menuSystem.dashboardRender;
            }
            if (menuSystem.insideMenuFlag) {
                *menuSystem.insideMenuFlag = 0;
            }
            menuSystem.clearScreen();
            menuSystem.renderFlag = true;
            oled_first_draw = true;
            break;

        default:
            break;
    }
}
