/*
 * eventManagers.c
 *
 *  Created on: Jul 23, 2025
 *      Author: kobac
 */

#include "eventManagers.h"
#include "screenWrappers.h"
#include "globals.h"

void dashboardEventManager(UserEvent_t ev) {
	__NOP();
    switch(ev) {
        case UE_ROTATE_CW:
            // Aca vamos a usar el overlay para cambiar el modo
            break;
        case UE_ROTATE_CCW:
        	// Aca vamos a usar el overlay para cambiar el modo
            break;
        case UE_LONG_PRESS:
            // entrar al menú
            menuSystem.clearScreen();
            menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
            *menuSystem.insideMenuFlag = 1;
            menuSystem.renderFlag = true;
            oled_first_draw = true;
            break;
        default:
            break;
    }
}

void menuEventManager(UserEvent_t ev) {
	__NOP();
    switch(ev) {
        case UE_ROTATE_CW:
            MenuSys_MoveCursorUp(&menuSystem);
            menuSystem.renderFlag = true;
            break;
        case UE_ROTATE_CCW:
            MenuSys_MoveCursorDown(&menuSystem);
            menuSystem.renderFlag = true;
            break;
        case UE_ENC_SHORT_PRESS:
            MenuSys_HandleClick(&menuSystem);
            break;
        case UE_ENC_LONG_PRESS:
            MenuSys_NavigateBack(&menuSystem);
            break;
        case UE_LONG_PRESS:
            // entrar al menú
            menuSystem.clearScreen();
            menuSystem.renderFn = OledUtils_RenderDashboard_Wrapper;
            *menuSystem.insideMenuFlag = 0;
            menuSystem.renderFlag = true;
            oled_first_draw = true;
            break;
        default:
            break;
    }
}

void motorTestEventManager(UserEvent_t ev) {
    /* Obtengo el motor seleccionado desde la parte alta de flags */
    uint8_t sel = NIBBLEH_GET_STATE(motorTask.motorData.flags);
    /* Puntero al config del motor activo (o NULL si ninguno) */
    Motor_Config_t *cfg = NULL;
    if (sel == MOTORLEFT_SELECTED) {
        cfg = &motorTask.motorData.motorLeft;
    } else if (sel == MOTORRIGHT_SELECTED) {
        cfg = &motorTask.motorData.motorRight;
    }

    switch(ev) {
        case UE_ROTATE_CW:
            if (cfg && cfg->motorSpeed <= 245) {
                cfg->motorSpeed += 10;
            }
            menuSystem.renderFlag = true;
            break;

        case UE_ROTATE_CCW:
            if (cfg && cfg->motorSpeed >= 10) {
                cfg->motorSpeed -= 10;
            }
            menuSystem.renderFlag = true;
            break;

        case UE_ENC_SHORT_PRESS:
            if (cfg) {
                /* Alternar dirección */
                cfg->motorDirection = (cfg->motorDirection == MOTOR_DIR_FORWARD)
                                      ? MOTOR_DIR_BACKWARD
                                      : MOTOR_DIR_FORWARD;
            }
            menuSystem.renderFlag = true;
            break;

        case UE_ENC_LONG_PRESS:
            /* Ciclar selección: Izquierdo → Derecho → Ninguno → Izquierdo */
            sel = (sel + 1) % 3;
            NIBBLEH_SET_STATE(motorTask.motorData.flags, sel);
            menuSystem.renderFlag = true;
            break;

        case UE_SHORT_PRESS:
            /* Activar/desactivar movimiento global */
            if (IS_FLAG_SET(motorTask.motorData.flags, ENABLE_MOVEMENT)) {
                CLEAR_FLAG(motorTask.motorData.flags, ENABLE_MOVEMENT);
            } else {
                SET_FLAG(motorTask.motorData.flags, ENABLE_MOVEMENT);
            }
            menuSystem.renderFlag = true;
            break;

        case UE_LONG_PRESS:
            /* Entrar al menú y desactivar movimiento */
            menuSystem.clearScreen();
            *menuSystem.insideMenuFlag = 1;
            CLEAR_FLAG(motorTask.motorData.flags, ENABLE_MOVEMENT);
            menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
            menuSystem.renderFlag = true;
            oled_first_draw = true;
            break;

        default:
            break;
    }
}
