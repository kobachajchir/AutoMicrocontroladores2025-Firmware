/*
 * menu_config.c
 *
 *  Created on: Sep 17, 2025
 *      Author: codex
 */

#include "menu_config.h"

#include "eventManagers.h"
#include "fonts.h"
#include "globals.h"
#include "oled_utils.h"
#include "screenWrappers.h"

static bool MenuItem_IsTestModeVisible(void)
{
    return GET_CAR_MODE() == TEST_MODE;
}

void OledUtils_Clear_Wrapper(void)
{
    OledUtils_Clear();
}

void OLED_Is_Ready(void)
{
    if (!IS_FLAG_SET(systemFlags, OLED_READY)) {
        menuSystem.renderFn = OledUtils_RenderStartupNotification_Wrapper;
        menuSystem.renderFlag = true;
        oled_first_draw = true;
        SET_FLAG(systemFlags, OLED_READY);
        __NOP();
    }
}

// Forward declarations for menu references used during initialization.
extern SubMenu mainMenu;
extern SubMenu submenu1;
extern SubMenu submenu2;
extern SubMenu submenu3;

// submenuESPItems
MenuItem submenuESPItems[] = {
    {"Chk Conexion", NULL, NULL, Icon_Link_bits, OledUtils_RenderESPCheckConnection_Wrapper, menuEventManager},
    {"Firmware",    NULL, NULL, Icon_Info_bits,  OledUtils_RenderESPFirmwareRequest_Wrapper, menuEventManager},
    {"Reset ESP",    NULL, NULL, Icon_Refrescar_bits,  OledUtils_RenderESPResetSent_Wrapper, menuEventManager},
    {"Volver",       MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager}
};

SubMenu submenuESP = {
    .name                = "Enlace ESP",
    .items               = submenuESPItems,
    .itemCount           = sizeof(submenuESPItems) / sizeof(submenuESPItems[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = &submenu1,
    .icon                = NULL
};

MenuSystem menuSystem = {
    .currentMenu      = &mainMenu,
    .clearScreen      = OledUtils_Clear_Wrapper,
    .drawItem         = OledUtils_DrawItem_Wrapper,
    .renderFn         = OledUtils_RenderStartupNotification_Wrapper,
    .insideMenuFlag   = &inside_menu_flag,
    .renderFlag       = false
};

// Ítems del menú principal
MenuItem mainMenuItems[] = {
    // nombre     acción          submenú        icono             pantalla render
    { "Wifi",      NULL,           &submenu1,     Icon_Wifi_bits,   MenuSys_RenderMenu_Wrapper, menuEventManager },
    { "Sensores",  NULL,           &submenu2,     Icon_Sensors_bits, MenuSys_RenderMenu_Wrapper, menuEventManager, MenuItem_IsTestModeVisible },
    { "Config.",   NULL,           &submenu3,     Icon_Config_bits, MenuSys_RenderMenu_Wrapper, menuEventManager },
    // name       action    submenu       icon               screenRenderFn
    { "Volver",   MenuSys_GoBack_Wrapper,     NULL,         Icon_Volver_bits,  OledUtils_RenderDashboard_Wrapper, dashboardEventManager },
};

// Menú principal
SubMenu mainMenu = {
    .name                = "Principal",
    .items               = mainMenuItems,
    .itemCount           = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = NULL,
    .icon                = NULL
};

// Ítems del submenu 1: MODO (sin pantalla asociada por ahora)
MenuItem submenu1Items[] = {
    {"Info AP",   NULL,       NULL, Icon_Info_bits, OledUtils_RenderWiFiConnectionStatus_Wrapper, menuEventManager},
    {"Buscar APs", NULL,     NULL, Icon_Refrescar_bits, OledUtils_RenderWiFiSearching_Wrapper, WiFiSearch_UserEventManager},
    {"Conexion ESP",   NULL,       &submenuESP, Icon_Link_bits, MenuSys_RenderMenu_Wrapper, menuEventManager},
    {"Volver", MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager}
};

SubMenu submenu1 = {
    .name                = "Wifi",
    .items               = submenu1Items,
    .itemCount           = sizeof(submenu1Items) / sizeof(submenu1Items[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = &mainMenu,
    .icon                = NULL
};

// Ítems del submenu 2 (“Pantallas”)
MenuItem submenu2Items[] = {
    {"Valores IR",     NULL,               NULL, Icon_Tool_bits, OledUtils_RenderValoresIR_Wrapper, ReadOnly_UserEventManager},
    {"Valores MPU",     NULL,               NULL, Icon_Tool_bits, OledUtils_RenderValoresMPU_Wrapper, ReadOnly_UserEventManager},
    {"Test motores",     NULL,               NULL, Icon_Tool_bits, OledUtils_RenderMotorTest_Wrapper, motorTestEventManager},
    {"Volver",         MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager}
};

SubMenu submenu2 = {
    .name                = "Sensores",
    .items               = submenu2Items,
    .itemCount           = sizeof(submenu2Items) / sizeof(submenu2Items[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = &mainMenu,
    .icon                = NULL
};

// submenu3Items
MenuItem submenu3Items[] = {
    {"Preferencias", NULL, NULL, Icon_Prefs_bits, NULL, menuEventManager},
    {"Acerca de",    NULL, NULL, Icon_Info_bits,  OledUtils_About_Wrapper, About_UserEventManager},
    {"Volver",       MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper, menuEventManager}
};

SubMenu submenu3 = {
    .name                = "Config.",
    .items               = submenu3Items,
    .itemCount           = sizeof(submenu3Items) / sizeof(submenu3Items[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = &mainMenu,
    .icon                = NULL
};
