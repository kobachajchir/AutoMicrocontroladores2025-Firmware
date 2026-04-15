/*
 * motor_control.c
 *
 *  Created on: Jun 7, 2025
 *      Author: kobac
 */


#include "motor_control.h"
extern void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);


/**
 * @brief  Initialize the motor PWM timer based on handle configuration
 * @param  self  Pointer to MotorControl_Handle
 * @retval HAL status
 */
HAL_StatusTypeDef Motor_Init(MotorControl_Handle *self)
{
    TIM_OC_InitTypeDef sConfig = {0};

    if (!self || !self->htim)
        return HAL_ERROR;

    /* 1) Inicialización de la base PWM */
    if (self->compute_params) {
        uint16_t prescaler = 0, period = 0;
        uint32_t timer_clk = HAL_RCC_GetPCLK1Freq();
        self->compute_params(self->desired_pwm_freq, timer_clk, &prescaler, &period);
        self->htim->Init.Prescaler = prescaler;
        self->htim->Init.Period    = period;
    } else {
        self->htim->Init.Prescaler     = 71;  // 72MHz/72 = 1MHz
        self->htim->Init.Period        = 255; // 1MHz/256 ≃ 3.9kHz
    }
    self->htim->Init.CounterMode   = TIM_COUNTERMODE_UP;
    self->htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    if (HAL_TIM_PWM_Init(self->htim) != HAL_OK)
        return HAL_ERROR;

    /* 2) Configurar GPIO AF para los canales (post‑init) */
    HAL_TIM_MspPostInit(self->htim);

    /* 3) Arrancar el contador del timer */
    if (HAL_TIM_Base_Start(self->htim) != HAL_OK)
        return HAL_ERROR;

    /* 4) Configurar cada canal en modo PWM1, pulse = 0 */
    sConfig.OCMode     = TIM_OCMODE_PWM1;
    sConfig.Pulse      = 0;
    sConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfig.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(self->htim, &sConfig, self->left_forward_channel);
    HAL_TIM_PWM_ConfigChannel(self->htim, &sConfig, self->left_backward_channel);
    HAL_TIM_PWM_ConfigChannel(self->htim, &sConfig, self->right_forward_channel);
    HAL_TIM_PWM_ConfigChannel(self->htim, &sConfig, self->right_backward_channel);

    /* 5) Arrancar PWM en todos los canales */
    HAL_TIM_PWM_Start(self->htim, self->left_forward_channel);
    HAL_TIM_PWM_Start(self->htim, self->left_backward_channel);
    HAL_TIM_PWM_Start(self->htim, self->right_forward_channel);
    HAL_TIM_PWM_Start(self->htim, self->right_backward_channel);

    return HAL_OK;
}


/**
 * @brief  Set left motor forward direction and speed
 * @param  self   Pointer to MotorControl_Handle
 * @param  speed  PWM duty cycle (0-255)
 */
void Motor_SetLeftForward(MotorControl_Handle *self, uint8_t speed)
{
    __HAL_TIM_SET_COMPARE(self->htim, self->left_forward_channel, speed);
    __HAL_TIM_SET_COMPARE(self->htim, self->left_backward_channel, 0);
}

/**
 * @brief  Set left motor backward direction and speed
 * @param  self   Pointer to MotorControl_Handle
 * @param  speed  PWM duty cycle (0-255)
 */
void Motor_SetLeftBackward(MotorControl_Handle *self, uint8_t speed)
{
    __HAL_TIM_SET_COMPARE(self->htim, self->left_forward_channel, 0);
    __HAL_TIM_SET_COMPARE(self->htim, self->left_backward_channel, speed);
}

/**
 * @brief  Set right motor forward direction and speed
 * @param  self   Pointer to MotorControl_Handle
 * @param  speed  PWM duty cycle (0-255)
 */
void Motor_SetRightForward(MotorControl_Handle *self, uint8_t speed)
{
    __HAL_TIM_SET_COMPARE(self->htim, self->right_forward_channel, speed);
    __HAL_TIM_SET_COMPARE(self->htim, self->right_backward_channel, 0);
}

/**
 * @brief  Set right motor backward direction and speed
 * @param  self   Pointer to MotorControl_Handle
 * @param  speed  PWM duty cycle (0-255)
 */
void Motor_SetRightBackward(MotorControl_Handle *self, uint8_t speed)
{
    __HAL_TIM_SET_COMPARE(self->htim, self->right_forward_channel, 0);
    __HAL_TIM_SET_COMPARE(self->htim, self->right_backward_channel, speed);
}

/**
 * @brief  Stop left motor
 * @param  self  Pointer to MotorControl_Handle
 */
void Motor_StopLeft(MotorControl_Handle *self)
{
    __HAL_TIM_SET_COMPARE(self->htim, self->left_forward_channel, 0);
    __HAL_TIM_SET_COMPARE(self->htim, self->left_backward_channel, 0);
}

/**
 * @brief  Stop right motor
 * @param  self  Pointer to MotorControl_Handle
 */
void Motor_StopRight(MotorControl_Handle *self)
{
    __HAL_TIM_SET_COMPARE(self->htim, self->right_forward_channel, 0);
    __HAL_TIM_SET_COMPARE(self->htim, self->right_backward_channel, 0);
}

/**
 * @brief  Stop both motors
 * @param  self  Pointer to MotorControl_Handle
 */
void Motor_StopBoth(MotorControl_Handle *self)
{
    Motor_StopLeft(self);
    Motor_StopRight(self);
}

/**
 * @brief  Set both motors speed and direction
 * @param  self   Pointer to MotorControl_Handle
 * @param  speed  PWM duty cycle (0-255)
 * @param  dir    Direction (MOTOR_DIR_FORWARD or MOTOR_DIR_BACKWARD)
 */
void Motor_SetBoth(MotorControl_Handle *self, uint8_t speed, Motor_Direction_t dir)
{
    if (dir == MOTOR_DIR_FORWARD) {
        Motor_SetLeftForward(self, speed);
        Motor_SetRightForward(self, speed);
    } else {
        Motor_SetLeftBackward(self, speed);
        Motor_SetRightBackward(self, speed);
    }
}

