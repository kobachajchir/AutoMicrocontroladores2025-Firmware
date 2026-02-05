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

bool MenuSys_IsItemVisible(const MenuItem *item) {
    if (!item) return false;
    if (!item->visibilityFn) return true;
    return item->visibilityFn();
}

static int8_t MenuSys_FindFirstVisibleIndexFrom(const SubMenu *menu, int8_t start) {
    if (!menu) return -1;
    if (start < 0) start = 0;
    for (int8_t i = start; i < menu->itemCount; i++) {
        if (MenuSys_IsItemVisible(&menu->items[i])) {
            return i;
        }
    }
    return -1;
}

static int8_t MenuSys_FindLastVisibleIndexBefore(const SubMenu *menu, int8_t start) {
    if (!menu) return -1;
    if (start >= menu->itemCount) start = menu->itemCount - 1;
    for (int8_t i = start; i >= 0; i--) {
        if (MenuSys_IsItemVisible(&menu->items[i])) {
            return i;
        }
    }
    return -1;
}

static int8_t MenuSys_FindNextVisibleIndex(const SubMenu *menu, int8_t start, int8_t direction) {
    if (!menu || (direction != 1 && direction != -1)) return -1;
    int8_t i = start + direction;
    while (i >= 0 && i < menu->itemCount) {
        if (MenuSys_IsItemVisible(&menu->items[i])) {
            return i;
        }
        i += direction;
    }
    return -1;
}

static int8_t MenuSys_ComputeLastVisibleIndex(const SubMenu *menu, int8_t firstVisibleIndex) {
    if (!menu) return -1;
    if (firstVisibleIndex < 0) return -1;
    int8_t lastVisible = -1;
    uint8_t count = 0;
    for (int8_t i = firstVisibleIndex; i < menu->itemCount; i++) {
        if (MenuSys_IsItemVisible(&menu->items[i])) {
            lastVisible = i;
            count++;
            if (count >= MENU_VISIBLE_ITEMS) {
                break;
            }
        }
    }
    return lastVisible;
}

void MenuSys_Init(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;
    int8_t firstVisible = MenuSys_FindFirstVisibleIndexFrom(ms->currentMenu, 0);
    if (firstVisible < 0) {
        return;
    }
    ms->currentMenu->currentItemIndex = firstVisible;
    ms->currentMenu->lastSelectedItemIndex = firstVisible;
    ms->currentMenu->firstVisibleItem = firstVisible;
    ms->renderFlag = false;
    ms->allowPeriodicRefresh = false;
    ms->userEventManagerFn = NULL;
}

void MenuSys_SetCallbacks(MenuSystem *ms,
                          ClearFunction    clear,
                          DrawItemFunction draw,
                          RenderFunction   render,
                          volatile uint8_t *insideFlag,
						  RenderFunction dashboardRender)
{
    if (!ms) return;
    ms->clearScreen    = clear;
    ms->drawItem       = draw;
    ms->renderFn       = render;
    ms->dashboardRender = dashboardRender;
    ms->insideMenuFlag = insideFlag;
}


/**
 * @brief  Mueve el cursor hacia arriba (si puede), ajusta el “bucket” de 3 ítems
 *         y no levanta renderFlag (lo hace el llamador).
 */

void MenuSys_MoveCursorUp(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;
    SubMenu *m = ms->currentMenu;

    int8_t prevVisible = MenuSys_FindNextVisibleIndex(m, m->currentItemIndex, -1);
    if (prevVisible < 0) return;

    m->lastSelectedItemIndex = m->currentItemIndex;
    m->lastVisibleItem       = m->firstVisibleItem;   // ← guardo bucket viejo
    m->currentItemIndex      = prevVisible;

    int8_t firstVisible = MenuSys_FindFirstVisibleIndexFrom(m, m->firstVisibleItem);
    if (firstVisible < 0) return;
    m->firstVisibleItem = firstVisible;

    while (m->currentItemIndex < m->firstVisibleItem) {
        int8_t prevFirst = MenuSys_FindLastVisibleIndexBefore(m, m->firstVisibleItem - 1);
        if (prevFirst < 0) break;
        m->firstVisibleItem = prevFirst;
    }
}

