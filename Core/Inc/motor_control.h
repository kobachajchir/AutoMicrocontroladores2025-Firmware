/*
 * motor_control.h
 *
 *  Created on: Jun 7, 2025
 *      Author: kobac
 */

#ifndef INC_TYPES_MOTOR_CONTROL_H_
#define INC_TYPES_MOTOR_CONTROL_H_

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

// Dirección de ambos motores
#define MOTOR_DIR_FORWARD   0x01
#define MOTOR_DIR_BACKWARD  0x02

typedef struct MotorControl_Handle MotorControl_Handle;

// Tipo de función para calcular prescaler y periodo
typedef void (*TimerComputeParamsFunc)(uint32_t target_freq_hz,
                                        uint32_t timer_clk_hz,
                                        uint16_t *prescaler_out,
                                        uint16_t *period_out);

	struct MotorControl_Handle {
		TIM_HandleTypeDef *htim;
		uint32_t desired_pwm_freq;
		TimerComputeParamsFunc compute_params;

		uint32_t left_forward_channel;
		uint32_t left_backward_channel;
		uint32_t right_forward_channel;
		uint32_t right_backward_channel;

		// Métodos
		HAL_StatusTypeDef (*init)(MotorControl_Handle *self);
		HAL_StatusTypeDef (*set_left_forward)(MotorControl_Handle *self, uint8_t speed);
		HAL_StatusTypeDef (*set_left_backward)(MotorControl_Handle *self, uint8_t speed);
		HAL_StatusTypeDef (*set_right_forward)(MotorControl_Handle *self, uint8_t speed);
		HAL_StatusTypeDef (*set_right_backward)(MotorControl_Handle *self, uint8_t speed);
		HAL_StatusTypeDef (*stop_left)(MotorControl_Handle *self);
		HAL_StatusTypeDef (*stop_right)(MotorControl_Handle *self);
		HAL_StatusTypeDef (*set_both)(MotorControl_Handle *self, uint8_t direction, uint8_t speed);
		HAL_StatusTypeDef (*set_speed)(MotorControl_Handle *self, int8_t left_speed, int8_t right_speed);

	};

#ifdef __cplusplus
}
#endif


#endif /* INC_TYPES_MOTOR_CONTROL_H_ */
