// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include "types/button_state.h"
#include "types/led_status.h"
#include "types/carmode_type.h"
#include "types/usart_buffer_type.h"
#include "utils/macros_utils.h"
#include "tcrt5000.h"

// =============================================
// LED de Estado (conectado a PC13 a traves de un BJT NPN)
// =============================================
#define LED_PORT        GPIOC
#define LED_PORT_PIN    GPIO_PIN_13

#define TCRT_LED_PORT        GPIOB
#define TCRT_LED_PORT_PIN    GPIO_PIN_0

#define COUNT_IRDATA_TENMS 13 //Aprox 130ms es lo que le lleva al ADC leer 4K samples de 8 sensores
#define INIT_CAR BIT0_MASK
#define PROCESS_IR_DATA BIT1_MASK
//#define A BIT2_MASK

#define USART1_BUFFER_SIZE 64

extern volatile bool  procesar_flag;
extern volatile bool  lanzar_ADC_trigger_flag;
extern volatile uint16_t tim3_overflow_count;
extern volatile uint32_t contador;
extern volatile ButtonState_t btnUser;
extern volatile LedStatus_t ledStatus;
extern volatile Byte_Flag_Struct systemFlags;
extern volatile CarMode_t testMode;
extern USART_Buffer_t usart1Buf;
extern volatile uint16_t sensor_raw_data[ TCRT5000_NUM_SENSORS ];
extern TCRT_LightConfig_t myLight;
extern TCRTHandlerTask tcrtTask;
extern bool pull_cfg[ TCRT5000_NUM_SENSORS ];
extern volatile uint8_t cnt_adc_trigger;
extern volatile uint16_t cnt_10ms;

#endif // GLOBALS_H
