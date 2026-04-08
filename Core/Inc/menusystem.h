/*
 * menuSystem.h
 *
 *  Created on: Jun 11, 2025
 *      Author: kobac
 */

#ifndef INC_MENUSYSTEM_H_
#define INC_MENUSYSTEM_H_

#include "types/menu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_VISIBLE_ITEMS
#define MAX_VISIBLE_ITEMS 2  // se puede redefinir externamente si se desea
#endif

/**
 * @brief Inicializa el sistema de menú apuntando al menú actual.
 * @param system Puntero al sistema de menú
 */
void MenuSys_Init(MenuSystem *system);

/**
 * @brief Configura los callbacks del sistema de menú.
 * @param system Puntero al sistema de menú
 * @param clear Función para limpiar pantalla
 * @param draw Función para dibujar cada ítem
 * @param render Función para renderizar la pantalla completa
 * @param insideFlag Puntero a la bandera de estado dentro del menú
 */
void MenuSys_SetCallbacks(MenuSystem *ms,
                          ClearFunction clear,
                          DrawItemFunction draw,
                          RenderFunction render,
                          volatile uint8_t *insideFlag,
                          RenderFunction dashboardRender);

void MenuSys_MoveCursorDown(MenuSystem *ms);
void MenuSys_MoveCursorUp(MenuSystem *ms);

void MenuSys_RenderMenu(MenuSystem *ms);
void MenuSys_NavigateBack(MenuSystem *ms);
void MenuSys_NavigateToMain(MenuSystem *ms);
void MenuSys_ResetMenu(MenuSystem *ms);
void MenuSys_OpenSubMenu(MenuSystem *ms, SubMenu *submenu);
void MenuSys_HandleClick(MenuSystem *ms);

bool MenuSys_IsItemVisible(const MenuItem *item);

static inline bool Menu_IsCurrentItem(const MenuSystem *sys, const MenuItem *item)
{
    return &sys->currentMenu->items[sys->currentMenu->currentItemIndex] == item;
}

#ifdef __cplusplus
}
#endif

#endif /* INC_MENUSYSTEM_H_ */
