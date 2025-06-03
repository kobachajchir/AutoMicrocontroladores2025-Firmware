/*
 * button_state.h
 *
 *  Created on: Jun 3, 2025
 *      Author: kobac
 */

#ifndef INC_TYPES_BUTTON_STATE_H_
#define INC_TYPES_BUTTON_STATE_H_

#include <stdint.h>
#include "globals.h"
#include "bitmap_type.h"
#include "utils/macros_utils.h"

// ---------------- Máscaras para banderas de usuario (nibble bajo) ----------------
// Estas máscaras usan bits 0..3 de Byte_Flag_Struct.byte:
#define BTN_USER_PREVSTATE     BIT0_MASK  // bit 0 → almacena el estado previo (0 o 1)
#define BTN_USER_SHORT_PRESS   BIT1_MASK  // bit 1 → pulsación corta detectada
#define BTN_USER_LONG_PRESS    BIT2_MASK  // bit 2 → pulsación larga detectada
#define BTN_USER_EASTER_EGG    BIT3_MASK  // Lo usaremos mas adelante para algo

// ---------------- Valores máximos para overflow count (nibble alto) ----------------
#define BTN_USER_OVF_MAX       9U   // si nibbleH > 9, descartamos (equivale a 10 s presionado)

// -------------------------------------------------------------------------------
// Estructura principal:
//   - flags.byte: nibble bajo = banderas (PREVSTATE, SHORT, LONG); nibble alto = contador de overflows (0..15)
//   - counter:   conteo en pasos de 10 ms (0..100)
// -------------------------------------------------------------------------------
typedef struct {
    Byte_Flag_Struct flags;   ///< nibble bajo = flags; nibble alto = overflow counter
    uint8_t          counter; ///< cuenta en pasos de 10 ms (va 0..100)
} ButtonState_t;

#endif /* INC_TYPES_BUTTON_STATE_H_ */
