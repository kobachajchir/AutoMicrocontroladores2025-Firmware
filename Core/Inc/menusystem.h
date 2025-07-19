/*
 * menuSystem.h
 *
 *  Created on: Jun 11, 2025
 *      Author: kobac
 */

#ifndef INC_MENUSYSTEM_H_
#define INC_MENUSYSTEM_H_

#include <stdbool.h>  // para el tipo bool
#include <stdint.h>   // para uint8_t, etc.
#include <stddef.h>   // para NULL

#ifdef __cplusplus
extern "C" {
#endif

	#ifndef MAX_VISIBLE_ITEMS
	#define MAX_VISIBLE_ITEMS 2  // se puede redefinir externamente si se desea

	/**
	 * @brief Tipo de función para acciones de ítems del menú.
	 */
	typedef void (*MenuFunction)();

	/**
	 * @brief Tipo de función para limpiar pantalla.
	 */
	typedef void (*ClearFunction)();

	/**
	 * @brief Tipo de función para dibujar un ítem del menú.
	 * @param *item Item
	 * @param y Posición vertical
	 * @param selected Verdadero si el ítem está seleccionado
	 */

	/**
	 * @brief Tipo de función para renderizar el menú completo (flush final).
	 */
	typedef void (*RenderFunction)();

	typedef void (*RenderScreenFunction)();

	typedef struct SubMenu SubMenu;

	// Estructura para un ítem del menú
	typedef struct MenuItem {
		const char *name;           ///< Nombre del ítem del menú
		MenuFunction action;       ///< Acción a ejecutar al seleccionar el ítem
		struct SubMenu *submenu;   ///< Submenú asociado (si existe)
		const uint8_t *icon;       ///< Icono asociado (opcional)
		RenderScreenFunction screenRenderFn;             ///< Puntero a la pantalla asociada (si existe)
	} MenuItem;

	typedef void (*DrawItemFunction)(const MenuItem *item, int y, bool selected);
	// Estructura para un submenú
	typedef struct SubMenu {
		const char *name;             ///< Nombre del submenú
		MenuItem *items;              ///< Lista de ítems del submenú
		uint8_t itemCount;                ///< Cantidad de ítems
		int8_t currentItemIndex;         ///< Índice del ítem seleccionado actualmente
		int8_t lastSelectedItemIndex;
		int8_t firstVisibleItem;         ///< Primer ítem visible para scroll
		int8_t  lastVisibleItem;
		struct SubMenu *parent;       ///< Puntero al submenú padre (para 'Volver')
		const uint8_t *icon;          ///< Icono asociado (opcional)
	} SubMenu;

	// Estructura principal del sistema de menús
	typedef struct MenuSystem {
		SubMenu *currentMenu;               ///< Submenú actual
		ClearFunction clearScreen;          ///< Callback para limpiar la pantalla
		DrawItemFunction drawItem;          ///< Callback para dibujar ítems del menú
		RenderFunction renderFn;            ///< Callback para renderizar la pantalla
		RenderFunction dashboardRender;
		volatile uint8_t *insideMenuFlag;   ///< Puntero a una bandera externa que indica si estamos dentro del menú
		bool renderFlag;
		bool allowPeriodicRefresh;
	} MenuSystem;

	// =================== PROTOTIPOS ===================

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
	                          ClearFunction    clear,
	                          DrawItemFunction draw,
	                          RenderFunction   render,
	                          volatile uint8_t *insideFlag,
							  RenderFunction dashboardRender);

	void MenuSys_MoveCursorDown(MenuSystem *ms) ;
	void MenuSys_MoveCursorUp(MenuSystem *ms) ;

	void MenuSys_RenderMenu(MenuSystem *ms);
	void MenuSys_NavigateBack(MenuSystem *ms);
	void MenuSys_NavigateToMain(MenuSystem *ms);



	static inline bool
	Menu_IsCurrentItem(const MenuSystem *sys,
	                   const MenuItem  *item)
	{
	    return &sys->currentMenu->items[sys->currentMenu->currentItemIndex] == item;
	}


#ifdef __cplusplus
}
#endif
#endif
#endif /* INC_MENUSYSTEM_H_ */
