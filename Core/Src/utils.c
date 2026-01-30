/*
 * utils.c
 *
 *  Created on: Jun 3, 2025
 *      Author: kobac
 */
#include "globals.h"
#include "utils.h"
#include "oled_utils.h"
#include "screenWrappers.h"
#include "utils/macros_utils.h"
#include "stm32f1xx_hal.h"  // para HAL_GPIO_ReadPin
#include "i2c_manager.h"
#include "user_button.h"
#include "main.h"

void USART1_DMA_CheckRx(void);

/* Libera busy al completar */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        usart1_tx_busy = 0;
    }
}


void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        USART1_DMA_CheckRx();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        USART1_DMA_CheckRx();
    }
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	I2C_Manager_OnTxCplt(&i2cManager, hi2c);
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
	I2C_Manager_OnTxCplt(&i2cManager, hi2c);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
	I2C_Manager_OnRxCplt(&i2cManager, hi2c);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2C_Manager_OnRxCplt(&i2cManager, hi2c);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    I2C_Manager_OnError(&i2cManager, hi2c);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	static uint16_t wifi_search_tick_10ms = 0;

    if (htim->Instance == TIM3)
    {
    	cnt_250us_MPU++;
		if (cnt_250us_MPU >= 32) {        // 32 × 250 µs = 8 ms
			cnt_250us_MPU = 0;            // reiniciamos el contador

			// sólo disparamos un nuevo trigger si no estamos
			// ya esperando a procesar datos pendientes
			if ( IS_FLAG_SET(systemFlags, MPU_GET_DATA) ) {
				CLEAR_FLAG(systemFlags, MPU_GET_DATA);
				mpu_trigger = true;
				__NOP();  // <— BREAKPOINT #1: programó trigger a 8 ms
			}
		}
    	cnt_10ms++;
		//Según el modo actual de tcrt, contar o no hacer nada
		TCRTCalibCounter_Task();
        // Cada vez que TIM3 desborda (250 µs), entramos aquí
        if (cnt_10ms >= 40) {          // 40 × 250 µs = 10 000 µs = 10 ms
            cnt_10ms = 0;
            if(!IS_FLAG_SET(systemFlags, OLED_TENMS_PASSED)){
            	SET_FLAG(systemFlags, OLED_TENMS_PASSED);
            }
            // Llamar a las rutinas de 10 ms
            StateLED_Task_10ms(&ledStatus);
            if (IS_FLAG_SET(systemFlags, INIT_CAR)) {
				UserButton_Task10ms(&btnUser);
				ENC_Task_N10ms(&encoder);
			}
            if (IS_FLAG_SET(systemFlags, OLED_READY)) {
				OLED_Task_10ms();
			}
            if(IS_FLAG_SET(systemFlags3, WIFI_SEARCHING)){
				wifi_search_tick_10ms++;
				if (wifi_search_tick_10ms >= 100) {
					wifi_search_tick_10ms = 0;
					if (wifiSearchingTimeout > 0) {
						if (wifiSearchingTimeout >= 100) {
							wifiSearchingTimeout -= 100;
						} else {
							wifiSearchingTimeout = 0;
						}
					}
					if (wifiSearchingTimeout == 0) {
						CLEAR_FLAG(systemFlags3, WIFI_SEARCHING);
						menuSystem.renderFn = OledUtils_RenderWiFiSearchResults_Wrapper;
						menuSystem.renderFlag = true;
						oled_first_draw = false;
						OledUtils_ShowNotificationMs(OledUtils_RenderWiFiSearchCompleteNotification, 2000);
					} else {
						if (!menuSystem.renderFlag) {
							menuSystem.renderFlag = true;
						}
					}
				}
			} else {
				wifi_search_tick_10ms = 0;
            }
        }
    }
}

