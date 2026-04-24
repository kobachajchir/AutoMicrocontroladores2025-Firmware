/*
 * screenWrappers.c
 *
 *  Created on: Jul 23, 2025
 *      Author: kobac
 */

#include "screenWrappers.h"
#include "uner_app.h"
#include "menusystem.h"
#include "oled_utils.h"
#include "fonts.h"
#include "encoder.h"
#include "eventManagers.h"
#include "globals.h"
#include <string.h>

static bool dashboardModeReportPending = false;

static void WiFiResults_ResetMenuState(void)
{
    memset(wifiResultsItems, 0, sizeof(wifiResultsItems));
    wifiResultsMenu.parent = &submenu1;
    wifiResultsMenu.itemCount = 0u;
    wifiResultsMenu.currentItemIndex = 0;
    wifiResultsMenu.firstVisibleItem = 0;
    wifiResultsMenu.lastSelectedItemIndex = -1;
    wifiResultsMenu.lastVisibleItem = -1;
}

void WiFiScan_ClearResults(void)
{
    networksFound = 0u;
    wifiSelectedNetworkIndex = WIFI_SELECTED_NETWORK_INVALID;
    memset(wifiNetworkSsids, 0, sizeof(wifiNetworkSsids));
    memset(wifiNetworkEncryptions, 0, sizeof(wifiNetworkEncryptions));
    memset(wifiNetworkSignalStrengths, 0, sizeof(wifiNetworkSignalStrengths));
    memset(wifiNetworkChannels, 0, sizeof(wifiNetworkChannels));
    memset(wifiNetworkEncryptionTypes, 0, sizeof(wifiNetworkEncryptionTypes));
    memset(wifiNetworkDetailValid, 0, sizeof(wifiNetworkDetailValid));
    WiFiResults_ResetMenuState();
}

static void WiFiResults_SetDefaultSelection(SubMenu *menu)
{
    if (!menu || menu->itemCount == 0u) {
        return;
    }

    if (menu->currentItemIndex < 0 || menu->currentItemIndex >= menu->itemCount) {
        menu->currentItemIndex = 0;
    }
    if (menu->firstVisibleItem < 0 || menu->firstVisibleItem >= menu->itemCount) {
        menu->firstVisibleItem = 0;
    }
    if (menu->currentItemIndex < menu->firstVisibleItem) {
        menu->firstVisibleItem = menu->currentItemIndex;
    }
    if ((menu->currentItemIndex - menu->firstVisibleItem) >= MENU_VISIBLE_ITEMS) {
        menu->firstVisibleItem = (int8_t)(menu->currentItemIndex - (MENU_VISIBLE_ITEMS - 1));
    }
    if (menu->lastSelectedItemIndex >= menu->itemCount) {
        menu->lastSelectedItemIndex = -1;
    }
    if (menu->lastVisibleItem >= menu->itemCount) {
        menu->lastVisibleItem = -1;
    }
}

static void WiFiResults_BuildMenu(void)
{
    uint8_t itemIndex = 0u;

    memset(wifiResultsItems, 0, sizeof(wifiResultsItems));
    wifiResultsMenu.parent = &submenu1;

    if (networksFound == 0u) {
        wifiResultsItems[itemIndex].name = "Sin redes";
        wifiResultsItems[itemIndex].icon = Icon_Wifi_bits;
        wifiResultsItems[itemIndex].screen_code = SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS;
        itemIndex++;
    } else {
        for (uint8_t i = 0u; i < networksFound && i < WIFI_SCAN_MAX_NETWORKS; ++i) {
            wifiResultsItems[itemIndex].name = wifiNetworkSsids[i];
            wifiResultsItems[itemIndex].icon = Icon_Wifi_bits;
            wifiResultsItems[itemIndex].screen_code = SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS;
            itemIndex++;
        }
    }

    wifiResultsItems[itemIndex].name = "Actualizar";
    wifiResultsItems[itemIndex].icon = Icon_Refrescar_bits;
    wifiResultsItems[itemIndex].screen_code = SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING;
    itemIndex++;

    wifiResultsItems[itemIndex].name = "Volver";
    wifiResultsItems[itemIndex].icon = Icon_Volver_bits;
    wifiResultsItems[itemIndex].screen_code = SCREEN_CODE_CONNECTIVITY_WIFI_MENU;
    itemIndex++;

    wifiResultsMenu.itemCount = itemIndex;
    WiFiResults_SetDefaultSelection(&wifiResultsMenu);
}

