/*
 * ui_event_router.h
 *
 *  Created on: Sep 22, 2025
 *      Author: kobac
 */

#ifndef INC_UI_EVENT_ROUTER_H_
#define INC_UI_EVENT_ROUTER_H_

#include "types/userEvent_type.h"

#ifdef __cplusplus
extern "C" {
#endif

void UiEventRouter_HandleEvent(UserEvent_t ev);

#ifdef __cplusplus
}
#endif

#endif /* INC_UI_EVENT_ROUTER_H_ */
