/*
 * led_status.h
 *
 *  Created on: Jun 3, 2025
 *      Author: kobac
 */

#ifndef INC_TYPES_LED_STATUS_H_
#define INC_TYPES_LED_STATUS_H_

#include <stdint.h>
#include "bitmap_type.h"
#include "utils/macros_utils.h"
#include "stm32f1xx_hal.h"  // ✅ Necesario para GPIO_TypeDef

// ========================= TIEMPOS POR MODO (×10ms) ========================= //
#define LED_IDLE_ONTIME     10   // 100 ms
#define LED_IDLE_OFFTIME    90   // 900 ms
#define LED_FOLLOW_ONTIME   50   // 500 ms
#define LED_FOLLOW_OFFTIME  50   // 500 ms
#define LED_TEST_ONTIME     80   // 800 ms
#define LED_TEST_OFFTIME    20   // 200 ms

// ========================= BANDERAS INTERNAS ========================= //
#define LED_FLAG_IS_ON       BIT0_MASK  // LED encendido lógicamente
#define LED_FLAG_DIRTY       BIT1_MASK  // Necesita actualizar tiempos
#define LED_FLAG_ACTIVE_LOW  BIT2_MASK  // Lógica inversa (1 = activo en bajo)

// ========================= ESTRUCTURA DE ESTADO ========================= //
typedef struct {
    Byte_Flag_Struct flags;  ///< Flags internas: is_on, dirty, active_low
    uint8_t counter;         ///< Contador decreciente ON/OFF (base 10 ms)
    uint8_t onTime;          ///< Tiempo de encendido
    uint8_t offTime;         ///< Tiempo de apagado
    GPIO_TypeDef    *gpio_port;    ///< Puerto del pin del LED
    uint16_t         gpio_pin;     ///< Número de pin del LED
} LedStatus_t;

#endif /* INC_TYPES_LED_STATUS_H_ */
