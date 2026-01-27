/*
 * eventManagers.c
 *
 *  Created on: Jul 23, 2025
 *      Author: kobac
 */

#include "eventManagers.h"
#include "screenWrappers.h"
#include "oled_utils.h"
#include "globals.h"

void dashboardEventManager(UserEvent_t ev) {
	__NOP();
    switch(ev) {
    case UE_ROTATE_CW:
        if (!IS_FLAG_SET(systemFlags2, MODIFYING_CARMODE)) {
            // Primera rotación
            auxCarMode = GET_CAR_MODE();  // Cargar modo actual
            menuSystem.clearScreen();
            SET_FLAG(systemFlags2, MODIFYING_CARMODE);

            // IMPORTANTE: Calcular el SIGUIENTE modo desde el actual
            auxCarMode = (auxCarMode + 1) % CAR_MODE_MAX;

            // Renderizar pantalla completa mostrando el modo SIGUIENTE
            menuSystem.renderFn = OledUtils_RenderModeChange_Full;
            menuSystem.renderFlag = true;
        } else {
            // Rotaciones subsiguientes
            auxCarMode = NEXT_CAR_MODE();

            // Solo actualizar el texto del modo
            menuSystem.renderFn = OledUtils_RenderModeChange_ModeOnly;
            menuSystem.renderFlag = true;
        }
        break;

    case UE_ROTATE_CCW:
        if (!IS_FLAG_SET(systemFlags2, MODIFYING_CARMODE)) {
            // Primera rotación
            auxCarMode = GET_CAR_MODE();  // Cargar modo actual
            menuSystem.clearScreen();
            SET_FLAG(systemFlags2, MODIFYING_CARMODE);

            // IMPORTANTE: Calcular el ANTERIOR modo desde el actual
            auxCarMode = (auxCarMode + CAR_MODE_MAX - 1) % CAR_MODE_MAX;

            // Renderizar pantalla completa mostrando el modo ANTERIOR
            menuSystem.renderFn = OledUtils_RenderModeChange_Full;
            menuSystem.renderFlag = true;
        } else {
            // Rotaciones subsiguientes
            auxCarMode = PREV_CAR_MODE();

            // Solo actualizar el texto del modo
            menuSystem.renderFn = OledUtils_RenderModeChange_ModeOnly;
            menuSystem.renderFlag = true;
        }
        break;

    case UE_ENC_SHORT_PRESS:
        if (IS_FLAG_SET(systemFlags2, MODIFYING_CARMODE)) {
            // Confirmar el modo
            SET_CAR_MODE(auxCarMode);
            CLEAR_FLAG(systemFlags2, MODIFYING_CARMODE);

            // Volver al dashboard
            menuSystem.renderFn = OledUtils_RenderDashboard;
            menuSystem.clearScreen();
            menuSystem.renderFlag = true;
        } else {
            // Acción cuando no estamos modificando modo
        }
        break;
        case UE_ENC_LONG_PRESS:
            break;
        case UE_SHORT_PRESS:
            MenuSys_NavigateToMain(&menuSystem);
            menuSystem.clearScreen();
            menuSystem.renderFn = MenuSys_RenderMenu_Wrapper;
            *menuSystem.insideMenuFlag = 1;
            menuSystem.renderFlag = true;
            oled_first_draw = true;
            break;
        case UE_LONG_PRESS:
            if (menuSystem.dashboardRender) {
                menuSystem.renderFn = menuSystem.dashboardRender;
            }
            if (menuSystem.insideMenuFlag) {
                *menuSystem.insideMenuFlag = 0;
            }
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
            if (menuSystem.dashboardRender) {
                menuSystem.renderFn = menuSystem.dashboardRender;
            }
            if (menuSystem.insideMenuFlag) {
                *menuSystem.insideMenuFlag = 0;
            }
            menuSystem.renderFlag = true;
            oled_first_draw = true;
            break;
        default:
            break;
    }
}

void About_UserEventManager(UserEvent_t ev)
{
    // Mantiene tu switch centralizado en ItemEventManager
    // y le pasa "qué wrapper usar" para refrescar el render según flag.
    ItemEventManager(ev, OledUtils_About_Wrapper);
}

void ItemEventManager(UserEvent_t ev, RenderWrapperFn wrapper)
{
    switch (ev)
    {
        case UE_ROTATE_CW:
        case UE_ROTATE_CCW:
            // El control del evento queda ACÁ (como querías)
            if (IS_FLAG_SET(systemFlags2, SHOWSECONDSCREEN)) {
                CLEAR_FLAG(systemFlags2, SHOWSECONDSCREEN);
            } else {
                SET_FLAG(systemFlags2, SHOWSECONDSCREEN);
            }

            // Si hay wrapper, que actualice qué pantalla toca
            if (wrapper) {
                wrapper();
            } else {
                // fallback: al menos forzar redraw
                menuSystem.renderFlag = true;
                oled_first_draw = true;
            }
            break;

        case UE_ENC_SHORT_PRESS:
            MenuSys_NavigateToMain(&menuSystem);
            menuSystem.clearScreen();
            inside_menu_flag = 1;
            menuSystem.renderFlag = true;
            oled_first_draw = true;
            break;
        case UE_ENC_LONG_PRESS:
            break;

        case UE_LONG_PRESS:
            if (menuSystem.dashboardRender) {
                menuSystem.renderFn = menuSystem.dashboardRender;
            }
            if (menuSystem.insideMenuFlag) {
                *menuSystem.insideMenuFlag = 0;
            }
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
            if (menuSystem.dashboardRender) {
                menuSystem.renderFn = menuSystem.dashboardRender;
            }
            if (menuSystem.insideMenuFlag) {
                *menuSystem.insideMenuFlag = 0;
            }
            CLEAR_FLAG(motorTask.motorData.flags, ENABLE_MOVEMENT);
            menuSystem.renderFlag = true;
            oled_first_draw = true;
            break;

        default:
            break;
    }
}
