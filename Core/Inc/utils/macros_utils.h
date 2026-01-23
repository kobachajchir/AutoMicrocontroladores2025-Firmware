/**
 * @file    bitmap_type.h
 * @brief   Macros para manipular Byte_Flag_Struct (flags y nibbles)
 * @author  Proyecto Auto Microcontroladores 2025
 * @date    2025-06-03
 */

#ifndef INC_UTILS_MACROS_UTILS_H_
#define INC_UTILS_MACROS_UTILS_H_

#include <stdint.h>
#include "../types/bitmap_type.h"
#include "../types/carmode_type.h"

extern volatile Byte_Flag_Struct systemFlags;
extern volatile Byte_Flag_Struct systemFlags2;
extern volatile Byte_Flag_Struct carModeFlags;

// ====================== Máscaras ========================= //

// --- Bits individuales ---
#define BIT0_MASK   0x01  // 0000 0001
#define BIT1_MASK   0x02  // 0000 0010
#define BIT2_MASK   0x04  // 0000 0100
#define BIT3_MASK   0x08  // 0000 1000
#define BIT4_MASK   0x10  // 0001 0000
#define BIT5_MASK   0x20  // 0010 0000
#define BIT6_MASK   0x40  // 0100 0000
#define BIT7_MASK   0x80  // 1000 0000

// --- Pares de bits (si alguna vez los necesitas) ---
#define BITS01_MASK 0x03  // 0000 0011
#define BITS23_MASK 0x0C  // 0000 1100
#define BITS45_MASK 0x30  // 0011 0000
#define BITS67_MASK 0xC0  // 1100 0000

// --- Nibbles ---
#define NIBBLE_L_MASK 0x0F  // 0000 1111 (bits 0..3)
#define NIBBLE_H_MASK 0xF0  // 1111 0000 (bits 4..7)

// --------------------------------------------
// FLAGS (parte baja del byte)
// --------------------------------------------
// Macros generales para manejar flags en Byte_Flag_Struct.

// Setea (pone a 1) los bits indicados en BIT_MASK
#define SET_FLAG(flag_struct, BIT_MASK)    ((flag_struct).byte |=  (uint8_t)(BIT_MASK))

// Limpia (pone a 0) los bits indicados en BIT_MASK
#define CLEAR_FLAG(flag_struct, BIT_MASK)  ((flag_struct).byte &= (uint8_t)(~(BIT_MASK)))

// Invierte (toggle) los bits indicados en BIT_MASK
#define TOGGLE_FLAG(flag_struct, BIT_MASK) ((flag_struct).byte ^=  (uint8_t)(BIT_MASK))

// Verifica si al menos uno de los bits de BIT_MASK está a 1
// Devuelve != 0 si alguno está activo
#define IS_FLAG_SET(flag_struct, BIT_MASK) (((flag_struct).byte & (BIT_MASK)) != 0U)

// --------------------------------------------
// NIBBLES (parte alta y baja del byte)
// --------------------------------------------
// Permite codificar un “estado” de 4 bits en el nibble bajo (bits 0..3)
// o en el nibble alto (bits 4..7). El parámetro 'state' debe entrar en 0..15.

// Setea el nibble bajo (bits 0–3) con el valor 'state' (0..15),
// preservando el nibble alto (bits 4..7).
#define NIBBLEL_SET_STATE(object, state)  \
    do { \
        (object).byte = (uint8_t)(((object).byte & NIBBLE_H_MASK) | ((uint8_t)((state) & NIBBLE_L_MASK))); \
    } while (0)

// Setea el nibble alto (bits 4–7) con el valor 'state' (0..15),
// preservando el nibble bajo (bits 0..3).
#define NIBBLEH_SET_STATE(object, state)  \
    do { \
        (object).byte = (uint8_t)(((object).byte & NIBBLE_L_MASK) | ((uint8_t)(((state) & NIBBLE_L_MASK) << 4))); \
    } while (0)

// Obtiene el valor (0..15) almacenado en el nibble alto (bits 4–7)
#define NIBBLEH_GET_STATE(object)  (uint8_t)(((object).byte & NIBBLE_H_MASK) >> 4)

// Obtiene el valor (0..15) almacenado en el nibble bajo (bits 0–3)
#define NIBBLEL_GET_STATE(object)  (uint8_t)((object).byte & NIBBLE_L_MASK)

// Macros para leer/escribir el modo en nibble H
#define GET_CAR_MODE()         (NIBBLEH_GET_STATE(carModeFlags))
#define SET_CAR_MODE(m)        NIBBLEH_SET_STATE(carModeFlags, (m) & 0x0F)
#define ADVANCE_CAR_MODE()     SET_CAR_MODE((GET_CAR_MODE() + 1) % CAR_MODE_MAX)

#endif /* INC_UTILS_MACROS_UTILS_H_ */
