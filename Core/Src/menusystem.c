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
#include "uner_app.h"

static bool MenuSys_HasNavigableSelection(const MenuSystem *ms);
static bool MenuSys_IsCurrentScreenMenu(const MenuSystem *ms);
static bool MenuSys_GetVisibleSelection(const SubMenu *menu,
                                        uint8_t *selected_index,
                                        uint8_t *item_count);
static void MenuSys_UpdateSelectionFromCurrentMenu(MenuSystem *ms, ScreenReportSource_t source);

bool MenuSys_IsItemVisible(const MenuItem *item) {
    if (!item) return false;
    if (!item->visibilityFn) return true;
    return item->visibilityFn();
}

void MenuSys_FlushScreenReport(MenuSystem *ms)
{
    if (!ms || !ms->screen_report_pending || ms->current_screen_code == SCREEN_CODE_NONE) {
        return;
    }

    if (UNER_App_ReportScreenChanged((uint32_t)ms->current_screen_code,
                                     ms->current_screen_source) == UNER_OK) {
        ms->last_reported_screen_code = ms->current_screen_code;
        ms->screen_report_pending = false;
    }
}

void MenuSys_FlushMenuSelectionReport(MenuSystem *ms)
{
    if (!ms || ms->screen_report_pending || !ms->selection_report_pending ||
        !MenuSys_HasNavigableSelection(ms)) {
        return;
    }

    if (UNER_App_ReportMenuSelectionChanged((uint32_t)ms->current_screen_code,
                                            ms->current_selected_index,
                                            ms->current_item_count,
                                            ms->current_selection_source) == UNER_OK) {
        ms->last_reported_selected_index = ms->current_selected_index;
        ms->last_reported_item_count = ms->current_item_count;
        ms->selection_report_pending = false;
    }
}

void MenuSys_SetCurrentMenuSelection(MenuSystem *ms,
                                     uint8_t selected_index,
                                     uint8_t item_count,
                                     ScreenReportSource_t source)
{
    if (!ms || item_count == 0u || selected_index >= item_count ||
        ms->current_screen_code == SCREEN_CODE_NONE) {
        return;
    }

    if (ms->current_selected_index != selected_index ||
        ms->current_item_count != item_count ||
        ms->current_selection_source != (uint8_t)source ||
        ms->last_reported_selected_index != selected_index ||
        ms->last_reported_item_count != item_count) {
        ms->current_selected_index = selected_index;
        ms->current_item_count = item_count;
        ms->current_selection_source = (uint8_t)source;
        ms->selection_report_pending = true;
    }

    MenuSys_FlushMenuSelectionReport(ms);
}

void MenuSys_ReportCurrentMenuSelection(MenuSystem *ms, ScreenReportSource_t source)
{
    MenuSys_UpdateSelectionFromCurrentMenu(ms, source);
}

void MenuSys_SetCurrentScreenCode(MenuSystem *ms, ScreenCode_t screen_code, ScreenReportSource_t source)
{
    if (!ms || screen_code == SCREEN_CODE_NONE) {
        return;
    }

    if (ms->current_screen_code != screen_code ||
        ms->current_screen_source != (uint8_t)source ||
        ms->last_reported_screen_code != screen_code) {
        ms->current_screen_code = screen_code;
        ms->current_screen_source = (uint8_t)source;
        ms->screen_report_pending = true;
    }

    MenuSys_FlushScreenReport(ms);
    if (MenuSys_IsCurrentScreenMenu(ms)) {
        MenuSys_UpdateSelectionFromCurrentMenu(ms, source);
    } else {
        MenuSys_ClearCurrentMenuSelection(ms, source);
    }
}

void MenuSys_SetCurrentMenuScreenCode(MenuSystem *ms)
{
    if (!ms || !ms->currentMenu) {
        return;
    }

    MenuSys_SetCurrentScreenCode(ms,
                                 ms->currentMenu->screen_code,
                                 SCREEN_REPORT_SOURCE_MENU);
}

