/*
 * menusystem.c
 *
 *  Created on: Jun 11, 2025
 *      Author: kobac
 */

#include "types/menu_types.h"
#include "globals.h"
#include "menusystem.h"

#ifndef MAX_VISIBLE_ITEMS
#define MAX_VISIBLE_ITEMS 2  ///< Cantidad máxima visible por pantalla (puede redefinirse)
#endif

void initMenuSystem(MenuSystem *system) {
    if (!system || !system->currentMenu) return;

    SubMenu *menu = system->currentMenu;
    menu->currentItemIndex = 0;
    menu->firstVisibleItem = 0;
}

void MenuSystem_SetCallbacks(MenuSystem *system,
                             ClearFunction clear,
                             DrawItemFunction draw,
                             RenderFunction render,
                             volatile uint8_t *insideFlag) {
    system->clearScreen = clear;
    system->drawItem = draw;
    system->renderFn = render;
    system->insideMenuFlag = insideFlag;
}

void moveCursorUp(MenuSystem *system) {
    SubMenu *menu = system->currentMenu;
    if (menu->currentItemIndex > 0) {
        menu->currentItemIndex--;
        if (menu->currentItemIndex < menu->firstVisibleItem) {
            menu->firstVisibleItem = menu->currentItemIndex;
        }
    }
}

void moveCursorDown(MenuSystem *system) {
    SubMenu *menu = system->currentMenu;
    if (menu->currentItemIndex + 1 < menu->itemCount) {
        menu->currentItemIndex++;
        if (menu->currentItemIndex >= menu->firstVisibleItem + MAX_VISIBLE_ITEMS) {
            menu->firstVisibleItem = menu->currentItemIndex - (MAX_VISIBLE_ITEMS - 1);
        }
    }
}

void selectCurrentItem(MenuSystem *system) {
    SubMenu *menu = system->currentMenu;
    MenuItem *item = &menu->items[menu->currentItemIndex];
    if (item->action) {
        item->action();
    }
}

void displayMenu(MenuSystem *system) {
    SubMenu *menu = system->currentMenu;
    if (system->clearScreen) system->clearScreen();

    for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
        int itemIndex = menu->firstVisibleItem + i;
        if (itemIndex >= menu->itemCount) break;

        MenuItem *item = &menu->items[itemIndex];
        int y = 12 + i * 12;

        if (system->drawItem) {
            bool selected = (itemIndex == menu->currentItemIndex);
            system->drawItem(item->name, y, selected);
        }
    }

    if (system->renderFn) system->renderFn();
}

void volver(MenuSystem *system) {
    if (!system) return;
    if (system->currentMenu == &mainMenu) {
        return;
    } else if (system->currentMenu->parent) {
        system->currentMenu = system->currentMenu->parent;
        displayMenu(system);
    }
}

void selectMenuItem(MenuSystem *system, int index) {
    if (index >= 0 && index < system->currentMenu->itemCount) {
        system->currentMenu->items[index].action();
    }
}

void navigateToMainMenu(MenuSystem *system) {
    if (!system) return;
    system->currentMenu = &mainMenu;
    system->currentMenu->currentItemIndex = 0;
    system->currentMenu->firstVisibleItem = 0;
    displayMenu(system);
    if (system->insideMenuFlag) *(system->insideMenuFlag) = 1;
}

void navigateBackInMenu(MenuSystem *system) {
    if (!system) return;
    if (system->currentMenu->parent != NULL) {
        system->currentMenu = system->currentMenu->parent;
        displayMenu(system);
    } else {
        if (system->insideMenuFlag) *(system->insideMenuFlag) = 0;
        system->currentMenu->currentItemIndex = 0;
        if (system->clearScreen) system->clearScreen();
        if (system->renderFn) system->renderFn();
    }
}

void submenuFn(MenuSystem *system, SubMenu *submenu) {
    system->currentMenu = submenu;
    displayMenu(system);
}

