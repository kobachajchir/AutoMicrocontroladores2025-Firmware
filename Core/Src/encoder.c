/*
 * encoder.c
 *
 *  Created on: Jun 13, 2025
 *      Author: kobac
 */


// encoder.c
#include "encoder.h"

void ENC_Init(ENC_Handle_t *h, TIM_HandleTypeDef *htim, uint32_t sampleTenMs, uint8_t calibrateCountPerStep, GPIO_TypeDef *port, uint16_t pin)
{
    h->htim         = htim;
    h->sampleTenMs  = sampleTenMs;
    h->dir          = ENC_DIR_NONE;
    h->flags.byte 	= 0;
    h->data.allData = 0;
    h->prevData.allData = 0;
    h->calibrateCountPerStep   = calibrateCountPerStep;      // 2 pulsos raw = 1 paso
    h->accumCount              = 0;
    // Inicializar botón interno
    h->btnState.flags.byte = 0;
    h->btnState.counter    = 0;
    h->btnState.port = port;
    h->btnState.pin = pin;
    h->prevCount = (int16_t)__HAL_TIM_GET_COUNTER(h->htim);
}

void ENC_Start(ENC_Handle_t *h)
{
    HAL_TIM_Encoder_Start(h->htim, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(h->htim, 0);
    h->prevData.pair.vel = 0;
    h->prevData.pair.acc = 0;
}

/**
 * @brief Se invoca cada 10 ms desde el callback de TIM3.
 *        Lee PB1 y actualiza flags, counter y overflow count (nibble alto).
 * @param btn Puntero a la estructura ButtonState_t
 */
void ENC_Button_Task_10ms(ENC_Handle_t *h)
{
    ENC_ButtonState_t *btn = &h->btnState;
    GPIO_PinState raw = HAL_GPIO_ReadPin(btn->port, btn->pin);

    // Considero pressed cuando el pin está a 0
   uint8_t pressed = (raw == GPIO_PIN_RESET);

    __NOP();

    // Flanco de subida (abierto→cerrado)
    if (pressed && !IS_FLAG_SET(btn->flags, ENC_BTN_PREVSTATE)) {
        btn->counter = 1;
        SET_FLAG  (btn->flags, ENC_BTN_PREVSTATE);
        CLEAR_FLAG(btn->flags, ENC_BTN_SHORT_PRESS);
        CLEAR_FLAG(btn->flags, ENC_BTN_LONG_PRESS);
        NIBBLEH_SET_STATE(btn->flags, 0);
    }
    // Mantenido presionado
    else if (pressed && IS_FLAG_SET(btn->flags, ENC_BTN_PREVSTATE)) {
        if (btn->counter < UINT8_MAX) btn->counter++;
        if (btn->counter > 100) {
            btn->counter = 0;
            uint8_t ovf = NIBBLEH_GET_STATE(btn->flags);
            if (ovf < ENC_BTN_OVF_MAX) {
                ovf++;
                NIBBLEH_SET_STATE(btn->flags, ovf);
            }
        }
    }
    // Flanco de bajada (cerrado→abierto)
    else if (!pressed && IS_FLAG_SET(btn->flags, ENC_BTN_PREVSTATE)) {
        if (NIBBLEH_GET_STATE(btn->flags) == 0) {
            if (btn->counter >= 10 && btn->counter < 30) {
                SET_FLAG(btn->flags, ENC_BTN_SHORT_PRESS);
            } else if (btn->counter >= 30 && btn->counter <= 100) {
                SET_FLAG(btn->flags, ENC_BTN_LONG_PRESS);
            }
        }
        CLEAR_FLAG(btn->flags, ENC_BTN_PREVSTATE);
        btn->counter = 0;
    }
}


void ENC_Task_N10ms(ENC_Handle_t *h)
{
    // —— Guards para evitar null pointers ——
    if (!h) return;
    if (!h->htim) return;
    if (!h->htim->Instance) return;  // evita acceder a un periférico no inicializado

    // 1) Debounce y flags del botón interno
    ENC_Button_Task_10ms(h);

    // 2) Leo el contador bruto actual
    int16_t cnt = (int16_t)__HAL_TIM_GET_COUNTER(h->htim);

    // 3) Calculo delta raw con wrap-around
    int32_t delta = (int32_t)cnt - (int32_t)h->prevCount;
    if (delta < -32768) delta += 65536;
    else if (delta >  32768) delta -= 65536;

    // 4) Actualizo prevCount para la próxima
    h->prevCount = cnt;

    // 5) Acumulo esos pulsos raw
    h->accumCount += delta;

    // 6) Extraigo pasos completos
    int32_t steps = h->accumCount / h->calibrateCountPerStep;
    h->accumCount %= h->calibrateCountPerStep;  // resto “incompleto”

    // 7) Velocidad y aceleración en pasos
    h->data.pair.vel      = (uint16_t)steps;
    h->data.pair.acc      = (uint16_t)(steps - (int16_t)h->prevData.pair.vel);
    h->prevData.allData   = h->data.allData;

    // 8) Marco update y dirección
    if (steps != 0)
        SET_FLAG(h->flags, ENC_FLAG_UPDATED);

    if (steps >  0)           h->dir = ENC_DIR_CW;
    else if (steps < 0)       h->dir = ENC_DIR_CCW;
    else                      h->dir = ENC_DIR_NONE;
}


ENC_Direction_t ENC_GetDirection(ENC_Handle_t *h)
{
    return h->dir;
}

uint16_t ENC_GetVelocity(ENC_Handle_t *h)
{
    return h->data.pair.vel;
}

uint16_t ENC_GetAcceleration(ENC_Handle_t *h)
{
    return h->data.pair.acc;
}