void MenuSys_ClearCurrentMenuSelection(MenuSystem *ms, ScreenReportSource_t source)
{
    if (ms == NULL) {
        return;
    }

    if (ms->current_selected_index != MENU_SELECTION_INVALID_INDEX ||
        ms->current_item_count != 0u ||
        ms->last_reported_selected_index != MENU_SELECTION_INVALID_INDEX ||
        ms->last_reported_item_count != 0u ||
        ms->current_selection_source != (uint8_t)source) {
        ms->current_selected_index = MENU_SELECTION_INVALID_INDEX;
        ms->current_item_count = 0u;
        ms->last_reported_selected_index = MENU_SELECTION_INVALID_INDEX;
        ms->last_reported_item_count = 0u;
        ms->current_selection_source = (uint8_t)source;
        ms->selection_report_pending = false;
    }
}

static bool MenuSys_HasNavigableSelection(const MenuSystem *ms)
{
    return (ms != NULL) &&
           (ms->current_screen_code != SCREEN_CODE_NONE) &&
           (ms->current_item_count > 0u) &&
           (ms->current_selected_index < ms->current_item_count);
}

static bool MenuSys_IsCurrentScreenMenu(const MenuSystem *ms)
{
    return (ms != NULL) &&
           (ms->currentMenu != NULL) &&
           (ms->current_screen_code == ms->currentMenu->screen_code);
}

static bool MenuSys_GetVisibleSelection(const SubMenu *menu,
                                        uint8_t *selected_index,
                                        uint8_t *item_count)
{
    uint8_t visible_count = 0u;
    uint8_t visible_selected = MENU_SELECTION_INVALID_INDEX;

    if (!menu || !selected_index || !item_count) {
        return false;
    }

    for (int8_t i = 0; i < menu->itemCount; i++) {
        if (MenuSys_IsItemVisible(&menu->items[i])) {
            if (i == menu->currentItemIndex) {
                visible_selected = visible_count;
            }
            visible_count++;
        }
    }

    if (visible_count == 0u || visible_selected == MENU_SELECTION_INVALID_INDEX) {
        return false;
    }

    *selected_index = visible_selected;
    *item_count = visible_count;
    return true;
}

static void MenuSys_UpdateSelectionFromCurrentMenu(MenuSystem *ms, ScreenReportSource_t source)
{
    uint8_t selected_index;
    uint8_t item_count;

    if (ms == NULL || ms->currentMenu == NULL) {
        MenuSys_ClearCurrentMenuSelection(ms, source);
        return;
    }

    if (ms->currentMenu->itemCount == 0u) {
        MenuSys_ClearCurrentMenuSelection(ms, source);
        return;
    }

    if (ms->currentMenu->currentItemIndex >= ms->currentMenu->itemCount) {
        MenuSys_ClearCurrentMenuSelection(ms, source);
        return;
    }

    if (!MenuSys_GetVisibleSelection(ms->currentMenu, &selected_index, &item_count)) {
        MenuSys_ClearCurrentMenuSelection(ms, source);
        return;
    }

    MenuSys_SetCurrentMenuSelection(ms,
                                    selected_index,
                                    item_count,
                                    source);
}

ScreenCode_t MenuSys_GetCurrentScreenCode(const MenuSystem *ms)
{
    return ms ? ms->current_screen_code : SCREEN_CODE_NONE;
}

