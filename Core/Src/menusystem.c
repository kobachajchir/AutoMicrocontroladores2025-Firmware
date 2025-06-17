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

/**
 * @brief  Reinicia índices y bandera "insideMenu" al cambiar de menú.
 */
static void changeMenu(MenuSystem *system, SubMenu *newMenu) {
    if (!system || !newMenu) return;
    system->currentMenu = newMenu;
    newMenu->currentItemIndex = 0;
    newMenu->firstVisibleItem = 0;
    // Si volvemos al mainMenu, activamos la bandera
    if (newMenu == &mainMenu && system->insideMenuFlag) {
        *(system->insideMenuFlag) = 1;
    }
    if (newMenu == &mainMenu) {
    	system->renderFn   = newMenu->items[newMenu->currentItemIndex].screenRenderFn;
    }else{
    	//Render del nuevo menu
    }
}

/**
 * @brief  Asigna renderFn según el ítem actual y levanta renderFlag.
 */
static void updateRender(MenuSystem *system) {
    if (!system || !system->currentMenu) return;
    SubMenu *menu = system->currentMenu;
    if (!menu->items || menu->itemCount == 0) return;
    uint8_t idx = menu->currentItemIndex;
    if (idx >= menu->itemCount) return;
    MenuItem *it = &menu->items[idx];
    if (it->screenRenderFn) {
        system->renderFn   = it->screenRenderFn;
        system->renderFlag = true;
    }
}

void initMenuSystem(MenuSystem *system) {
    if (!system || !system->currentMenu) return;

    SubMenu *menu = system->currentMenu;
    menu->currentItemIndex = 0;
    menu->firstVisibleItem = 0;
    system->renderFlag = false;
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
 * Mueve el cursor al ítem anterior, ajusta scroll si es necesario
 * y dispara el render de la nueva pantalla asociada.
 */
void moveCursorUp(MenuSystem *system) {
    SubMenu *menu = system->currentMenu;
    if (menu->currentItemIndex > 0) {
        menu->currentItemIndex--;
        if (menu->currentItemIndex < menu->firstVisibleItem) {
            menu->firstVisibleItem = menu->currentItemIndex;
        }
        // Asignar la nueva función de render y activar el flag
        MenuItem *it = &menu->items[menu->currentItemIndex];
        if (it->screenRenderFn) {
            system->renderFn   = it->screenRenderFn;
            system->renderFlag = true;
        }
    }
}

/**
 * Mueve el cursor al siguiente ítem, ajusta scroll si es necesario
 * y dispara el render de la nueva pantalla asociada.
 */
void moveCursorDown(MenuSystem *system) {
    SubMenu *menu = system->currentMenu;
    if (menu->currentItemIndex + 1 < menu->itemCount) {
        menu->currentItemIndex++;
        if (menu->currentItemIndex >= menu->firstVisibleItem + MAX_VISIBLE_ITEMS) {
            menu->firstVisibleItem = menu->currentItemIndex - (MAX_VISIBLE_ITEMS - 1);
        }
        // Asignar la nueva función de render y activar el flag
        MenuItem *it = &menu->items[menu->currentItemIndex];
        if (it->screenRenderFn) {
            system->renderFn   = it->screenRenderFn;
            system->renderFlag = true;
        }
    }
}



/**
 * @brief  Selecciona el ítem actual: navega o ejecuta acción, luego renderiza.
 */
void selectCurrentItem(MenuSystem *system) {
    if (!system || !system->currentMenu) return;
    SubMenu *menu = system->currentMenu;
    if (!menu->items || menu->itemCount == 0) return;
    uint8_t idx = menu->currentItemIndex;
    if (idx >= menu->itemCount) return;
    MenuItem *it = &menu->items[idx];

    if (it->submenu) {
        changeMenu(system, it->submenu);
    } else if (it->action) {
        it->action();
    }
    updateRender(system);
}

/**
 * @brief  Sube un nivel o sale al main, luego renderiza el ítem activo.
 */
void navigateBackInMenu(MenuSystem *system) {
    if (!system || !system->currentMenu) return;
    SubMenu *parent = system->currentMenu->parent;
    if (parent) {
        changeMenu(system, parent);
    } else {
        if (system->insideMenuFlag) {
            *(system->insideMenuFlag) = 0;
        }
        changeMenu(system, &mainMenu);
    }
    updateRender(system);
}

/**
 * @brief  Regresa siempre al menú principal y renderiza su primer ítem.
 */
void navigateToMainMenu(MenuSystem *system) {
    if (!system) return;
    if (system->insideMenuFlag) {
        *(system->insideMenuFlag) = 1;
    }
    changeMenu(system, &mainMenu);
    updateRender(system);
}

/**
 * @brief  Abre un submenú dado y renderiza su primer ítem.
 */
void submenuFn(MenuSystem *system, SubMenu *submenu) {
    if (!system || !submenu) return;
    changeMenu(system, submenu);
    updateRender(system);
}

