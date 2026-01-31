/*
 * menu_definitions.h
 *
 *  Created on: 31 ene 2026
 *      Author: kobac
 */

#ifndef INC_MENU_DEFINITIONS_H_
#define INC_MENU_DEFINITIONS_H_

#include "menusystem.h"

/* Menus */
extern MenuSystem menuSystem;

extern SubMenu mainMenu;
extern SubMenu submenu1;
extern SubMenu submenu2;
extern SubMenu submenu3;
extern SubMenu submenuESP;

/* Items */
extern MenuItem mainMenuItems[];
extern MenuItem submenu1Items[];
extern MenuItem submenu2Items[];
extern MenuItem submenu3Items[];
extern MenuItem submenuESPItems[];

bool MenuItem_IsTestModeVisible(void);

#endif /* INC_MENU_DEFINITIONS_H_ */
