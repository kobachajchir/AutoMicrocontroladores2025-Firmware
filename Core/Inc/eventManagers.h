/*
 * eventManagers.h
 *
 *  Created on: Jul 23, 2025
 *      Author: kobac
 */

#ifndef INC_EVENTMANAGERS_H_
#define INC_EVENTMANAGERS_H_

#include "types/userEvent_type.h"
#include "globals.h"

void menuEventManager(UserEvent_t ev);
void dashboardEventManager(UserEvent_t ev);
void motorTestEventManager(UserEvent_t ev);

#endif /* INC_EVENTMANAGERS_H_ */