void MenuSys_MoveCursorDown(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;
    SubMenu *m = ms->currentMenu;

    int8_t nextVisible = MenuSys_FindNextVisibleIndex(m, m->currentItemIndex, 1);
    if (nextVisible < 0) return;

    m->lastSelectedItemIndex = m->currentItemIndex;
    m->lastVisibleItem       = m->firstVisibleItem;   // ← guardo bucket viejo
    m->currentItemIndex      = nextVisible;

    int8_t firstVisible = MenuSys_FindFirstVisibleIndexFrom(m, m->firstVisibleItem);
    if (firstVisible < 0) return;
    m->firstVisibleItem = firstVisible;

    int8_t lastVisible = MenuSys_ComputeLastVisibleIndex(m, m->firstVisibleItem);
    while (lastVisible >= 0 && m->currentItemIndex > lastVisible) {
        int8_t nextFirst = MenuSys_FindFirstVisibleIndexFrom(m, m->firstVisibleItem + 1);
        if (nextFirst < 0) break;
        m->firstVisibleItem = nextFirst;
        lastVisible = MenuSys_ComputeLastVisibleIndex(m, m->firstVisibleItem);
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
    int8_t firstVisible = MenuSys_FindFirstVisibleIndexFrom(submenu, 0);
    if (firstVisible < 0) return;
    submenu->currentItemIndex  = firstVisible;
    submenu->firstVisibleItem  = firstVisible;
    submenu->lastSelectedItemIndex = -1; //Asi renderiza todo nuevamente
    submenu->lastVisibleItem = -1; //Asi renderiza todo nuevamente
    ms->renderFlag             = true;
}

void MenuSys_ResetMenu(MenuSystem *ms) {
	if (!ms) return;
	ms->currentMenu            = &mainMenu;
	int8_t firstVisible = MenuSys_FindFirstVisibleIndexFrom(ms->currentMenu, 0);
	if (firstVisible < 0) return;
	ms->currentMenu->currentItemIndex  = firstVisible;
	ms->currentMenu->firstVisibleItem  = firstVisible;
	ms->currentMenu->lastSelectedItemIndex = -1; //Asi renderiza todo nuevamente
	ms->currentMenu->lastVisibleItem = -1; //Asi renderiza todo nuevamente
}

/**
 * @brief  sube un nivel o sale al main, y levanta renderFlag.
 */
void MenuSys_NavigateBack(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;

    SubMenu *parent = ms->currentMenu->parent;
    if (parent) {
        // si hay padre, subimos
        MenuSys_OpenSubMenu(ms, parent);
        MenuSys_RenderMenu(ms);
    }
    else {
        // en el Main: vamos al dashboard
        ms->dashboardRender();
        // salimos del menú
        if (ms->insideMenuFlag) *ms->insideMenuFlag = false;
    }
}


/**
 * @brief  va directamente al menú principal, levanta renderFlag.
 */
void MenuSys_NavigateToMain(MenuSystem *ms) {
    MenuSys_OpenSubMenu(ms, &mainMenu);
}

/**
 * @brief  Renderiza el menú actual usando los callbacks, borrando pantalla al cambiar de página.
 */
void MenuSys_RenderMenu(MenuSystem *ms) {
    if (!ms || !ms->currentMenu || !ms->clearScreen || !ms->drawItem)
        return;

    SubMenu *m = ms->currentMenu;
    int8_t firstVisible = MenuSys_FindFirstVisibleIndexFrom(m, m->firstVisibleItem);
    if (firstVisible < 0) return;

    if (!MenuSys_IsItemVisible(&m->items[m->currentItemIndex])) {
        int8_t nextVisible = MenuSys_FindFirstVisibleIndexFrom(m, m->currentItemIndex);
        if (nextVisible < 0) {
            nextVisible = MenuSys_FindLastVisibleIndexBefore(m, m->currentItemIndex);
        }
        if (nextVisible >= 0) {
            m->currentItemIndex = nextVisible;
        }
    }

    m->firstVisibleItem = firstVisible;

    // 1) Limpiar toda la pantalla
    ms->clearScreen();

    // 2) Rango de ítems a mostrar
    int8_t drawIndex = firstVisible;
    uint8_t drawn = 0;

    // 3) Dibujar cada uno
    while (drawIndex >= 0 && drawIndex < m->itemCount && drawn < MENU_VISIBLE_ITEMS) {
        uint8_t y        = MENU_ITEM_Y0 + drawn * MENU_ITEM_SPACING;
        bool    selected = (drawIndex == m->currentItemIndex);
        ms->drawItem(&m->items[drawIndex], y, selected);
        drawn++;
        drawIndex = MenuSys_FindNextVisibleIndex(m, drawIndex, 1);
    }

    // 4) Actualizo el estado para el wrapper
    m->lastVisibleItem       = m->firstVisibleItem;
    m->lastSelectedItemIndex = m->currentItemIndex;
}

/**
 * @brief  maneja “select”: abre submenú o llama action, luego re-renderiza menú.
 */
void MenuSys_HandleClick(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;

    SubMenu  *m   = ms->currentMenu;
    int8_t    idx = m->currentItemIndex;
    if (idx >= m->itemCount) return;

    if (!MenuSys_IsItemVisible(&m->items[idx])) {
        int8_t nextVisible = MenuSys_FindFirstVisibleIndexFrom(m, idx);
        if (nextVisible < 0) {
            nextVisible = MenuSys_FindLastVisibleIndexBefore(m, idx);
        }
        if (nextVisible < 0) {
            return;
        }
        m->currentItemIndex = nextVisible;
        idx = nextVisible;
    }

    MenuItem *it = &m->items[idx];

    // 1) Navegación o acción
    if (it->submenu) {
        MenuSys_OpenSubMenu(ms, it->submenu);
    }
    else if (it->action) {
        it->action(ms);
    }

    // 2) Decidir qué renderFn usar
    if (it->screenRenderFn) {
        // pantalla “terminal” asociada al ítem
        ms->renderFn = it->screenRenderFn;
        m->lastSelectedItemIndex = -1;
        m->lastVisibleItem = -1;
    }

    // 3) Asignar callbacks del manager si aplica
    if (it->eventManagerFn) {
        ms->userEventManagerFn = it->eventManagerFn;
    }

    // 4) Limpiar y encender el flag para que OLED_MainTask lo pinte
    if (ms->clearScreen) ms->clearScreen();
    ms->renderFlag = true;
}
