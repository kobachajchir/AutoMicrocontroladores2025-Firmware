/*
 * menuSystem.h
 *
 *  Created on: Jun 11, 2025
 *      Author: kobac
 */

#ifndef INC_MENUSYSTEM_H_
#define INC_MENUSYSTEM_H_


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
 * @param name Nombre del ítem
 * @param y Posición vertical
 * @param selected Verdadero si el ítem está seleccionado
 */
typedef void (*DrawItemFunction)(const char *name, int y, bool selected);

/**
 * @brief Tipo de función para renderizar el menú completo (flush final).
 */
typedef void (*RenderFunction)();

// Estructura para un ítem del menú
typedef struct MenuItem {
    const char *name;           ///< Nombre del ítem del menú
    MenuFunction action;       ///< Acción a ejecutar al seleccionar el ítem
    struct SubMenu *submenu;   ///< Submenú asociado (si existe)
    const uint8_t *icon;       ///< Icono asociado (opcional)
} MenuItem;

// Estructura para un submenú
typedef struct SubMenu {
    const char *name;             ///< Nombre del submenú
    MenuItem *items;              ///< Lista de ítems del submenú
    uint8_t itemCount;                ///< Cantidad de ítems
    uint8_t currentItemIndex;         ///< Índice del ítem seleccionado actualmente
    uint8_t firstVisibleItem;         ///< Primer ítem visible para scroll
    struct SubMenu *parent;       ///< Puntero al submenú padre (para 'Volver')
    const uint8_t *icon;          ///< Icono asociado (opcional)
} SubMenu;

// Estructura principal del sistema de menús
typedef struct MenuSystem {
    SubMenu *currentMenu;               ///< Submenú actual
    ClearFunction clearScreen;          ///< Callback para limpiar la pantalla
    DrawItemFunction drawItem;          ///< Callback para dibujar ítems del menú
    RenderFunction renderFn;            ///< Callback para renderizar la pantalla (flush)
    volatile uint8_t *insideMenuFlag;   ///< Puntero a una bandera externa que indica si estamos dentro del menú
} MenuSystem;

// =================== PROTOTIPOS ===================

/**
 * @brief Inicializa el sistema de menú apuntando al menú principal.
 * @param system Puntero al sistema de menú
 */
void initMenuSystem(MenuSystem *system);

void MenuSystem_SetCallbacks(MenuSystem *system,
                             ClearFunction clear,
                             DrawItemFunction draw,
                             RenderFunction render,
							 volatile uint8_t *insideFlag)

/**
 * @brief Mueve el cursor una posición hacia arriba.
 * @param system Puntero al sistema de menú
 */
void moveCursorUp(MenuSystem *system);

/**
 * @brief Mueve el cursor una posición hacia abajo.
 * @param system Puntero al sistema de menú
 */
void moveCursorDown(MenuSystem *system);

/**
 * @brief Ejecuta la acción del ítem actualmente seleccionado.
 * @param system Puntero al sistema de menú
 */
void selectCurrentItem(MenuSystem *system);

/**
 * @brief Dibuja el menú actual usando las funciones de dibujo configuradas.
 * @param system Puntero al sistema de menú
 */
void displayMenu(MenuSystem *system);

/**
 * @brief Cambia al submenú 1 y actualiza la pantalla.
 */
void submenu1Fn(void);

/**
 * @brief Cambia al submenú 2 y actualiza la pantalla.
 */
void submenu2Fn(void);

/**
 * @brief Cambia al submenú 3 y actualiza la pantalla.
 */
void submenu3Fn(void);

/**
 * @brief Vuelve al menú padre del menú actual, si existe.
 *        Si se encuentra en el menú principal, no hace nada.
 */
void volver(void);

/**
 * @brief Selecciona un ítem del menú actual por índice.
 * @param system Puntero al sistema de menú
 * @param index Índice del ítem a ejecutar
 */
void selectMenuItem(MenuSystem *system, int index);

/**
 * @brief Cambia el menú actual al menú principal y reinicia la posición del cursor.
 */
void navigateToMainMenu(void);

/**
 * @brief Vuelve al menú padre o sale del menú si ya está en el menú principal.
 *        Limpia la pantalla y llama a renderFn si está configurado.
 */
void navigateBackInMenu(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_MENUSYSTEM_H_ */
