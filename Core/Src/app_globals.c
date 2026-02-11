/*
 * app_globals.c
 *
 *  Created on: Sep 17, 2025
 *      Author: codex
 */

#include "globals.h"

volatile bool procesar_flag = false;
volatile bool lanzar_ADC_trigger_flag = false;
volatile bool mpu_trigger = false;

volatile LedStatus_t ledStatus;

/* Byte flags */
volatile Byte_Flag_Struct systemFlags;
volatile Byte_Flag_Struct systemFlags2;
volatile Byte_Flag_Struct systemFlags3;
volatile Byte_Flag_Struct carModeFlags;

/* Sensores */
volatile uint16_t sensor_raw_data[TCRT5000_NUM_SENSORS];

/* Contadores */
volatile uint8_t  cnt_adc_trigger = 0;
volatile uint16_t cnt_250us_MPU = 0;
volatile uint16_t cnt_10ms = 0;
volatile uint32_t cnt_10us = 0;
volatile uint32_t tcrt_calib_cnt_phase = 0;

/* UI / inputs */
volatile uint8_t  inside_menu_flag = 0;
volatile uint8_t  encoder_fast_scroll_enabled = 0;
volatile uint16_t encoderValue = 0;
volatile uint8_t  oled10msCounter = 0;

/* Motores */
volatile uint8_t motorSelected = 0; // 0: izquierdo, 1: derecho, 2: ambos
volatile uint8_t motorSpeed    = 100;
volatile uint8_t motorDir      = 0; // 0: adelante, 1: atrás

/* TCRT config */
TCRT_LightConfig_t tcrtLight;

/* WiFi */
uint16_t wifiSearchingTimeout = WIFIDEFAULTSEARCHTIMEOUT;
uint8_t networksFound = 0;
volatile IPStruct_t espStaIp = {0u, 0u, 0u, 0u};
volatile IPStruct_t espApIp = {0u, 0u, 0u, 0u};
char espFirmwareVersion[33] = {0};

/* Pull config */
bool pull_cfg[TCRT5000_NUM_SENSORS] = {
    TCRT_PULL_UP,    // canal 0: línea central  (no invertir)
    TCRT_PULL_UP,    // canal 1: línea lateral izq
    TCRT_PULL_UP,    // canal 2: línea lateral der
    TCRT_PULL_DOWN,  // canal 3: obstáculo diag izq (invertir)
    TCRT_PULL_DOWN,  // canal 4: obstáculo frente izq
    TCRT_PULL_DOWN,  // canal 5: obstáculo centro
    TCRT_PULL_DOWN,  // canal 6: obstáculo frente der
    TCRT_PULL_DOWN   // canal 7: obstáculo diag der
};

/* USART1 DMA RX */
uint8_t usart1_rx_dma_buf[USART1_RX_DMA_BUF_LEN];
volatile uint16_t usart1_rx_prev_pos = 0;
volatile uint8_t usart1_feed_pending = 0;
volatile uint8_t usart1_tx_busy = 0;

/* --- Handlers de librerias --- */
USART_Buffer_t usart1Buf;
TCRTHandlerTask tcrtTask;
MotorControl_Handle motorTask;
MPU6050_Handle_t mpuTask;
UserButton_Handle_t btnUser;
ENC_Handle_t encoder;
I2C_ManagerHandle i2cManager;
CarMode_t auxCarMode;
volatile CarMode_t carMode;
volatile uint16_t tim3_overflow_count = 0;
volatile uint32_t contador = 0;

volatile bool oled_first_draw = false;

/* OLED handle global */
OledHandle oledHandle = {0};
