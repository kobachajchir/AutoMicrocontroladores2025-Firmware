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
#include "encoder.h"
#include "eventManagers.h"

static volatile uint8_t esp_notification_canceled = 0u;

void ScreenWrappers_MarkEspNotificationCanceled(void)
{
    esp_notification_canceled = 1u;
}


static void OledUtils_OnHide_SendFirmwareRequest(void)
{
    if (esp_notification_canceled) {
        esp_notification_canceled = 0u;
        return;
    }
    (void)UNER_App_SendCommand(UNER_CMD_ID_REQUEST_FIRMWARE, NULL, 0u);
}

static void OledUtils_OnHide_SendESPReset(void)
{
    if (esp_notification_canceled) {
        esp_notification_canceled = 0u;
        return;
    }
    (void)UNER_App_SendCommand(UNER_CMD_ID_REBOOT_ESP, NULL, 0u);
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

    // ¡no vuelvo a tocar menuSystem.renderFlag!
}

void OledUtils_RenderStartupNotification_Wrapper(void)
{
    OledUtils_DisableContinuousRender();
    menuSystem.userEventManagerFn = ReadOnly_UserEventManager;
    inside_menu_flag = false;
    encoder.allowEncoderInput = false;

    menuSystem.renderFn = OledUtils_RenderDashboard_Wrapper;
    menuSystem.renderFlag = true; //Desactivar para activar la de abajo
    //OledUtils_ShowNotificationMs(OledUtils_RenderStartupNotification, 3500); //Activar para el final
}

void OledUtils_RenderTestScreen_Wrapper(void)
{
    __NOP(); // BREAKPOINT: wrapper pantalla de prueba
    OledUtils_DisableContinuousRender();
    menuSystem.userEventManagerFn = dashboardEventManager;
    __NOP();
    inside_menu_flag = false;

    oled_first_draw = true;

    encoder.allowEncoderInput = IS_FLAG_SET(systemFlags2, OLED_ACTIVE);

    OledUtils_Clear();
    OledUtils_RenderTestScreen();
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

void OledUtils_RenderRadar_Wrapper(void) {
    OledUtils_EnableContinuousRender();
    inside_menu_flag = false;
    encoder.allowEncoderInput = true;  // Cambiar a true
    menuSystem.userEventManagerFn = ReadOnly_UserEventManager;  // AGREGADO
    menuSystem.renderFlag = true;

    if(!oled_first_draw){
        OledUtils_Clear();
        OledUtils_RenderRadarGraph(sensor_raw_data);
        oled_first_draw = true;
    }else{
        OledUtils_RenderRadarGraph_Objs(sensor_raw_data);
    }
}

void OledUtils_About_Wrapper(void)
{
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
    OledUtils_DisableContinuousRender();
    inside_menu_flag = false;
    encoder.allowEncoderInput = true;
    menuSystem.userEventManagerFn = WiFiSearch_UserEventManager;  // AGREGADO
    SET_FLAG(systemFlags3, WIFI_SEARCHING);

    if (!oled_first_draw) {
        wifiSearchingTimeout = WIFIDEFAULTSEARCHTIMEOUT;
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
	if(!IS_FLAG_SET(systemFlags3, WIFI_ACTIVE)){
		menuSystem.renderFn = OledUtils_RenderWiFiNotConnected;
	}else{ //Aca deberia pedir datos de conexion al esp
		__NOP();
		(void)UNER_App_SendCommand(UNER_CMD_ID_GET_STATUS, NULL, 0u);
		menuSystem.renderFn = OledUtils_RenderWiFiStatus;
	}
	menuSystem.renderFlag = true;
}

void OledUtils_RenderWiFiSearchResults_Wrapper(void)
{
    OledUtils_DisableContinuousRender();
    inside_menu_flag = false;
    encoder.allowEncoderInput = true;
    menuSystem.userEventManagerFn = WiFiSearch_UserEventManager;  // AGREGADO
    CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
    if (!oled_first_draw) {
        OledUtils_Clear();
        OledUtils_ShowWifiResults();
        oled_first_draw = true;
    } else {
        OledUtils_ShowWifiResults();
    }
}

static void OledUtils_ShowESPNotification(RenderFunction renderFn,
                                          OledNotificationHook onShow,
                                          OledNotificationHook onHide)
{
    OledUtils_DisableContinuousRender();
    encoder.allowEncoderInput = false;
    menuSystem.userEventManagerFn = menuEventManager;

    if (!oled_first_draw) {
        menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
        oled_first_draw = true;
        OledUtils_ShowNotificationMsEx(renderFn, 2000u, onShow, onHide);
    }
}

void OledUtils_RenderESPCheckConnection_Wrapper(void)
{
    __NOP();
    (void)UNER_App_SendCommand(UNER_CMD_ID_PING, NULL, 0u);
    OledUtils_ShowESPNotification(OledUtils_RenderESPCheckingConnectionNotification, NULL, NULL);
}

void OledUtils_RenderESPFirmwareRequest_Wrapper(void)
{
    esp_notification_canceled = 0u;
    RenderFunction notificationFn = OledUtils_RenderESPCheckConnectionRequiredNotification;
    OledNotificationHook onHideHook = NULL;

    if (IS_FLAG_SET(systemFlags2, ESP_PRESENT)) {
        notificationFn = OledUtils_RenderESPFirmwareRequestNotification;
        menuSystem.userEventManagerFn = ClickCancelar_UserEventManager;
        onHideHook = OledUtils_OnHide_SendFirmwareRequest;
    }

    OledUtils_ShowESPNotification(notificationFn, NULL, onHideHook);
}

void OledUtils_RenderESPResetSent_Wrapper(void)
{
    esp_notification_canceled = 0u;
    RenderFunction notificationFn = OledUtils_RenderESPCheckConnectionRequiredNotification;
    OledNotificationHook onHideHook = NULL;

    if (IS_FLAG_SET(systemFlags2, ESP_PRESENT)) {
        notificationFn = OledUtils_RenderESPResetSentNotification;
        menuSystem.userEventManagerFn = ClickCancelar_UserEventManager;
        onHideHook = OledUtils_OnHide_SendESPReset;
    }

    OledUtils_ShowESPNotification(notificationFn, NULL, onHideHook);
}

void onRenderComplete(void) {
    __NOP();
    if(IS_FLAG_SET(systemFlags, OLED_READY)){
        menuSystem.allowPeriodicRefresh = false;
        __NOP();
    }
}
