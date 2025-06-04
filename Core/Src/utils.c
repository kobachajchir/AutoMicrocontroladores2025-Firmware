/*
 * utils.c
 *
 *  Created on: Jun 3, 2025
 *      Author: kobac
 */
#include "globals.h"
#include "utils.h"
#include "utils/macros_utils.h"
#include "stm32f1xx_hal.h"  // para HAL_GPIO_ReadPin

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        // Cada vez que TIM3 desborda (250 µs), entramos aquí
        cnt_10ms++;
        if (cnt_10ms >= 40) {          // 40 × 250 µs = 10 000 µs = 10 ms
            cnt_10ms = 0;
            // Llamar a las rutinas de 10 ms
            Button_Task_10ms(&btnUser);
            StateLED_Task_10ms(&ledStatus);

            // Si deseas seguir con el conteo de ~130 ms para PROCESS_IR_DATA:
            // static uint16_t tim3_overflow_count = 0;
            // tim3_overflow_count++;
            // if (tim3_overflow_count >= (130000 / 250)) {  // 130 ms / 250 µs ≈ 520
            //     tim3_overflow_count = 0;
            //     SET_FLAG(systemFlags, PROCESS_IR_DATA);
            // }
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

/**
 * @brief Se invoca cada 10 ms desde el callback de TIM3.
 *        Lee PB1 y actualiza flags, counter y overflow count (nibble alto).
 * @param btn Puntero a la estructura ButtonState_t
 */
void Button_Task_10ms(volatile ButtonState_t *btn)
{
    uint8_t raw = (uint8_t)HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);

    if (raw == 1 && !IS_FLAG_SET(btn->flags, BTN_USER_PREVSTATE)) {
        // Flanco de subida
        btn->counter = 1;
        SET_FLAG(btn->flags, BTN_USER_PREVSTATE);
        CLEAR_FLAG(btn->flags, BTN_USER_SHORT_PRESS);
        CLEAR_FLAG(btn->flags, BTN_USER_LONG_PRESS);
        NIBBLEH_SET_STATE(btn->flags, 0);
    }
    else if (raw == 1 && IS_FLAG_SET(btn->flags, BTN_USER_PREVSTATE)) {
        // Mantenido
        if (btn->counter < 255)
            btn->counter++;

        if (btn->counter > 100) {
            btn->counter = 0;
            uint8_t ovf = NIBBLEH_GET_STATE(btn->flags);
            ovf++;
            if (ovf > 9) {
                btn->flags.byte = 0;
                btn->counter = 0;
                return;
            }
            NIBBLEH_SET_STATE(btn->flags, ovf);
        }
    }
    else if (raw == 0 && IS_FLAG_SET(btn->flags, BTN_USER_PREVSTATE)) {
        // Flanco de bajada
        if (NIBBLEH_GET_STATE(btn->flags) == 0) {
            if (btn->counter >= 10 && btn->counter < 30) {
                SET_FLAG(btn->flags, BTN_USER_SHORT_PRESS);
            }
            else if (btn->counter >= 30 && btn->counter <= 100) {
                SET_FLAG(btn->flags, BTN_USER_LONG_PRESS);
            }
        }

        CLEAR_FLAG(btn->flags, BTN_USER_PREVSTATE);
        btn->counter = 0;
    }
}


void Handle_ModeChange_ByButton(volatile ButtonState_t *btn, volatile LedStatus_t *led)
{
	ADVANCE_CAR_MODE();  // Cambio cíclico
	testMode = GET_CAR_MODE();
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
