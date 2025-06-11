/*
 * menusystem.c
 *
 *  Created on: Jun 11, 2025
 *      Author: kobac
 */

#include "types/menu_types.h"
#include "menusystem.h"

#ifndef MAX_VISIBLE_ITEMS
#define MAX_VISIBLE_ITEMS 2  ///< Cantidad máxima visible por pantalla (puede redefinirse)
#endif

void initMenuSystem(MenuSystem *system) {
    system->currentMenu = &mainMenu;
    system->currentMenu->currentItemIndex = 0;
    system->currentMenu->firstVisibleItem = 0;
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

void volver() {
    if (menuSystem.currentMenu == &mainMenu) {
        // En el menú principal podrías mostrar otra pantalla (dashboard)
        return;
    } else if (menuSystem.currentMenu->parent) {
        menuSystem.currentMenu = menuSystem.currentMenu->parent;
        displayMenu(&menuSystem);
    }
}

void selectMenuItem(MenuSystem *system, int index) {
    if (index >= 0 && index < system->currentMenu->itemCount) {
        system->currentMenu->items[index].action();
    }
}

void navigateToMainMenu() {
    menuSystem.currentMenu = &mainMenu;
    menuSystem.currentMenu->currentItemIndex = 0;
    menuSystem.currentMenu->firstVisibleItem = 0;
    displayMenu(&menuSystem);
}

void navigateBackInMenu() {
    if (menuSystem.currentMenu->parent != NULL) {
        menuSystem.currentMenu = menuSystem.currentMenu->parent;
        displayMenu(&menuSystem);
    } else {
        INSIDE_MENU = 0;
        menuSystem.currentMenu->currentItemIndex = 0;
        if (menuSystem.clearScreen) menuSystem.clearScreen();
        if (menuSystem.renderFn) menuSystem.renderFn();
    }
}

void submenu1Fn() {
    menuSystem.currentMenu = &submenu1;
    displayMenu(&menuSystem);
}

void submenu2Fn() {
    menuSystem.currentMenu = &submenu2;
    displayMenu(&menuSystem);
}

void submenu3Fn() {
    menuSystem.currentMenu = &submenu3;
    displayMenu(&menuSystem);
}
