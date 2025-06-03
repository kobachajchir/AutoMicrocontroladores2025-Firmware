/**
 * @file    bitmap_type.h
 * @brief   Definición de Byte_Flag_Struct para flags y nibbles
 * @author  Proyecto Auto Microcontroladores 2025
 * @date    2025-06-03
 */

#ifndef INC_UTILS_BITMAP_TYPE_H_
#define INC_UTILS_BITMAP_TYPE_H_


#include <stdint.h>

/**
 * @brief Unión para manejar un byte como:
 *   - 8 bits individuales (bit0…bit7)
 *   - 2 nibbles (bitL = bits [3:0], bitH = bits [7:4])
 *   - acceso global como byte
 */
typedef union {
    struct {
        uint8_t bit0: 1;  ///< Bit 0  (parte baja)
        uint8_t bit1: 1;  ///< Bit 1  (parte baja)
        uint8_t bit2: 1;  ///< Bit 2  (parte baja)
        uint8_t bit3: 1;  ///< Bit 3  (parte baja)
        uint8_t bit4: 1;  ///< Bit 4  (parte alta)
        uint8_t bit5: 1;  ///< Bit 5  (parte alta)
        uint8_t bit6: 1;  ///< Bit 6  (parte alta)
        uint8_t bit7: 1;  ///< Bit 7  (parte alta)
    } bitmap;            ///< Acceso individual a cada bit
    struct {
        uint8_t bitL: 4; ///< Nibble bajo  (bits 0–3)
        uint8_t bitH: 4; ///< Nibble alto  (bits 4–7)
    } nibbles;           ///< Acceso a cada nibble
    uint8_t byte;        ///< Acceso completo a los 8 bits (0–255)
} Byte_Flag_Struct;

#endif /* INC_UTILS_BITMAP_TYPE_H_ */