static uint8_t MenuSys_VisibleOffset(const SubMenu *menu, int8_t firstVisible, int8_t itemIndex) {
    uint8_t offset = 0;
    if (!menu) return 0;
    if (itemIndex < firstVisible) return 0;
    for (int8_t i = firstVisible; i < itemIndex; i++) {
        if (MenuSys_IsItemVisible(&menu->items[i])) {
            offset++;
        }
    }
    return offset;
}

void OledUtils_DrawItem_Wrapper(const MenuItem *item,
                                int               y,
                                bool              selected)
{
    // llamamos a la versión "completa" pasándole el handle
    OledUtils_DrawItem(item, y, selected);
}

/**
 * @brief  Wrapper para la pantalla principal (dashboard).
 */
void OledUtils_RenderDashboard_Wrapper(void)
{
    __NOP(); // BREAKPOINT: wrapper dashboard
    MenuSys_SetCurrentScreenCode(&menuSystem,
                                 SCREEN_CODE_CORE_DASHBOARD,
                                 SCREEN_REPORT_SOURCE_RENDER);
    // 1) Deshabilito el refresco periódico
    OledUtils_DisableContinuousRender();
    menuSystem.userEventManagerFn = dashboardEventManager;
    Dashboard_ResetModeConfirmState();
    __NOP();
    inside_menu_flag = false;

    // 2) Marco que esta es la primera pasada para full-refresh
    oled_first_draw = true;

    // 3) Bloqueo / desbloqueo encoder según el flag
    encoder.allowEncoderInput = IS_FLAG_SET(systemFlags2, OLED_ACTIVE);

    // 4) Llamo a la función que pinta TODO en el buffer
    OledUtils_Clear();
    OledUtils_RenderDashboard();
    dashboardModeReportPending = true;

    // ¡no vuelvo a tocar menuSystem.renderFlag!
}

/**
 * @brief  Wrapper que decide si redibuja todo o sólo mueve cursor.
 */
void MenuSys_RenderMenu_Wrapper(void) {
    __NOP(); // BREAKPOINT: wrapper menú
    OledUtils_DisableContinuousRender();
    menuSystem.userEventManagerFn = menuEventManager;
    inside_menu_flag = true;

    SubMenu *m = menuSystem.currentMenu;
    if (!m) return;

    // Configuración general
    encoder.allowEncoderInput = true;

    bool firstDraw  = oled_first_draw;
    bool pageChanged = (m->firstVisibleItem != m->lastVisibleItem);
    bool invalidSelection = (m->lastSelectedItemIndex < 0 || m->lastSelectedItemIndex >= m->itemCount ||
                             m->currentItemIndex < 0 || m->currentItemIndex >= m->itemCount ||
                             !MenuSys_IsItemVisible(&m->items[m->currentItemIndex]) ||
                             !MenuSys_IsItemVisible(&m->items[m->lastSelectedItemIndex]));

    if (firstDraw || pageChanged || invalidSelection) {
        // 👉 full render: limpia + dibuja TODOS los ítems visibles
        MenuSys_RenderMenu(&menuSystem);
        oled_first_draw = false;
    }
    else {
        // 👉 incremental: sólo borrar cursor antiguo y pintar el nuevo
        const uint8_t Y0 = MENU_ITEM_Y0;
        const uint8_t SP = MENU_ITEM_SPACING;

        uint8_t oldVisIdx = MenuSys_VisibleOffset(m, m->firstVisibleItem, m->lastSelectedItemIndex);
        uint8_t newVisIdx = MenuSys_VisibleOffset(m, m->firstVisibleItem, m->currentItemIndex);
        uint8_t oldY      = Y0 + oldVisIdx * SP;
        uint8_t newY      = Y0 + newVisIdx * SP;

        // Borrar cursor anterior
        OledUtils_DrawItem(&m->items[m->lastSelectedItemIndex], oldY, false);

        // Pintar cursor nuevo
        OledUtils_DrawItem(&m->items[m->currentItemIndex], newY, true);

        // Actualizo sólo la selección
        m->lastSelectedItemIndex = m->currentItemIndex;
    }
}

