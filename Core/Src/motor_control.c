/*
 * motor_control.c
 *
 *  Created on: Jun 7, 2025
 *      Author: kobac
 */


#include "motor_control.h"

// Prototipos internos
void Motor_AttachMethods(MotorControl_Handle *self);
static HAL_StatusTypeDef Motor_Init(MotorControl_Handle *self);
static HAL_StatusTypeDef Motor_SetLeftForward(MotorControl_Handle *self, uint8_t speed);
static HAL_StatusTypeDef Motor_SetLeftBackward(MotorControl_Handle *self, uint8_t speed);
static HAL_StatusTypeDef Motor_SetRightForward(MotorControl_Handle *self, uint8_t speed);
static HAL_StatusTypeDef Motor_SetRightBackward(MotorControl_Handle *self, uint8_t speed);
static HAL_StatusTypeDef Motor_StopLeft(MotorControl_Handle *self);
static HAL_StatusTypeDef Motor_StopRight(MotorControl_Handle *self);
static HAL_StatusTypeDef Motor_SetBoth(MotorControl_Handle *self, uint8_t direction, uint8_t speed);
static HAL_StatusTypeDef Motor_SetSpeed(MotorControl_Handle *self, int8_t left_speed, int8_t right_speed);

void Motor_AttachMethods(MotorControl_Handle *self)
{
    if (!self) return;
    self->init               = Motor_Init;
    self->set_left_forward   = Motor_SetLeftForward;
    self->set_left_backward  = Motor_SetLeftBackward;
    self->set_right_forward  = Motor_SetRightForward;
    self->set_right_backward = Motor_SetRightBackward;
    self->stop_left          = Motor_StopLeft;
    self->stop_right         = Motor_StopRight;
    self->set_both           = Motor_SetBoth;
    self->set_speed          = Motor_SetSpeed;
}

static HAL_StatusTypeDef Motor_Init(MotorControl_Handle *self)
{
    if (!self || !self->htim || !self->compute_params) return HAL_ERROR;

    uint16_t prescaler = 0, period = 0;
    uint32_t timer_clk = HAL_RCC_GetPCLK1Freq() * 2;

    self->compute_params(self->desired_pwm_freq, timer_clk, &prescaler, &period);

    self->htim->Init.Prescaler     = prescaler;
    self->htim->Init.Period        = period;
    self->htim->Init.CounterMode   = TIM_COUNTERMODE_UP;
    self->htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    if (HAL_TIM_PWM_Init(self->htim) != HAL_OK)
        return HAL_ERROR;

    self->init               = Motor_Init;
    self->set_left_forward   = Motor_SetLeftForward;
    self->set_left_backward  = Motor_SetLeftBackward;
    self->set_right_forward  = Motor_SetRightForward;
    self->set_right_backward = Motor_SetRightBackward;
    self->stop_left          = Motor_StopLeft;
    self->stop_right         = Motor_StopRight;
    self->set_both           = Motor_SetBoth;

    return HAL_OK;
}

static HAL_StatusTypeDef Motor_SetLeftForward(MotorControl_Handle *self, uint8_t speed)
{
    if (!self || speed > 255) return HAL_ERROR;
    HAL_TIM_PWM_Stop(self->htim, self->left_backward_channel);
    __HAL_TIM_SET_COMPARE(self->htim, self->left_backward_channel, 0);
    uint16_t duty = ((self->htim->Init.Period + 1) * speed) / 255;
    __HAL_TIM_SET_COMPARE(self->htim, self->left_forward_channel, duty);
    return HAL_TIM_PWM_Start(self->htim, self->left_forward_channel);
}

static HAL_StatusTypeDef Motor_SetLeftBackward(MotorControl_Handle *self, uint8_t speed)
{
    if (!self || speed > 255) return HAL_ERROR;
    HAL_TIM_PWM_Stop(self->htim, self->left_forward_channel);
    __HAL_TIM_SET_COMPARE(self->htim, self->left_forward_channel, 0);
    uint16_t duty = ((self->htim->Init.Period + 1) * speed) / 255;
    __HAL_TIM_SET_COMPARE(self->htim, self->left_backward_channel, duty);
    return HAL_TIM_PWM_Start(self->htim, self->left_backward_channel);
}

