/*
 * user_button.c
 *
 *  Created on: Sep 22, 2025
 *      Author: kobac
 */

#include "user_button.h"
#include "utils/macros_utils.h"

void UserButton_Init(UserButton_Handle_t *btn,
                     GPIO_TypeDef *port,
                     uint16_t pin,
                     UserButton_ActiveLevel activeLevel)
{
    if (!btn) return;
    btn->state.flags.byte = 0;
    btn->state.counter = 0;
    btn->state.port = port;
    btn->state.pin = pin;
    btn->activeLevel = activeLevel;
}

void UserButton_Task10ms(UserButton_Handle_t *btn)
{
    if (!btn) return;

    GPIO_PinState raw = HAL_GPIO_ReadPin(btn->state.port, btn->state.pin);
    bool pressed = (btn->activeLevel == USER_BUTTON_ACTIVE_HIGH)
                   ? (raw == GPIO_PIN_SET)
                   : (raw == GPIO_PIN_RESET);

    if (pressed && !IS_FLAG_SET(btn->state.flags, BTN_USER_PREVSTATE)) {
        btn->state.counter = 1;
        SET_FLAG(btn->state.flags, BTN_USER_PREVSTATE);
        CLEAR_FLAG(btn->state.flags, BTN_USER_SHORT_PRESS);
        CLEAR_FLAG(btn->state.flags, BTN_USER_LONG_PRESS);
        NIBBLEH_SET_STATE(btn->state.flags, 0);
    } else if (pressed && IS_FLAG_SET(btn->state.flags, BTN_USER_PREVSTATE)) {
        if (btn->state.counter < 255) {
            btn->state.counter++;
        }

        if (btn->state.counter > 100) {
            btn->state.counter = 0;
            uint8_t ovf = NIBBLEH_GET_STATE(btn->state.flags);
            ovf++;
            if (ovf > BTN_USER_OVF_MAX) {
                btn->state.flags.byte = 0;
                btn->state.counter = 0;
                return;
            }
            NIBBLEH_SET_STATE(btn->state.flags, ovf);
        }
    } else if (!pressed && IS_FLAG_SET(btn->state.flags, BTN_USER_PREVSTATE)) {
        if (NIBBLEH_GET_STATE(btn->state.flags) == 0) {
            if (btn->state.counter >= 10 && btn->state.counter < 30) {
                SET_FLAG(btn->state.flags, BTN_USER_SHORT_PRESS);
            } else if (btn->state.counter >= 30 && btn->state.counter <= 100) {
                SET_FLAG(btn->state.flags, BTN_USER_LONG_PRESS);
            }
        }

        CLEAR_FLAG(btn->state.flags, BTN_USER_PREVSTATE);
        btn->state.counter = 0;
    }
}
