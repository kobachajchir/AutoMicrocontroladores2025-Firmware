#ifndef EVENTMANAGERS_H
#define EVENTMANAGERS_H

#include "types/userEvent_type.h"

// Tipo de función para callbacks de render
typedef void (*RenderFn)(void);
typedef void (*RenderWrapperFn)(void);

/**
 * @brief Estructura de callbacks para manejar eventos de usuario
 */
typedef struct {
    RenderFn onRotateCW;      // Callback al rotar sentido horario
    RenderFn onRotateCCW;     // Callback al rotar sentido antihorario
    RenderFn onShortPress;    // Callback al presionar encoder corto
    RenderFn onLongPress;     // Callback al presionar botón usuario largo
    RenderFn onUserButton;    // Callback al presionar botón usuario corto
    RenderFn onEncLongPress;  // Callback al presionar encoder largo
} EventCallbacks_t;

// Manager genérico
void GenericEventManager(UserEvent_t ev, const EventCallbacks_t *callbacks);

// Event managers públicos
void dashboardEventManager(UserEvent_t ev);
void menuEventManager(UserEvent_t ev);
void About_UserEventManager(UserEvent_t ev);
void motorTestEventManager(UserEvent_t ev);
void WiFiSearch_UserEventManager(UserEvent_t ev);
void ReadOnly_UserEventManager(UserEvent_t ev);

// Legacy (por compatibilidad)
void ItemEventManager(UserEvent_t ev, RenderWrapperFn wrapper);

#endif /* EVENTMANAGERS_H */
