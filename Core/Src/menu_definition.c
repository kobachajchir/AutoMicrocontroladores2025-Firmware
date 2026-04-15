#include "menu_definitions.h"
#include "screenWrappers.h"
#include "notificationWrappers.h"
#include "eventManagers.h"
#include "oled_utils.h"
#include "fonts.h"
#include "globals.h"

/* ---------- submenu ESP ---------- */

MenuItem submenuESPItems[] = {
    {"Chk Conexion", NULL, NULL, Icon_Link_bits, OledUtils_RenderESPCheckConnection_Wrapper, menuEventManager, NULL, SCREEN_CODE_CONNECTIVITY_ESP_CHECKING},
    {"Firmware",    NULL, NULL, Icon_Info_bits,  OledUtils_RenderESPFirmwareRequest_Wrapper, menuEventManager, NULL, SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_REQUEST},
    {"Reset ESP",   NULL, NULL, Icon_Refrescar_bits, OledUtils_RenderESPResetSent_Wrapper, menuEventManager, NULL, SCREEN_CODE_CONNECTIVITY_ESP_RESET_SENT},
    {"Volver", MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager, NULL, SCREEN_CODE_CORE_MAIN_MENU}
};

SubMenu submenuESP = {
    .name = "Enlace ESP",
    .items = submenuESPItems,
    .itemCount = sizeof(submenuESPItems)/sizeof(submenuESPItems[0]),
    .currentItemIndex = 0,
    .firstVisibleItem = 0,
    .parent = &submenu1,
    .icon = NULL,
    .screen_code = SCREEN_CODE_CONNECTIVITY_ESP_MENU
};

/* ---------- Menu System ---------- */

MenuSystem menuSystem = {
    .currentMenu = &mainMenu,
    .clearScreen = OledUtils_Clear,
    .drawItem = OledUtils_DrawItem_Wrapper,
    .renderFn = OledUtils_RenderStartupNotification_Wrapper,
    .insideMenuFlag = &inside_menu_flag,
    .renderFlag = false,
    .current_screen_code = SCREEN_CODE_CORE_STARTUP,
    .last_reported_screen_code = SCREEN_CODE_NONE,
    .current_screen_source = SCREEN_REPORT_SOURCE_SYSTEM,
    .screen_report_pending = false
};

/* ---------- Main menu ---------- */

MenuItem mainMenuItems[] = {
    { "Wifi", NULL, &submenu1, Icon_Wifi_bits, MenuSys_RenderMenu_Wrapper, menuEventManager, NULL, SCREEN_CODE_CONNECTIVITY_WIFI_MENU },
    { "Sensores", NULL, &submenu2, Icon_Sensors_bits, MenuSys_RenderMenu_Wrapper, menuEventManager, MenuItem_IsTestModeVisible, SCREEN_CODE_SENSORS_MENU },
    { "Config.", NULL, &submenu3, Icon_Config_bits, MenuSys_RenderMenu_Wrapper, menuEventManager, NULL, SCREEN_CODE_SETTINGS_MENU },
    { "Volver", MenuSys_GoBack_Wrapper, NULL, Icon_Volver_bits, OledUtils_RenderDashboard_Wrapper, dashboardEventManager, NULL, SCREEN_CODE_CORE_DASHBOARD },
};

SubMenu mainMenu = {
    .name = "Principal",
    .items = mainMenuItems,
    .itemCount = sizeof(mainMenuItems)/sizeof(mainMenuItems[0]),
    .currentItemIndex = 0,
    .firstVisibleItem = 0,
    .parent = NULL,
    .icon = NULL,
    .screen_code = SCREEN_CODE_CORE_MAIN_MENU
};

/* ---------- Submenu WIFI ---------- */

MenuItem submenu1Items[] = {
    {"Info AP", NULL, NULL, Icon_Info_bits, OledUtils_RenderWiFiConnectionStatus_Wrapper, menuEventManager, NULL, SCREEN_CODE_CONNECTIVITY_WIFI_STATUS},
    {"Buscar APs", NULL, NULL, Icon_Refrescar_bits, OledUtils_RenderWiFiSearching_Wrapper, WiFiSearch_UserEventManager, NULL, SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING},
    {"Conexion ESP", NULL, &submenuESP, Icon_Link_bits, MenuSys_RenderMenu_Wrapper, menuEventManager, NULL, SCREEN_CODE_CONNECTIVITY_ESP_MENU},
    {"Volver", MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager, NULL, SCREEN_CODE_CORE_MAIN_MENU}
};

SubMenu submenu1 = {
    .name = "Wifi",
    .items = submenu1Items,
    .itemCount = sizeof(submenu1Items)/sizeof(submenu1Items[0]),
    .currentItemIndex = 0,
    .firstVisibleItem = 0,
    .parent = &mainMenu,
    .icon = NULL,
    .screen_code = SCREEN_CODE_CONNECTIVITY_WIFI_MENU
};

/* ---------- Submenu Sensores ---------- */

MenuItem submenu2Items[] = {
    {"Valores IR", NULL, NULL, Icon_Tool_bits, OledUtils_RenderValoresIR_Wrapper, ReadOnly_UserEventManager, NULL, SCREEN_CODE_SENSORS_IR_VALUES},
    {"Valores MPU", NULL, NULL, Icon_Tool_bits, OledUtils_RenderValoresMPU_Wrapper, ReadOnly_UserEventManager, NULL, SCREEN_CODE_SENSORS_MPU_VALUES},
    {"Test motores", NULL, NULL, Icon_Tool_bits, OledUtils_RenderMotorTest_Wrapper, motorTestEventManager, NULL, SCREEN_CODE_MOTOR_TEST},
    {"Volver", MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager, NULL, SCREEN_CODE_CORE_MAIN_MENU}
};

SubMenu submenu2 = {
    .name = "Sensores",
    .items = submenu2Items,
    .itemCount = sizeof(submenu2Items)/sizeof(submenu2Items[0]),
    .currentItemIndex = 0,
    .firstVisibleItem = 0,
    .parent = &mainMenu,
    .icon = NULL,
    .screen_code = SCREEN_CODE_SENSORS_MENU
};

/* ---------- Submenu Config ---------- */

MenuItem submenu3Items[] = {
    {"Preferencias", NULL, NULL, Icon_Prefs_bits, NULL, menuEventManager, NULL, SCREEN_CODE_NONE},
    {"Acerca de", NULL, NULL, Icon_Info_bits, OledUtils_About_Wrapper, About_UserEventManager, NULL, SCREEN_CODE_SETTINGS_ABOUT_PROJECT},
    {"Volver", MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager, NULL, SCREEN_CODE_CORE_MAIN_MENU}
};

SubMenu submenu3 = {
    .name = "Config.",
    .items = submenu3Items,
    .itemCount = sizeof(submenu3Items)/sizeof(submenu3Items[0]),
    .currentItemIndex = 0,
    .firstVisibleItem = 0,
    .parent = &mainMenu,
    .icon = NULL,
    .screen_code = SCREEN_CODE_SETTINGS_MENU
};

bool MenuItem_IsTestModeVisible(void) {
    return GET_CAR_MODE() == TEST_MODE;
}