static HAL_StatusTypeDef Motor_SetRightForward(MotorControl_Handle *self, uint8_t speed)
{
    if (!self || speed > 255) return HAL_ERROR;
    HAL_TIM_PWM_Stop(self->htim, self->right_backward_channel);
    __HAL_TIM_SET_COMPARE(self->htim, self->right_backward_channel, 0);
    uint16_t duty = ((self->htim->Init.Period + 1) * speed) / 255;
    __HAL_TIM_SET_COMPARE(self->htim, self->right_forward_channel, duty);
    return HAL_TIM_PWM_Start(self->htim, self->right_forward_channel);
}

static HAL_StatusTypeDef Motor_SetRightBackward(MotorControl_Handle *self, uint8_t speed)
{
    if (!self || speed > 255) return HAL_ERROR;
    HAL_TIM_PWM_Stop(self->htim, self->right_forward_channel);
    __HAL_TIM_SET_COMPARE(self->htim, self->right_forward_channel, 0);
    uint16_t duty = ((self->htim->Init.Period + 1) * speed) / 255;
    __HAL_TIM_SET_COMPARE(self->htim, self->right_backward_channel, duty);
    return HAL_TIM_PWM_Start(self->htim, self->right_backward_channel);
}

static HAL_StatusTypeDef Motor_StopLeft(MotorControl_Handle *self)
{
    if (!self) return HAL_ERROR;
    HAL_TIM_PWM_Stop(self->htim, self->left_forward_channel);
    HAL_TIM_PWM_Stop(self->htim, self->left_backward_channel);
    __HAL_TIM_SET_COMPARE(self->htim, self->left_forward_channel, 0);
    __HAL_TIM_SET_COMPARE(self->htim, self->left_backward_channel, 0);
    return HAL_OK;
}

static HAL_StatusTypeDef Motor_StopRight(MotorControl_Handle *self)
{
    if (!self) return HAL_ERROR;
    HAL_TIM_PWM_Stop(self->htim, self->right_forward_channel);
    HAL_TIM_PWM_Stop(self->htim, self->right_backward_channel);
    __HAL_TIM_SET_COMPARE(self->htim, self->right_forward_channel, 0);
    __HAL_TIM_SET_COMPARE(self->htim, self->right_backward_channel, 0);
    return HAL_OK;
}

static HAL_StatusTypeDef Motor_SetBoth(MotorControl_Handle *self, uint8_t direction, uint8_t speed)
{
    if (!self || speed > 255) return HAL_ERROR;

    switch (direction) {
        case MOTOR_DIR_FORWARD:
            self->set_left_forward(self, speed);
            self->set_right_forward(self, speed);
            break;
        case MOTOR_DIR_BACKWARD:
            self->set_left_backward(self, speed);
            self->set_right_backward(self, speed);
            break;
        default:
            return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef Motor_SetSpeed(MotorControl_Handle *self, int8_t left_speed, int8_t right_speed)
{
    if (!self) return HAL_ERROR;

    HAL_StatusTypeDef status = HAL_OK;

    // Motor izquierdo
    if (left_speed > 0) {
        status |= self->set_left_forward(self, (uint8_t)left_speed);
    } else if (left_speed < 0) {
        status |= self->set_left_backward(self, (uint8_t)(-left_speed));
    } else {
        status |= self->stop_left(self);
    }

    // Motor derecho
    if (right_speed > 0) {
        status |= self->set_right_forward(self, (uint8_t)right_speed);
    } else if (right_speed < 0) {
        status |= self->set_right_backward(self, (uint8_t)(-right_speed));
    } else {
        status |= self->stop_right(self);
    }

    return status;
}