void MenuSys_GoBack_Wrapper(){
    MenuSys_NavigateBack(&menuSystem);
}

void OledUtils_RenderMotorTest_Wrapper(void) {
    MenuSys_SetCurrentScreenCode(&menuSystem,
                                 SCREEN_CODE_MOTOR_TEST,
                                 SCREEN_REPORT_SOURCE_RENDER);
    OledUtils_DisableContinuousRender();
    menuSystem.userEventManagerFn = motorTestEventManager;
    inside_menu_flag = false;
    encoder.allowEncoderInput = true;
    SET_FLAG(motorTask.motorData.flags, ENABLE_MOVEMENT);
    if(!oled_first_draw){
        OledUtils_Clear();
        OledUtils_MotorTest_Complete();
        oled_first_draw = true;
    }else{
        __NOP();
        OledUtils_MotorTest_Changes();
    }
}

void OledUtils_About_Wrapper(void)
{
    MenuSys_SetCurrentScreenCode(&menuSystem,
                                 IS_FLAG_SET(systemFlags2, SHOWSECONDSCREEN)
                                     ? SCREEN_CODE_SETTINGS_ABOUT_REPO
                                     : SCREEN_CODE_SETTINGS_ABOUT_PROJECT,
                                 SCREEN_REPORT_SOURCE_RENDER);
    // Estado base de esta pantalla
    OledUtils_DisableContinuousRender();
    inside_menu_flag = false;
    encoder.allowEncoderInput = true;

    // Seleccionar cuál pantalla real renderizar según flag
    if (IS_FLAG_SET(systemFlags2, SHOWSECONDSCREEN)) {
        OledUtils_Clear();
        OledUtils_RenderProyectInfoScreen();
        __NOP();
    } else {
        OledUtils_Clear();
        OledUtils_RenderProyectScreen();
        __NOP();
    }
    __NOP();
    // Activar el manager de eventos para esta pantalla
    menuSystem.userEventManagerFn = About_UserEventManager;
}

/**
 * @brief  Wrapper para la pantalla de Valores IR.
 */
void OledUtils_RenderValoresIR_Wrapper(void) {
    MenuSys_SetCurrentScreenCode(&menuSystem,
                                 SCREEN_CODE_SENSORS_IR_VALUES,
                                 SCREEN_REPORT_SOURCE_RENDER);
    OledUtils_EnableContinuousRender();
    inside_menu_flag = false;
    encoder.allowEncoderInput = true;  // Cambiar a true para poder salir
    menuSystem.userEventManagerFn = ReadOnly_UserEventManager;  // AGREGADO
    menuSystem.renderFlag = true;

    if (!oled_first_draw) {
        OledUtils_Clear();
        OledUtils_RenderIRGraphScene();
        OledUtils_UpdateIRBars(sensor_raw_data);
        oled_first_draw = true;
    } else {
        OledUtils_UpdateIRBars(sensor_raw_data);
    }
}

/**
 * @brief  Wrapper para la pantalla de Valores MPU.
 */
void OledUtils_RenderValoresMPU_Wrapper(void)
{
    MenuSys_SetCurrentScreenCode(&menuSystem,
                                 SCREEN_CODE_SENSORS_MPU_VALUES,
                                 SCREEN_REPORT_SOURCE_RENDER);
    OledUtils_EnableContinuousRender();
    inside_menu_flag = false;
    encoder.allowEncoderInput = true;  // Cambiar a true
    menuSystem.userEventManagerFn = ReadOnly_UserEventManager;  // AGREGADO
    menuSystem.renderFlag = true;

    if(!oled_first_draw){
        OledUtils_Clear();
        OledUtils_RenderMPUScene();
        OledUtils_UpdateMPUValues(&mpuTask);
        oled_first_draw = true;
    }else{
        OledUtils_UpdateMPUValues(&mpuTask);
    }
}
/**
 * @brief Wrapper para la pantalla de búsqueda WiFi
 */
