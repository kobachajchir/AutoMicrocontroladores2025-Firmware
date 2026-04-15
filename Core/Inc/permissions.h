/*
 * permissions.h
 *
 * Permission and PIN authorization manager.
 */

#ifndef INC_PERMISSIONS_H_
#define INC_PERMISSIONS_H_

#include <stdbool.h>
#include <stdint.h>
#include "types/userEvent_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PERMISSION_PIN_DIGITS 4u

typedef enum {
    PERMISSION_NONE = 0,
    PERMISSION_ESP_RESET,
    PERMISSION_FACTORY_RESET,
    PERMISSION_WIFI_CREDENTIALS,
    PERMISSION_MOTOR_TEST,
    PERMISSION_SYSTEM_CONFIG,
    PERMISSION_CAR_MODE_TEST,
    PERMISSION_COUNT
} PermissionId_t;

typedef enum {
    AUTH_IDLE = 0,
    AUTH_INPUT_PIN,
    AUTH_WAITING_ESP,
    AUTH_GRANTED,
    AUTH_DENIED,
    AUTH_CANCELLED,
    AUTH_TIMEOUT,
    AUTH_EXPIRED,
    AUTH_LOCKED
} AuthState_t;

typedef enum {
    PERMISSION_EVT_REQUIRED = 0,
    PERMISSION_EVT_PIN_ENTRY_STARTED,
    PERMISSION_EVT_PIN_SUBMITTED,
    PERMISSION_EVT_VALIDATION_STARTED,
    PERMISSION_EVT_GRANTED,
    PERMISSION_EVT_DENIED,
    PERMISSION_EVT_TIMEOUT,
    PERMISSION_EVT_EXPIRED,
    PERMISSION_EVT_REVOKED,
    PERMISSION_EVT_CANCELLED,
    PERMISSION_EVT_LOCKED,
    PERMISSION_EVT_BUSY
} PermissionEventId_t;

typedef struct {
    PermissionId_t id;
    uint32_t ttl_ms;
    uint8_t max_attempts;
} PermissionPolicy_t;

typedef struct {
    PermissionEventId_t event;
    PermissionId_t permission;
    uint8_t request_id;
    uint8_t attempts_left;
} PermissionEvent_t;

typedef void (*PermissionContinuationFn)(void *ctx);
typedef void (*PermissionEventCallback)(const PermissionEvent_t *event);

void Permission_Init(void);
void Permission_Task(uint32_t now_ms);

bool Permission_IsAuthorized(PermissionId_t permission, uint32_t now_ms);
bool Permission_IsAuthorizedNow(PermissionId_t permission);

bool Permission_Request(PermissionId_t permission,
                        PermissionContinuationFn on_granted,
                        void *ctx);

void Permission_Revoke(PermissionId_t permission);
void Permission_RevokeAll(void);

void Permission_HandleUserEvent(UserEvent_t ev);
void Permission_RenderPinEntry(void);

void Permission_OnValidationResult(uint8_t request_id,
                                   PermissionId_t permission,
                                   bool granted,
                                   uint32_t ttl_ms,
                                   uint8_t attempts_left);

void Permission_SetEventCallback(PermissionEventCallback callback);
AuthState_t Permission_GetState(void);
PermissionId_t Permission_GetRequestedPermission(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_PERMISSIONS_H_ */
