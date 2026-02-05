/*
 * oled_handle.h
 *
 *  Created on: Sep 21, 2026
 *      Author: Codex
 */

#ifndef INC_OLED_HANDLE_H_
#define INC_OLED_HANDLE_H_

#include "menusystem.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct OledNotificationState {
	bool active;
	RenderFunction renderFn;
	uint16_t timeoutTicks;
	uint16_t totalTicks;
	bool needsFullRender;
	RenderFunction previousRenderFn;
	bool previousAllowPeriodicRefresh;
	bool previousRenderFlag;
	struct {
		bool valid;
		RenderFunction renderFn;
		uint16_t remainingTicks;
	} suspended;
} OledNotificationState;

typedef struct OledHandle {
	OledNotificationState notification;
} OledHandle;

#endif /* INC_OLED_HANDLE_H_ */