void OledUtils_RenderWiFiSearching_Wrapper(void)
{
    MenuSys_SetCurrentScreenCode(&menuSystem,
                                 SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING,
                                 SCREEN_REPORT_SOURCE_RENDER);
    OledUtils_DisableContinuousRender();
    inside_menu_flag = false;
    encoder.allowEncoderInput = true;
    menuSystem.userEventManagerFn = WiFiSearch_UserEventManager;  // AGREGADO
    SET_FLAG(systemFlags3, WIFI_SEARCHING);

    if (!oled_first_draw) {
        wifiSearchingTimeout = WIFIDEFAULTSEARCHTIMEOUT;
        WiFiScan_ClearResults();
        wifiScanSessionActive = 1u;
        wifiScanResultsPending = 0u;
        __NOP();
        (void)UNER_App_SendCommand(UNER_CMD_ID_START_SCAN, NULL, 0u);
        OledUtils_Clear();
        OledUtils_RenderWiFiSearchScene();
        OledUtils_UpdateWiFiSearchTimer((uint8_t)(wifiSearchingTimeout / 100U));
        oled_first_draw = true;
    } else {
        OledUtils_UpdateWiFiSearchTimer((uint8_t)(wifiSearchingTimeout / 100U));
    }
}

void OledUtils_RenderWiFiConnectionStatus_Wrapper(){
	oled_first_draw = true;
	encoder.allowEncoderInput = true;
	menuSystem.userEventManagerFn = ReadOnly_UserEventManager;
	__NOP();
	(void)UNER_App_SendCommand(UNER_CMD_ID_GET_STATUS, NULL, 0u);
	menuSystem.renderFn = OledUtils_RenderWiFiStatus;
	OledUtils_RenderWiFiStatus();
	menuSystem.renderFlag = true;
}

void OledUtils_RenderESPFirmwareScreen_Wrapper(void)
{
    MenuSys_SetCurrentScreenCode(&menuSystem,
                                 SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_RECEIVED,
                                 SCREEN_REPORT_SOURCE_RENDER);
    OledUtils_DisableContinuousRender();
    inside_menu_flag = false;
    encoder.allowEncoderInput = true;
    menuSystem.userEventManagerFn = ReadOnly_UserEventManager;
    OledUtils_RenderESPFirmwareScreen();
    oled_first_draw = true;
}

void OledUtils_RenderWiFiSearchResults_Wrapper(void)
{
    MenuSys_SetCurrentScreenCode(&menuSystem,
                                 SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS,
                                 SCREEN_REPORT_SOURCE_MENU);
    OledUtils_DisableContinuousRender();
    inside_menu_flag = true;
    encoder.allowEncoderInput = true;
    menuSystem.userEventManagerFn = wifiEventManager;
    CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);

    WiFiResults_BuildMenu();
    menuSystem.currentMenu = &wifiResultsMenu;
    MenuSys_RenderMenu(&menuSystem);
    oled_first_draw = false;
}

void OledUtils_RenderWiFiDetails_Wrapper(void)
{
    MenuSys_SetCurrentScreenCode(&menuSystem,
                                 SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS,
                                 SCREEN_REPORT_SOURCE_RENDER);
    OledUtils_DisableContinuousRender();
    inside_menu_flag = false;
    encoder.allowEncoderInput = true;
    menuSystem.userEventManagerFn = WiFiDetails_UserEventManager;

    if (wifiSelectedNetworkIndex >= networksFound ||
        wifiSelectedNetworkIndex >= WIFI_SCAN_MAX_NETWORKS) {
        wifiSelectedNetworkIndex = WIFI_SELECTED_NETWORK_INVALID;
        menuSystem.renderFn = OledUtils_RenderWiFiSearchResults_Wrapper;
        menuSystem.renderFlag = true;
        oled_first_draw = false;
        return;
    }

    OledUtils_RenderWiFiDetails(wifiNetworkSsids[wifiSelectedNetworkIndex],
                                wifiNetworkEncryptions[wifiSelectedNetworkIndex],
                                wifiNetworkSignalStrengths[wifiSelectedNetworkIndex],
                                wifiNetworkChannels[wifiSelectedNetworkIndex],
                                wifiNetworkDetailValid[wifiSelectedNetworkIndex]);
    oled_first_draw = true;
}

void onRenderComplete(void) {
    __NOP();
    if(IS_FLAG_SET(systemFlags, OLED_READY)){
        menuSystem.allowPeriodicRefresh = false;
        __NOP();
    }

    if (dashboardModeReportPending) {
        dashboardModeReportPending = false;
        (void)UNER_App_ReportCarModeChanged((uint8_t)GET_CAR_MODE());
    }
}