void TCRTCalibCounter_Task(void)
{
    // Solo contamos durante CALIB_LINE_BLACK / WHITE / OBSTACLES
    if (tcrtTask.mode == TCRT_MODE_READY ||
        tcrtTask.mode == TCRT_MODE_IDLE)
    {
        return;
    }

    tcrt_calib_cnt_phase++;

    if (tcrtTask.mode == TCRT_MODE_CALIB_LINE_BLACK ||
        tcrtTask.mode == TCRT_MODE_CALIB_LINE_WHITE)
    {
        // 3 s = 12 000 ticks de 250 µs
        if (tcrt_calib_cnt_phase >= TICKS_250US_IN_3S) {
        	tcrt_calib_cnt_phase = 0;
        	TCRT5000_RequestModeChange(&tcrtTask);
        }
    }
    else if (tcrtTask.mode == TCRT_MODE_CALIB_OBSTACLES)
    {
        // 5 s = 20 000 ticks de 250 µs
        if (tcrt_calib_cnt_phase >= TICKS_250US_IN_5S) {
        	tcrt_calib_cnt_phase = 0;
        	TCRT5000_RequestModeChange(&tcrtTask);
        }
    }
}

/* Callback que el HAL llama cuando DMA terminó de llenar sensor_raw_data[8] */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
		procesar_flag = true; // Cada vez que hacer ovf el tim 3 que es cada 250us -> 4Khz de muestreo
    }
}

void OLED_Task_10ms(){
	if(oled10msCounter > 10){
		oled10msCounter = 0;
		SET_FLAG(systemFlags, OLED_REFRESH);
	}else{
		oled10msCounter++;
	}

    OledUtils_NotificationTick10ms();
}


/**
 * @brief Se invoca cada 10 ms desde el callback de TIM3.
 *        Verifica el tiempo del led y si ya se acabo cambia el estado.
 * @param btn Puntero a la estructura led, port, y pin del port
 */
void StateLED_Task_10ms(volatile LedStatus_t *led)
{
    // Determinar tiempos según el modo
    uint8_t onTime = 0, offTime = 0;
    CarMode_t modo = NIBBLEH_GET_STATE(systemFlags);
    switch (modo) {
        case IDLE_MODE:
            onTime = LED_IDLE_ONTIME;
            offTime = LED_IDLE_OFFTIME;
            break;
        case FOLLOW_MODE:
            onTime = LED_FOLLOW_ONTIME;
            offTime = LED_FOLLOW_OFFTIME;
            break;
        case TEST_MODE:
            onTime = LED_TEST_ONTIME;
            offTime = LED_TEST_OFFTIME;
            break;
        default:
            onTime = 100;
            offTime = 100;
            break;
    }

    led->counter++;
    uint8_t isActiveLow = IS_FLAG_SET(led->flags, LED_FLAG_ACTIVE_LOW);

    if (IS_FLAG_SET(led->flags, LED_FLAG_IS_ON)) {
        if (led->counter >= onTime) {
            CLEAR_FLAG(led->flags, LED_FLAG_IS_ON);
            led->counter = 0;
            HAL_GPIO_WritePin(led->gpio_port, led->gpio_pin, isActiveLow ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }
    } else {
        if (led->counter >= offTime) {
            SET_FLAG(led->flags, LED_FLAG_IS_ON);
            led->counter = 0;
            HAL_GPIO_WritePin(led->gpio_port, led->gpio_pin, isActiveLow ? GPIO_PIN_RESET : GPIO_PIN_SET);
        }
    }
}




void Handle_ModeChange_ByButton(volatile ButtonState_t *btn, volatile LedStatus_t *led)
{
	ADVANCE_CAR_MODE();  // Cambio cíclico
	switch (GET_CAR_MODE()) {
	case IDLE_MODE:
		led->onTime = LED_IDLE_ONTIME;
		led->offTime = LED_IDLE_OFFTIME;
		break;
	case FOLLOW_MODE:
		led->onTime = LED_FOLLOW_ONTIME;
		led->offTime = LED_FOLLOW_OFFTIME;
		break;
	case TEST_MODE:
		led->onTime = LED_TEST_ONTIME;
		led->offTime = LED_TEST_OFFTIME;
		break;
	}
	// Forzamos que el LED reinicie con nueva secuencia
	SET_FLAG(led->flags, LED_FLAG_DIRTY);
}
