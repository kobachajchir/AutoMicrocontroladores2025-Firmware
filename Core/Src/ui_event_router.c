/*
 * ui_event_router.c
 *
 *  Created on: Sep 22, 2025
 *      Author: kobac
 */

#include "ui_event_router.h"
#include "globals.h"
#include "menusystem.h"

void UiEventRouter_HandleEvent(UserEvent_t ev)
{
    if (ev == UE_NONE) {
        return;
    }

    if (ev == UE_LONG_PRESS) {
    	//No deberia estar esto aca, hacer chequeo de si esta buscando antes
    	if(IS_FLAG_SET(systemFlags3, WIFI_SEARCHING)){
			CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
			wifiScanSessionActive = 0u;
			wifiScanResultsPending = 0u;
			wifiSearchingTimeout = WIFIDEFAULTSEARCHTIMEOUT;
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
