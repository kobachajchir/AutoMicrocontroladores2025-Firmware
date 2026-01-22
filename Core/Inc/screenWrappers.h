/*
 * screenWrappers.h
 *
 *  Created on: Jul 23, 2025
 *      Author: kobac
 */

#ifndef INC_SCREENWRAPPERS_H_
#define INC_SCREENWRAPPERS_H_

#include "globals.h"

void OledUtils_DrawItem_Wrapper(const MenuItem *item, int y, bool selected);
void OledUtils_RenderDashboard_Wrapper(void);
void OledUtils_RenderTestScreen_Wrapper(void);
void MenuSys_RenderMenu_Wrapper(void);
void MenuSys_GoBack_Wrapper(void);
void OledUtils_RenderMotorTest_Wrapper(void);
void OledUtils_RenderRadar_Wrapper(void);
void OledUtils_RenderValoresIR_Wrapper(void);
void OledUtils_RenderValoresMPU_Wrapper(void);
void onRenderComplete(void);

#endif /* INC_SCREENWRAPPERS_H_ */
