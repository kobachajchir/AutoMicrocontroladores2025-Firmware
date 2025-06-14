/*
 * menusystem.c
 *
 *  Created on: Jun 11, 2025
 *      Author: kobac
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "types/menu_types.h"
#include "globals.h"         // USART1_PrintString
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
    system->clearScreen    = clear;
    system->drawItem       = draw;
    system->renderFn       = render;
    system->insideMenuFlag = insideFlag;
}

/**
 * Mueve el cursor al ítem anterior y ajusta scroll si es necesario.
 * Imprime el nombre del ítem actual por UART.
 */
void moveCursorUp(MenuSystem *system) {
    SubMenu *menu = system->currentMenu;
    if (menu->currentItemIndex > 0) {
        menu->currentItemIndex--;
        if (menu->currentItemIndex < menu->firstVisibleItem) {
            menu->firstVisibleItem = menu->currentItemIndex;
        }
    }
}

/**
 * Mueve el cursor al siguiente ítem y ajusta scroll si es necesario.
 * Imprime el nombre del ítem actual por UART.
 */
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
	SubMenu *m    = system->currentMenu;
	MenuItem *it  = &m->items[m->currentItemIndex];

	if (it->submenu) {
		// ¡Es un submenu! Navego a él
		submenuFn(system, it->submenu);
	}
	else if (it->action) {
		// Es un comando “terminal”
		it->action();

	}
}


void displayMenu(MenuSystem *system) {
    SubMenu *menu = system->currentMenu;
    if (system->clearScreen) system->clearScreen();

    /* Descomenta si quieres scroll visible
    for (int i = 0; i < MAX_VISIBLE_ITEMS; i++) {
        int idx = menu->firstVisibleItem + i;
        if (idx >= menu->itemCount) break;
        MenuItem *it = &menu->items[idx];
        int y = 12 + i * 12;
        if (system->drawItem) {
            bool sel = (idx == menu->currentItemIndex);
            system->drawItem(it->name, y, sel);
        }
    }
    */

    if (system->renderFn) system->renderFn();
}

void volver(MenuSystem *system) {
    if (!system) return;
    if (system->currentMenu == &mainMenu) {
        // ya en main, no hacer nada
        return;
    }
    if (system->currentMenu->parent) {
        system->currentMenu = system->currentMenu->parent;
        displayMenu(system);
    }
}

/**
 * Regresa siempre al menú principal y lo muestra desde el primer ítem.
 * Enciende la bandera INSIDE_MENU.
 */
void navigateToMainMenu(MenuSystem *system) {
    if (!system) return;
    system->currentMenu = &mainMenu;
    system->currentMenu->currentItemIndex = 0;
    system->currentMenu->firstVisibleItem   = 0;
    displayMenu(system);
    if (system->insideMenuFlag) {
        *(system->insideMenuFlag) = 1;
    }
}

/**
 * Si hay menú padre, sube a él; si no (estás en main), sale del sistema de menús.
 */
void navigateBackInMenu(MenuSystem *system) {
    if (!system) return;
    if (system->currentMenu->parent != NULL) {
        system->currentMenu = system->currentMenu->parent;
        displayMenu(system);
    } else {
        // Salir del modo menú
        if (system->insideMenuFlag) {
            *(system->insideMenuFlag) = 0;
        }
        system->currentMenu->currentItemIndex = 0;
        if (system->clearScreen) system->clearScreen();
        if (system->renderFn)   system->renderFn();
    }
}

/**
 * Función genérica para abrir un submenú pasado como parámetro.
 */
void submenuFn(MenuSystem *system, SubMenu *submenu) {
    if (!system || !submenu) return;
    system->currentMenu = submenu;
    system->currentMenu->currentItemIndex = 0;
    system->currentMenu->firstVisibleItem   = 0;
    displayMenu(system);
}