uint8_t MenuSys_GetCurrentScreenSource(const MenuSystem *ms)
{
    return ms ? ms->current_screen_source : SCREEN_REPORT_SOURCE_UNKNOWN;
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
    if (ms->current_screen_code == SCREEN_CODE_NONE) {
        ms->current_screen_code = SCREEN_CODE_CORE_STARTUP;
    }
    ms->last_reported_screen_code = SCREEN_CODE_NONE;
    ms->current_screen_source = SCREEN_REPORT_SOURCE_SYSTEM;
    ms->screen_report_pending = false;
    ms->current_selected_index = MENU_SELECTION_INVALID_INDEX;
    ms->last_reported_selected_index = MENU_SELECTION_INVALID_INDEX;
    ms->current_item_count = 0u;
    ms->last_reported_item_count = 0u;
    ms->current_selection_source = SCREEN_REPORT_SOURCE_UNKNOWN;
    ms->selection_report_pending = false;
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

    int8_t nextIndex;

    nextIndex = MenuSys_FindNextVisibleIndex(m, m->currentItemIndex, -1);
    if (nextIndex < 0) return;
    if (nextIndex == m->currentItemIndex) return;

    m->lastSelectedItemIndex = m->currentItemIndex;
    m->lastVisibleItem       = m->firstVisibleItem;   // ← guardo bucket viejo
    m->currentItemIndex      = nextIndex;

    {
        int8_t firstVisible = MenuSys_FindFirstVisibleIndexFrom(m, m->firstVisibleItem);
        if (firstVisible < 0) return;
        m->firstVisibleItem = firstVisible;
    }

    while (m->currentItemIndex < m->firstVisibleItem) {
        int8_t prevFirst = MenuSys_FindLastVisibleIndexBefore(m, m->firstVisibleItem - 1);
        if (prevFirst < 0) break;
        m->firstVisibleItem = prevFirst;
    }

    MenuSys_ReportCurrentMenuSelection(ms, SCREEN_REPORT_SOURCE_MENU);
}

void MenuSys_MoveCursorDown(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;
    SubMenu *m = ms->currentMenu;

    int8_t nextIndex;

    nextIndex = MenuSys_FindNextVisibleIndex(m, m->currentItemIndex, 1);
    if (nextIndex < 0) return;
    if (nextIndex == m->currentItemIndex) return;

    m->lastSelectedItemIndex = m->currentItemIndex;
    m->lastVisibleItem       = m->firstVisibleItem;   // ← guardo bucket viejo
    m->currentItemIndex      = nextIndex;

    {
        int8_t firstVisible = MenuSys_FindFirstVisibleIndexFrom(m, m->firstVisibleItem);
        if (firstVisible < 0) return;
        m->firstVisibleItem = firstVisible;
    }

    {
        int8_t lastVisible = MenuSys_ComputeLastVisibleIndex(m, m->firstVisibleItem);
        while (lastVisible >= 0 && m->currentItemIndex > lastVisible) {
            int8_t nextFirst = MenuSys_FindFirstVisibleIndexFrom(m, m->firstVisibleItem + 1);
            if (nextFirst < 0) break;
            m->firstVisibleItem = nextFirst;
            lastVisible = MenuSys_ComputeLastVisibleIndex(m, m->firstVisibleItem);
        }
    }

    MenuSys_ReportCurrentMenuSelection(ms, SCREEN_REPORT_SOURCE_MENU);
}

/**
 * @brief  abre un submenú, reinicia índices y levanta renderFlag.
 */
void MenuSys_OpenSubMenu(MenuSystem *ms, SubMenu *submenu) {
    if (!ms || !submenu) return;
    // Al abrir cualquier menú, activamos el contexto de navegación de menú.
    if (ms->insideMenuFlag) {
        *(ms->insideMenuFlag) = true;
    }
    ms->currentMenu            = submenu;
    encoder_fast_scroll_enabled = 0;
    int8_t firstVisible = MenuSys_FindFirstVisibleIndexFrom(submenu, 0);
    if (firstVisible < 0) return;
    submenu->currentItemIndex  = firstVisible;
    submenu->firstVisibleItem  = firstVisible;
    submenu->lastSelectedItemIndex = -1; //Asi renderiza todo nuevamente
    submenu->lastVisibleItem = -1; //Asi renderiza todo nuevamente
    ms->renderFlag             = true;
    MenuSys_SetCurrentScreenCode(ms, submenu->screen_code, SCREEN_REPORT_SOURCE_MENU);
}

