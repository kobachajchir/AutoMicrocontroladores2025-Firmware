/*
 * utils.h
 *
 *  Created on: Jun 3, 2025
 *      Author: kobac
 */

#ifndef INC_UTILS_H_
#define INC_UTILS_H_

#include "types/button_state.h"
#include "types/led_status.h"
#include "types/carmode_type.h"

// ================== Prototipos ================== //

/**
 * @brief Tarea periódica cada 10 ms.
 *        Gestiona flancos y duración de la pulsación del botón conectado a PB1.
 * @param btn Puntero a la estructura del botón.
 */
void Button_Task_10ms(volatile ButtonState_t *btn);

/**
 * @brief Tarea periódica cada 10 ms.
 *        Alterna el estado ON/OFF del LED según el modo actual.
 * @param led Puntero a la estructura del LED.
 */
void StateLED_Task_10ms(volatile LedStatus_t *led);

/**
 * @brief Cambia de modo global (IDLE → FOLLOW → TEST → IDLE).
 *        Reconfigura tiempos del LED de estado.
 * @param btn Puntero a estructura del botón (no usada actualmente, se deja por si se amplía).
 * @param led Puntero al LED de estado.
 */
void Handle_ModeChange_ByButton(volatile ButtonState_t *btn, volatile LedStatus_t *led);

#endif /* INC_UTILS_H_ */
