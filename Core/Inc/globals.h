// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include "types/button_state.h"
#include "encoder.h"
#include "types/led_status.h"
#include "types/carmode_type.h"
#include "types/usart_buffer_type.h"
#include "utils/macros_utils.h"
#include "tcrt5000.h"
#include "motor_control.h"
#include "oled_ssd1306_dma.h"
#include "menusystem.h"

// =============================================
// LED de Estado (conectado a PC13 a traves de un BJT NPN)
// =============================================

//Defines de tiempos
//#define COUNT_IRDATA_TENMS 13
#define TICKS_250US_IN_3S      12000U
#define TICKS_250US_IN_5S      20000U

//Variables flag globales
#define INIT_CAR BIT0_MASK
#define PROCESS_IR_DATA BIT1_MASK
#define TCRT_CALIB_BLACK_AND_WHITE_COMPLETE BIT2_MASK
#define TCRT_CALIB_OBSTACLES_COMPLETE BIT3_MASK
#define MPU_GET_DATA BIT4_MASK
#define OLED_TENMS_PASSED BIT5_MASK
//#define A BIT2_MASK

//Definicion de tamanios
#define USART1_BUFFER_SIZE 64

#define I2C_ADDR_OLED     0x3C
#define I2C_ADDR_MPU6050  0x68

#define INSIDE_MENU (inside_menu_flag)

//Variables globales
extern volatile bool  procesar_flag;
extern volatile bool  lanzar_ADC_trigger_flag;
extern volatile uint16_t tim3_overflow_count;
extern volatile uint32_t contador;
extern volatile LedStatus_t ledStatus;
extern volatile Byte_Flag_Struct systemFlags;
extern volatile CarMode_t carMode;
extern volatile uint16_t sensor_raw_data[ TCRT5000_NUM_SENSORS ];
extern TCRT_LightConfig_t myLight;
extern bool pull_cfg[ TCRT5000_NUM_SENSORS ];
extern volatile uint8_t cnt_adc_trigger;
extern volatile uint16_t cnt_10ms;
extern volatile uint32_t cnt_10us;
extern volatile uint32_t tcrt_calib_cnt_phase;  // contador de 10 µs para la fase actual
extern volatile uint8_t i2c_tx_busy_flag;

extern USART_Buffer_t usart1Buf;
extern TCRTHandlerTask tcrtTask;
extern MotorControl_Handle motorTask;
extern OLED_HandleTypeDef oledTask;
extern ButtonState_t btnUser;
extern ENC_Handle_t encoder;

extern MenuSystem    menuSystem;
extern SubMenu       mainMenu, submenu1, submenu2, submenu3;
extern MenuItem      mainMenuItems[], submenu1Items[], submenu2Items[], submenu3Items[];
extern volatile uint8_t inside_menu_flag;
extern const uint8_t* generic_icon;

#endif // GLOBALS_H