void MenuSys_ResetMenu(MenuSystem *ms) {
	if (!ms) return;
	ms->currentMenu            = &mainMenu;
	encoder_fast_scroll_enabled = 0;
	int8_t firstVisible = MenuSys_FindFirstVisibleIndexFrom(ms->currentMenu, 0);
	if (firstVisible < 0) return;
	ms->currentMenu->currentItemIndex  = firstVisible;
	ms->currentMenu->firstVisibleItem  = firstVisible;
	ms->currentMenu->lastSelectedItemIndex = -1; //Asi renderiza todo nuevamente
	ms->currentMenu->lastVisibleItem = -1; //Asi renderiza todo nuevamente
	MenuSys_ClearCurrentMenuSelection(ms, SCREEN_REPORT_SOURCE_MENU);
}

/**
 * @brief  sube un nivel o sale al main, y levanta renderFlag.
 */
void MenuSys_NavigateBack(MenuSystem *ms) {
    if (!ms || !ms->currentMenu) return;

    encoder_fast_scroll_enabled = 0;

    SubMenu *parent = ms->currentMenu->parent;
    if (parent) {
        ms->currentMenu = parent;

        if (!MenuSys_IsItemVisible(&parent->items[parent->currentItemIndex])) {
            int8_t nextVisible = MenuSys_FindFirstVisibleIndexFrom(parent, parent->currentItemIndex);
            if (nextVisible < 0) {
                nextVisible = MenuSys_FindLastVisibleIndexBefore(parent, parent->currentItemIndex);
            }
            if (nextVisible >= 0) {
                parent->currentItemIndex = nextVisible;
            }
        }

        int8_t firstVisible = parent->firstVisibleItem;
        if (firstVisible < 0 || firstVisible >= parent->itemCount ||
            !MenuSys_IsItemVisible(&parent->items[firstVisible])) {
            firstVisible = MenuSys_FindFirstVisibleIndexFrom(parent, 0);
        }

        while (firstVisible >= 0 && parent->currentItemIndex < firstVisible) {
            int8_t prevFirst = MenuSys_FindLastVisibleIndexBefore(parent, firstVisible - 1);
            if (prevFirst < 0) break;
            firstVisible = prevFirst;
        }

        int8_t lastVisible = MenuSys_ComputeLastVisibleIndex(parent, firstVisible);
        while (firstVisible >= 0 && lastVisible >= 0 && parent->currentItemIndex > lastVisible) {
            int8_t nextFirst = MenuSys_FindFirstVisibleIndexFrom(parent, firstVisible + 1);
            if (nextFirst < 0) break;
            firstVisible = nextFirst;
            lastVisible = MenuSys_ComputeLastVisibleIndex(parent, firstVisible);
        }

        if (firstVisible >= 0) {
            parent->firstVisibleItem = firstVisible;
        }

        parent->lastSelectedItemIndex = -1;
        parent->lastVisibleItem = -1;
        ms->renderFlag = true;
        return;
    }

    // en el Main: vamos al dashboard
    if (ms->dashboardRender) {
        ms->dashboardRender();
    }
    // salimos del menú
    if (ms->insideMenuFlag) *ms->insideMenuFlag = false;
}

/**
 * @brief  va directamente al menú principal, levanta renderFlag.
 */
void MenuSys_NavigateToMain(MenuSystem *ms) {
    encoder_fast_scroll_enabled = 0;
    MenuSys_OpenSubMenu(ms, &mainMenu);
}

/**
 * @brief  Renderiza el menú actual usando los callbacks, borrando pantalla al cambiar de página.
 */
void MenuSys_RenderMenu(MenuSystem *ms) {
    if (!ms || !ms->currentMenu || !ms->clearScreen || !ms->drawItem)
        return;

    SubMenu *m = ms->currentMenu;
    MenuSys_SetCurrentMenuScreenCode(ms);

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
