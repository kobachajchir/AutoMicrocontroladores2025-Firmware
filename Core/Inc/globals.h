// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>

extern volatile uint16_t adc_last_values[8];
extern volatile uint8_t  procesar_flag;
extern volatile uint16_t tim3_overflow_count;
extern volatile uint32_t contador;

#endif // GLOBALS_H
