#include "menu_definitions.h"
#include "screenWrappers.h"
#include "eventManagers.h"
#include "oled_utils.h"
#include "fonts.h"
#include "globals.h"

/* ---------- submenu ESP ---------- */

MenuItem submenuESPItems[] = {
    {"Chk Conexion", NULL, NULL, Icon_Link_bits, OledUtils_RenderESPCheckConnection_Wrapper, menuEventManager},
    {"Firmware",    NULL, NULL, Icon_Info_bits,  OledUtils_RenderESPFirmwareRequest_Wrapper, menuEventManager},
    {"Reset ESP",   NULL, NULL, Icon_Refrescar_bits, OledUtils_RenderESPResetSent_Wrapper, menuEventManager},
    {"Volver", MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager}
};

SubMenu submenuESP = {
    .name = "Enlace ESP",
    .items = submenuESPItems,
    .itemCount = sizeof(submenuESPItems)/sizeof(submenuESPItems[0]),
    .currentItemIndex = 0,
    .firstVisibleItem = 0,
    .parent = &submenu1,
    .icon = NULL
};

/* ---------- Menu System ---------- */

MenuSystem menuSystem = {
    .currentMenu = &mainMenu,
    .clearScreen = OledUtils_Clear,
    .drawItem = OledUtils_DrawItem_Wrapper,
    .renderFn = OledUtils_RenderStartupNotification_Wrapper,
    .insideMenuFlag = &inside_menu_flag,
    .renderFlag = false
};

/* ---------- Main menu ---------- */

MenuItem mainMenuItems[] = {
    { "Wifi", NULL, &submenu1, Icon_Wifi_bits, MenuSys_RenderMenu_Wrapper, menuEventManager },
    { "Sensores", NULL, &submenu2, Icon_Sensors_bits, MenuSys_RenderMenu_Wrapper, menuEventManager, MenuItem_IsTestModeVisible },
    { "Config.", NULL, &submenu3, Icon_Config_bits, MenuSys_RenderMenu_Wrapper, menuEventManager },
    { "Volver", MenuSys_GoBack_Wrapper, NULL, Icon_Volver_bits, OledUtils_RenderDashboard_Wrapper, dashboardEventManager },
};

SubMenu mainMenu = {
    .name = "Principal",
    .items = mainMenuItems,
    .itemCount = sizeof(mainMenuItems)/sizeof(mainMenuItems[0]),
    .currentItemIndex = 0,
    .firstVisibleItem = 0,
    .parent = NULL,
    .icon = NULL
};

/* ---------- Submenu WIFI ---------- */

MenuItem submenu1Items[] = {
    {"Info AP", NULL, NULL, Icon_Info_bits, OledUtils_RenderWiFiConnectionStatus_Wrapper, menuEventManager},
    {"Buscar APs", NULL, NULL, Icon_Refrescar_bits, OledUtils_RenderWiFiSearching_Wrapper, WiFiSearch_UserEventManager},
    {"Conexion ESP", NULL, &submenuESP, Icon_Link_bits, MenuSys_RenderMenu_Wrapper, menuEventManager},
    {"Volver", MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager}
};

SubMenu submenu1 = {
    .name = "Wifi",
    .items = submenu1Items,
    .itemCount = sizeof(submenu1Items)/sizeof(submenu1Items[0]),
    .currentItemIndex = 0,
    .firstVisibleItem = 0,
    .parent = &mainMenu,
    .icon = NULL
};

/* ---------- Submenu Sensores ---------- */

MenuItem submenu2Items[] = {
    {"Valores IR", NULL, NULL, Icon_Tool_bits, OledUtils_RenderValoresIR_Wrapper, ReadOnly_UserEventManager},
    {"Valores MPU", NULL, NULL, Icon_Tool_bits, OledUtils_RenderValoresMPU_Wrapper, ReadOnly_UserEventManager},
    {"Test motores", NULL, NULL, Icon_Tool_bits, OledUtils_RenderMotorTest_Wrapper, motorTestEventManager},
    {"Volver", MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager}
};

SubMenu submenu2 = {
    .name = "Sensores",
    .items = submenu2Items,
    .itemCount = sizeof(submenu2Items)/sizeof(submenu2Items[0]),
    .currentItemIndex = 0,
    .firstVisibleItem = 0,
    .parent = &mainMenu,
    .icon = NULL
};

/* ---------- Submenu Config ---------- */

MenuItem submenu3Items[] = {
    {"Preferencias", NULL, NULL, Icon_Prefs_bits, NULL, menuEventManager},
    {"Acerca de", NULL, NULL, Icon_Info_bits, OledUtils_About_Wrapper, About_UserEventManager},
    {"Volver", MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager}
};

SubMenu submenu3 = {
    .name = "Config.",
    .items = submenu3Items,
    .itemCount = sizeof(submenu3Items)/sizeof(submenu3Items[0]),
    .currentItemIndex = 0,
    .firstVisibleItem = 0,
    .parent = &mainMenu,
    .icon = NULL
};

bool MenuItem_IsTestModeVisible(void) {
    return GET_CAR_MODE() == TEST_MODE;
}
