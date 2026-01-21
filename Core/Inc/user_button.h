/*
 * user_button.h
 *
 *  Created on: Sep 22, 2025
 *      Author: kobac
 */

#ifndef INC_USER_BUTTON_H_
#define INC_USER_BUTTON_H_

#include <stdbool.h>
#include "types/button_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    USER_BUTTON_ACTIVE_LOW = 0,
    USER_BUTTON_ACTIVE_HIGH = 1
} UserButton_ActiveLevel;

typedef struct {
    ButtonState_t state;
    UserButton_ActiveLevel activeLevel;
} UserButton_Handle_t;

void UserButton_Init(UserButton_Handle_t *btn,
                     GPIO_TypeDef *port,
                     uint16_t pin,
                     UserButton_ActiveLevel activeLevel);

void UserButton_Task10ms(UserButton_Handle_t *btn);

#ifdef __cplusplus
}
#endif

#endif /* INC_USER_BUTTON_H_ */
