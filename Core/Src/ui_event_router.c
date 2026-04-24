/*
 * ui_event_router.c
 *
 *  Created on: Sep 22, 2025
 *      Author: kobac
 */

#include "ui_event_router.h"
#include "globals.h"
#include "menusystem.h"
#include "screenWrappers.h"
#include "uner_app.h"

void UiEventRouter_HandleEvent(UserEvent_t ev)
{
    if (ev == UE_NONE) {
        return;
    }

    if (ev == UE_LONG_PRESS) {
    	//No deberia estar esto aca, hacer chequeo de si esta buscando antes
        const ScreenCode_t screen_code = MenuSys_GetCurrentScreenCode(&menuSystem);
    	if(IS_FLAG_SET(systemFlags3, WIFI_SEARCHING)){
            (void)UNER_App_SendCommand(UNER_CMD_ID_STOP_SCAN, NULL, 0u);
			CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
			wifiScanSessionActive = 0u;
			wifiScanResultsPending = 0u;
			wifiSearchingTimeout = WIFIDEFAULTSEARCHTIMEOUT;
    	}
        if (screen_code == SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING ||
            screen_code == SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS ||
            screen_code == SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS) {
            WiFiScan_ClearResults();
        }
        MenuSys_ResetMenu(&menuSystem);
        if (menuSystem.dashboardRender) {
        	//Aca deberia mandar el comando de reporte de modo
            menuSystem.renderFn = menuSystem.dashboardRender;
        }
        if (menuSystem.insideMenuFlag) {
            *menuSystem.insideMenuFlag = false;
        }
        if (menuSystem.clearScreen) {
            menuSystem.clearScreen();
        }
        menuSystem.renderFlag = true;
        oled_first_draw = true;
        return;
    }

    if (menuSystem.userEventManagerFn) {
        menuSystem.userEventManagerFn(ev);
    }
}
