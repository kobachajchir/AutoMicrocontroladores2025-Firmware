/*
 * menu_types.h
 *
 *  Created on: Jun 11, 2025
 *      Author: kobac
 */

#ifndef INC_TYPES_MENU_TYPES_H_
#define INC_TYPES_MENU_TYPES_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "screen_codes.h"
#include "userEvent_type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*MenuFunction)();
typedef void (*ClearFunction)();
typedef void (*RenderFunction)();
typedef void (*RenderWrapperFn)(void);
typedef void (*RenderScreenFunction)();
typedef void (*UserEventManagerFn)(UserEvent_t ev);
typedef bool (*MenuVisibilityFn)(void);

typedef struct SubMenu SubMenu;

typedef struct MenuItem {
    const char *name;                         ///< Nombre del ítem del menú
    MenuFunction action;                      ///< Acción a ejecutar al seleccionar el ítem
    struct SubMenu *submenu;                  ///< Submenú asociado (si existe)
    const uint8_t *icon;                      ///< Icono asociado (opcional)
    RenderScreenFunction screenRenderFn;      ///< Puntero a la pantalla asociada (si existe)
    UserEventManagerFn eventManagerFn;        ///< Callback manager para la pantalla asociada (si existe)
    MenuVisibilityFn visibilityFn;            ///< Condición para mostrar/seleccionar el ítem (si existe)
    ScreenCode_t screen_code;                 ///< Identificador estable de la pantalla destino
} MenuItem;

typedef void (*DrawItemFunction)(const MenuItem *item, int y, bool selected);

typedef struct SubMenu {
    const char *name;                         ///< Nombre del submenú
    MenuItem *items;                          ///< Lista de ítems del submenú
    uint8_t itemCount;                        ///< Cantidad de ítems
    int8_t currentItemIndex;                  ///< Índice del ítem seleccionado actualmente
    int8_t lastSelectedItemIndex;
    int8_t firstVisibleItem;                  ///< Primer ítem visible para scroll
    int8_t lastVisibleItem;
    struct SubMenu *parent;                   ///< Puntero al submenú padre (para 'Volver')
    const uint8_t *icon;                      ///< Icono asociado (opcional)
    ScreenCode_t screen_code;                 ///< Identificador estable de esta pantalla de menu
} SubMenu;

typedef struct MenuSystem {
    SubMenu *currentMenu;                     ///< Submenú actual
    ClearFunction clearScreen;                ///< Callback para limpiar la pantalla
    DrawItemFunction drawItem;                ///< Callback para dibujar ítems del menú
    RenderFunction renderFn;                  ///< Callback para renderizar la pantalla
    RenderFunction dashboardRender;
    UserEventManagerFn userEventManagerFn;
    volatile uint8_t *insideMenuFlag;         ///< Puntero a una bandera externa que indica si estamos dentro del menú
    volatile bool renderFlag;
    bool allowPeriodicRefresh;
    ScreenCode_t current_screen_code;
    ScreenCode_t last_reported_screen_code;
    uint8_t current_screen_source;
    bool screen_report_pending;
} MenuSystem;

#ifdef __cplusplus
}
#endif

#endif /* INC_TYPES_MENU_TYPES_H_ */
