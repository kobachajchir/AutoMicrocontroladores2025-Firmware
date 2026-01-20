/*
 * screenWrappers.c
 *
 *  Created on: Jul 23, 2025
 *      Author: kobac
 */

#include "screenWrappers.h"
#include "menusystem.h"
#include "oled_utils.h"
#include "encoder.h"
#include "eventManagers.h"

void OledUtils_DrawItem_Wrapper(const MenuItem *item,
                                int               y,
                                bool              selected)
{
    // llamamos a la versión “completa” pasándole el handle
    OledUtils_DrawItem(&oledTask, item, y, selected);
}

/**
 * @brief  Wrapper para la pantalla principal (dashboard).
 */
void OledUtils_RenderDashboard_Wrapper(void)
{
    // 1) Deshabilito el refresco periódico
	OledUtils_DisableContinuousRender(&oledTask, onRenderComplete);
	menuSystem.userEventManagerFn = dashboardEventManager;
	__NOP();
	inside_menu_flag = false;

    // 2) Marco que esta es la primera pasada para full-refresh
    oledTask.first_Fn_Draw = true;

    // 3) Bloqueo / desbloqueo encoder según el flag
    encoder.allowEncoderInput = IS_FLAG_SET(systemFlags2, OLED_ACTIVE);

    // 4) Llamo a la función que pinta TODO en el buffer
    OledUtils_Clear(&oledTask, false);
    OledUtils_RenderDashboard(&oledTask);

    // 5) Arranco el flush: que el MainTask (a través de OLED_Update)
    //    vaya enviando página a página hasta que queue_count==0
    oledTask.auto_flush = true;

    // ¡no vuelvo a tocar menuSystem.renderFlag!
}

/**
 * @brief  Wrapper que decide si redibuja todo o sólo mueve cursor.
 */
void MenuSys_RenderMenu_Wrapper(void) {
    OledUtils_DisableContinuousRender(&oledTask, onRenderComplete);
    menuSystem.userEventManagerFn = menuEventManager;

    SubMenu *m = menuSystem.currentMenu;
    if (!m) return;

    // Configuración general
    oledTask.font             = &Font_6x12_Min;
    encoder.allowEncoderInput = true;
    oledTask.auto_flush       = true;

    bool firstDraw  = oledTask.first_Fn_Draw;
    bool pageChanged = (m->firstVisibleItem != m->lastVisibleItem);

    if (firstDraw || pageChanged) {
        // 👉 full render: limpia + dibuja TODOS los ítems visibles
        MenuSys_RenderMenu(&menuSystem);
        oledTask.first_Fn_Draw = false;
    }
    else {
        // 👉 incremental: sólo borrar cursor antiguo y pintar el nuevo
        const uint8_t Y0 = MENU_ITEM_Y0;
        const uint8_t SP = MENU_ITEM_SPACING;

        uint8_t oldVisIdx = m->lastSelectedItemIndex - m->firstVisibleItem;
        uint8_t newVisIdx = m->currentItemIndex      - m->firstVisibleItem;
        uint8_t oldY      = Y0 + oldVisIdx * SP;
        uint8_t newY      = Y0 + newVisIdx * SP;

        // Borrar cursor anterior
        OledUtils_DrawItem(&oledTask,
                           &m->items[m->lastSelectedItemIndex],
                           oldY,
                           false);

        // Pintar cursor nuevo
        OledUtils_DrawItem(&oledTask,
                           &m->items[m->currentItemIndex],
                           newY,
                           true);

        // Actualizo sólo la selección
        m->lastSelectedItemIndex = m->currentItemIndex;
    }
}

void MenuSys_GoBack_Wrapper(){
	MenuSys_NavigateBack(&menuSystem);
}

void OledUtils_RenderMotorTest_Wrapper(void) {
	OledUtils_DisableContinuousRender(&oledTask, onRenderComplete);
	menuSystem.userEventManagerFn = motorTestEventManager;
	inside_menu_flag = false;
	encoder.allowEncoderInput = true;
	SET_FLAG(motorTask.motorData.flags, ENABLE_MOVEMENT);
	if(!oledTask.first_Fn_Draw){
		OledUtils_Clear(&oledTask, false);
		OledUtils_MotorTest_Complete(&oledTask);
		oledTask.first_Fn_Draw = true;
	}else{
		__NOP();
		OledUtils_MotorTest_Changes(&oledTask);
	}
	oledTask.auto_flush = true;
}

void OledUtils_RenderRadar_Wrapper(void) {
	OledUtils_EnableContinuousRender(&oledTask);
	inside_menu_flag = false;
	encoder.allowEncoderInput = false;
	menuSystem.renderFlag = true;
	if(!oledTask.first_Fn_Draw){
		OledUtils_Clear(&oledTask, false);
		OledUtils_RenderRadarGraph(&oledTask, sensor_raw_data); // La primera vez renderizo todo
		oledTask.first_Fn_Draw = true;
	}else{
		__NOP();
		OledUtils_RenderRadarGraph_Objs(&oledTask, sensor_raw_data); //La siguiente solo los objetos
	}
	oledTask.auto_flush = true;
}

/**
 * @brief  Wrapper para la pantalla de Valores IR.
 */
void OledUtils_RenderValoresIR_Wrapper(void) {
    // cada vez que entrenamos aquí, nos aseguramos de tener auto_flush
    OledUtils_EnableContinuousRender(&oledTask);
    inside_menu_flag = false;
    menuSystem.renderFlag = true;
    oledTask.auto_flush = true;
    if (!oledTask.first_Fn_Draw) {
        // primera pasada → gráfico completo
    	OledUtils_Clear(&oledTask, false);
        OledUtils_DrawIRGraph(&oledTask, sensor_raw_data);
        // señalizamos que ya no estamos en la primera pasada
        oledTask.first_Fn_Draw = false;
    } else {
        // siguientes pasadas → sólo barras
        OledUtils_DrawIRBars(&oledTask, sensor_raw_data);
    }
}


/**
 * @brief  Wrapper para la pantalla de Valores MPU.
 */
void OledUtils_RenderValoresMPU_Wrapper(void)
{
	OledUtils_EnableContinuousRender(&oledTask);
    menuSystem.insideMenuFlag = false;
    menuSystem.renderFlag = true;
	encoder.allowEncoderInput = false;
	if(!oledTask.first_Fn_Draw){
		oledTask.first_Fn_Draw = true;
		OledUtils_Clear(&oledTask, false);
		OledUtils_RenderValoresMPUScreen(&oledTask, &mpuTask);
	}else{
		__NOP();
		OledUtils_RenderValoresMPUScreen(&oledTask, &mpuTask); //Despues cambiar esta por la que imprime solo los valores
	}
	oledTask.auto_flush = true;
}

void onRenderComplete(void) {
	__NOP();
	if(IS_FLAG_SET(systemFlags, OLED_READY)){
		menuSystem.allowPeriodicRefresh = false;
		 //oledTask.auto_flush = false;
		__NOP();
	}
}
