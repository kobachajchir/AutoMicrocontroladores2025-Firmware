// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include "types/button_state.h"
#include "types/led_status.h"
#include "types/carmode_type.h"
#include "utils/macros_utils.h"

extern volatile uint16_t adc_last_values[8];
extern volatile uint8_t  procesar_flag;
extern volatile uint16_t tim3_overflow_count;
extern volatile uint32_t contador;
extern volatile ButtonState_t btnUser;
extern volatile LedStatus_t ledStatus;
extern volatile Byte_Flag_Struct systemFlags;
extern volatile CarMode_t testMode;

// =============================================
// LED de Estado (conectado a PC13 a traves de un BJT NPN)
// =============================================
#define LED_PORT        GPIOC
#define LED_PORT_PIN    GPIO_PIN_13

#define COUNT_IRDATA_TENMS 13 //Aprox 130ms es lo que le lleva al ADC leer 4K samples de 8 sensores

#define INIT_CAR BIT0_MASK
#define PROCESS_IR_DATA BIT1_MASK

#endif // GLOBALS_H
