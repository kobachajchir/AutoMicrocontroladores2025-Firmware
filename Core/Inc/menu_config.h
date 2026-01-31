#ifndef MENU_CONFIG_H
#define MENU_CONFIG_H

#include "menusystem.h"

extern MenuSystem menuSystem;

extern SubMenu mainMenu;
extern SubMenu submenu1;
extern SubMenu submenu2;
extern SubMenu submenu3;
extern SubMenu submenuESP;

extern MenuItem mainMenuItems[];
extern MenuItem submenu1Items[];
extern MenuItem submenu2Items[];
extern MenuItem submenu3Items[];
extern MenuItem submenuESPItems[];

void OLED_Is_Ready(void);
void OledUtils_Clear_Wrapper(void);

#endif // MENU_CONFIG_H
