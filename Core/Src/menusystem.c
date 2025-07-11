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


void MenuSys_Init(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;
    ms->currentMenu->currentItemIndex = 0;
    ms->currentMenu->lastSelectedItemIndex = 0;
    ms->currentMenu->firstVisibleItem = 0;
    ms->renderFlag = false;
    ms->allowPeriodicRefresh = false;
}

void MenuSys_SetCallbacks(MenuSystem *ms,
                          ClearFunction    clear,
                          DrawItemFunction draw,
                          RenderFunction   render,
                          volatile uint8_t *insideFlag)
{
    if (!ms) return;
    ms->clearScreen    = clear;
    ms->drawItem       = draw;
    ms->renderFn       = render;
    ms->insideMenuFlag = insideFlag;
}


/**
 * @brief  Mueve el cursor hacia arriba (si puede), ajusta el “bucket” de 3 ítems
 *         y no levanta renderFlag (lo hace el llamador).
 */

void MenuSys_MoveCursorUp(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;
    SubMenu *m = ms->currentMenu;

    if (m->currentItemIndex > 0) {
        m->lastSelectedItemIndex = m->currentItemIndex;
        m->lastVisibleItem       = m->firstVisibleItem;   // ← guardo bucket viejo

        m->currentItemIndex--;
        m->firstVisibleItem =
            (m->currentItemIndex / MENU_VISIBLE_ITEMS)
            * MENU_VISIBLE_ITEMS;
    }
}

void MenuSys_MoveCursorDown(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;
    SubMenu *m = ms->currentMenu;

    if (m->currentItemIndex + 1 < m->itemCount) {
        m->lastSelectedItemIndex = m->currentItemIndex;
        m->lastVisibleItem       = m->firstVisibleItem;   // ← guardo bucket viejo

        m->currentItemIndex++;
        m->firstVisibleItem =
            (m->currentItemIndex / MENU_VISIBLE_ITEMS)
            * MENU_VISIBLE_ITEMS;
    }
}

/**
 * @brief  abre un submenú, reinicia índices y levanta renderFlag.
 */
void MenuSys_OpenSubMenu(MenuSystem *ms, SubMenu *submenu) {
    if (!ms || !submenu) return;
    // Si abrimos el menú principal, marcamos insideFlag...
    if (submenu == &mainMenu && ms->insideMenuFlag) {
        *(ms->insideMenuFlag) = true;
    }
    ms->currentMenu            = submenu;
    submenu->currentItemIndex  = 0;
    submenu->firstVisibleItem  = 0;
    ms->renderFlag             = true;
}


/**
 * @brief  sube un nivel o sale al main, y levanta renderFlag.
 */
void MenuSys_NavigateBack(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;
    SubMenu *parent = ms->currentMenu->parent;
    if (parent) {
        MenuSys_OpenSubMenu(ms, parent);
    } else {
        // salimos del menú
        if (ms->insideMenuFlag) *(ms->insideMenuFlag) = false;
        // reabrimos el main
        MenuSys_OpenSubMenu(ms, &mainMenu);
    }
}

/**
 * @brief  va directamente al menú principal, levanta renderFlag.
 */
void MenuSys_NavigateToMain(MenuSystem *ms) {
    MenuSys_OpenSubMenu(ms, &mainMenu);
}

/**
 * @brief  renderiza el menú actual usando los callbacks.
 */
void MenuSys_RenderMenu(MenuSystem *ms) {
    // Validaciones iniciales
    if (!ms ||
        !ms->currentMenu ||
        !ms->clearScreen ||
        !ms->drawItem ||
        !ms->renderFn)
    {
        return;
    }

    // 1) Limpiar pantalla
	ms->clearScreen();

    // 2) Determinar rango de ítems a dibujar
    SubMenu *m    = ms->currentMenu;
    uint8_t first = m->firstVisibleItem;
    uint8_t last  = first + MENU_VISIBLE_ITEMS;
    if (last > m->itemCount) last = m->itemCount;

    // 3) Dibujar cada ítem visible
    for (uint8_t i = first; i < last; i++) {
        uint8_t local    = i - first;                             // 0..(MENU_VISIBLE_ITEMS-1)
        uint8_t     y        = MENU_ITEM_Y0 + local * MENU_ITEM_SPACING;
        bool    selected = (i == m->currentItemIndex);
        const MenuItem *it = &m->items[i];

        ms->drawItem(it, y, selected);
    }
}

/**
 * @brief  maneja “select”: abre submenú o llama action, luego re-renderiza menú.
 */
void MenuSys_HandleClick(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;
    SubMenu *m = ms->currentMenu;
    uint8_t idx = m->currentItemIndex;
    if (idx >= m->itemCount) return;

    MenuItem *it = &m->items[idx];
    if (it->submenu) {
        MenuSys_OpenSubMenu(ms, it->submenu);
    }
    else if (it->action) {
        it->action();
    }

    // re-render
    MenuSys_RenderMenu(ms);
}

