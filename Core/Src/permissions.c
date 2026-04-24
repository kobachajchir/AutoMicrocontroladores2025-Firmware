/*
 * permissions.c
 *
 * Non-blocking PIN authorization manager.
 */

#include "permissions.h"

#include <string.h>

#include "globals.h"
#include "menusystem.h"
#include "oled_utils.h"
#include "uner_app.h"
#include "ssd1306.h"
#include "stm32f1xx_hal.h"

#define PERMISSION_DEFAULT_TTL_MS        30000u
#define PERMISSION_REQUEST_TIMEOUT_MS     5000u
#define PERMISSION_RESPONSE_TTL_MAX_MS  300000u

typedef struct {
    PermissionContinuationFn onGranted;
    void *ctx;
} PendingPermissionAction_t;

typedef struct {
    AuthState_t state;
    PermissionId_t requestedPermission;
    PendingPermissionAction_t pendingAction;

    uint8_t requestId;
    uint8_t pinDigits[PERMISSION_PIN_DIGITS];
    uint8_t pinIndex;
    uint8_t attemptsLeft;
    uint8_t validationSendPending;

    uint32_t requestStartedAt;
    uint32_t authorizedUntil[PERMISSION_COUNT];

    RenderFunction previousRenderFn;
    UserEventManagerFn previousUserEventManagerFn;
    bool previousAllowPeriodicRefresh;
    uint8_t previousInsideMenu;

    PermissionEventCallback eventCallback;
} PermissionManager_t;

static PermissionManager_t permissionManager;

static void Permission_FormatAttempts(char *buf, size_t len, uint8_t attempts)
{
    static const char prefix[] = "Intentos: ";
    char digits[3];
    uint8_t count = 0u;
    size_t pos = 0u;

    if (!buf || len == 0u) {
        return;
    }

    while (prefix[pos] != '\0' && pos + 1u < len) {
        buf[pos] = prefix[pos];
        pos++;
    }

    do {
        digits[count++] = (char)('0' + (attempts % 10u));
        attempts /= 10u;
    } while (attempts != 0u && count < (uint8_t)sizeof(digits));

    while (count > 0u && pos + 1u < len) {
        buf[pos++] = digits[--count];
    }

    buf[pos] = '\0';
}

static const PermissionPolicy_t permissionPolicies[PERMISSION_COUNT] = {
    [PERMISSION_NONE] = {
        .id = PERMISSION_NONE,
        .ttl_ms = 0u,
        .max_attempts = 0u,
    },
    [PERMISSION_ESP_RESET] = {
        .id = PERMISSION_ESP_RESET,
        .ttl_ms = 30000u,
        .max_attempts = 3u,
    },
    [PERMISSION_FACTORY_RESET] = {
        .id = PERMISSION_FACTORY_RESET,
        .ttl_ms = 15000u,
        .max_attempts = 3u,
    },
    [PERMISSION_WIFI_CREDENTIALS] = {
        .id = PERMISSION_WIFI_CREDENTIALS,
        .ttl_ms = 60000u,
        .max_attempts = 3u,
    },
    [PERMISSION_MOTOR_TEST] = {
        .id = PERMISSION_MOTOR_TEST,
        .ttl_ms = 30000u,
        .max_attempts = 3u,
    },
    [PERMISSION_SYSTEM_CONFIG] = {
        .id = PERMISSION_SYSTEM_CONFIG,
        .ttl_ms = 60000u,
        .max_attempts = 3u,
    },
    [PERMISSION_CAR_MODE_TEST] = {
        .id = PERMISSION_CAR_MODE_TEST,
        .ttl_ms = 60000u,
        .max_attempts = 3u,
    },
};

static const PermissionPolicy_t *Permission_GetPolicy(PermissionId_t permission)
{
    if (permission >= PERMISSION_COUNT) {
        return &permissionPolicies[PERMISSION_NONE];
    }

    return &permissionPolicies[permission];
}

static void Permission_EmitFor(PermissionId_t permission, PermissionEventId_t event)
{
    if (!permissionManager.eventCallback) {
        return;
    }

    PermissionEvent_t permissionEvent = {
        .event = event,
        .permission = permission,
        .request_id = permissionManager.requestId,
        .attempts_left = permissionManager.attemptsLeft,
    };

    permissionManager.eventCallback(&permissionEvent);
}

static void Permission_Emit(PermissionEventId_t event)
{
    Permission_EmitFor(permissionManager.requestedPermission, event);
}

static void Permission_ClearPin(void)
{
    memset(permissionManager.pinDigits, 0, sizeof(permissionManager.pinDigits));
    permissionManager.pinIndex = 0u;
}

static void Permission_RequestRender(void)
{
    menuSystem.renderFn = Permission_RenderPinEntry;
    menuSystem.userEventManagerFn = Permission_HandleUserEvent;
    menuSystem.allowPeriodicRefresh = false;
    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static void Permission_SaveUiContext(void)
{
    permissionManager.previousRenderFn = menuSystem.renderFn;
    permissionManager.previousUserEventManagerFn = menuSystem.userEventManagerFn;
    permissionManager.previousAllowPeriodicRefresh = menuSystem.allowPeriodicRefresh;
    permissionManager.previousInsideMenu = menuSystem.insideMenuFlag ? *menuSystem.insideMenuFlag : 0u;
}

static void Permission_RestoreUiContext(void)
{
    menuSystem.renderFn = permissionManager.previousRenderFn;
    menuSystem.userEventManagerFn = permissionManager.previousUserEventManagerFn;
    menuSystem.allowPeriodicRefresh = permissionManager.previousAllowPeriodicRefresh;

    if (menuSystem.insideMenuFlag) {
        *menuSystem.insideMenuFlag = permissionManager.previousInsideMenu;
    }

    if (menuSystem.clearScreen) {
        menuSystem.clearScreen();
    }

    menuSystem.renderFlag = true;
    oled_first_draw = true;
}

static void Permission_ClearPendingRequest(void)
{
    permissionManager.state = AUTH_IDLE;
    permissionManager.requestedPermission = PERMISSION_NONE;
    permissionManager.pendingAction.onGranted = NULL;
    permissionManager.pendingAction.ctx = NULL;
    permissionManager.validationSendPending = 0u;
    permissionManager.requestStartedAt = 0u;
    Permission_ClearPin();
}

static bool Permission_IsExpired(uint32_t deadline, uint32_t now_ms)
{
    return deadline != 0u && ((int32_t)(deadline - now_ms) <= 0);
}

static void Permission_StartInput(void)
{
    const PermissionPolicy_t *policy = Permission_GetPolicy(permissionManager.requestedPermission);

    Permission_ClearPin();
    permissionManager.state = AUTH_INPUT_PIN;
    permissionManager.validationSendPending = 0u;
    permissionManager.attemptsLeft = policy->max_attempts;
    permissionManager.requestStartedAt = HAL_GetTick();
    Permission_RequestRender();
    Permission_Emit(PERMISSION_EVT_PIN_ENTRY_STARTED);
}

static void Permission_CancelRequest(void)
{
    permissionManager.state = AUTH_CANCELLED;
    Permission_Emit(PERMISSION_EVT_CANCELLED);
    Permission_ClearPendingRequest();
    Permission_RestoreUiContext();
}

static void Permission_MarkResult(AuthState_t state, PermissionEventId_t event)
{
    permissionManager.state = state;
    permissionManager.validationSendPending = 0u;
    Permission_ClearPin();
    Permission_RequestRender();
    Permission_Emit(event);
}

static uint32_t Permission_ResolveTtl(PermissionId_t permission, uint32_t ttl_ms)
{
    const PermissionPolicy_t *policy = Permission_GetPolicy(permission);
    uint32_t resolved = ttl_ms;

    if (resolved == 0u) {
        resolved = policy->ttl_ms;
    }

    if (resolved == 0u) {
        resolved = PERMISSION_DEFAULT_TTL_MS;
    }

    if (resolved > PERMISSION_RESPONSE_TTL_MAX_MS) {
        resolved = PERMISSION_RESPONSE_TTL_MAX_MS;
    }

    return resolved;
}

static void Permission_Grant(uint32_t ttl_ms)
{
    const PermissionId_t permission = permissionManager.requestedPermission;
    const uint32_t now = HAL_GetTick();
    const uint32_t resolvedTtl = Permission_ResolveTtl(permission, ttl_ms);
    PermissionContinuationFn onGranted = permissionManager.pendingAction.onGranted;
    void *ctx = permissionManager.pendingAction.ctx;

    if (permission > PERMISSION_NONE && permission < PERMISSION_COUNT) {
        permissionManager.authorizedUntil[permission] = now + resolvedTtl;
        if (permissionManager.authorizedUntil[permission] == 0u) {
            permissionManager.authorizedUntil[permission] = 1u;
        }
    }

    permissionManager.state = AUTH_GRANTED;
    Permission_Emit(PERMISSION_EVT_GRANTED);

    Permission_ClearPendingRequest();
    Permission_RestoreUiContext();

    if (onGranted) {
        onGranted(ctx);
    }
}

static bool Permission_HasActiveRequest(void)
{
    return (permissionManager.state == AUTH_INPUT_PIN ||
            permissionManager.state == AUTH_WAITING_ESP) &&
           permissionManager.requestedPermission > PERMISSION_NONE &&
           permissionManager.requestedPermission < PERMISSION_COUNT;
}

static uint8_t Permission_ResolveDeniedAttempts(uint8_t attempts_left)
{
    if (attempts_left == 0xFFu) {
        return permissionManager.attemptsLeft > 0u
            ? (uint8_t)(permissionManager.attemptsLeft - 1u)
            : 0u;
    }

    return attempts_left;
}

static void Permission_ApplyDeniedState(PermissionId_t permission, uint8_t attempts_left)
{
    permissionManager.attemptsLeft = attempts_left;

    if (permission == PERMISSION_CAR_MODE_TEST) {
        Permission_Emit(attempts_left == 0u ? PERMISSION_EVT_LOCKED : PERMISSION_EVT_DENIED);
        Permission_ClearPendingRequest();
        Permission_RestoreUiContext();
        OledUtils_ShowSimpleNotificationMs(OLED_SIMPLE_NOTIFICATION_PERMISSION_DENIED, 2200u);
        return;
    }

    if (attempts_left == 0u) {
        Permission_MarkResult(AUTH_LOCKED, PERMISSION_EVT_LOCKED);
    } else {
        Permission_MarkResult(AUTH_DENIED, PERMISSION_EVT_DENIED);
    }
}

static void Permission_ApplyTimeoutState(void)
{
    Permission_MarkResult(AUTH_TIMEOUT, PERMISSION_EVT_TIMEOUT);
}

static void Permission_TrySendValidation(void)
{
#if PERMISSION_EXTERNAL_PIN_GRANT_ONLY
    return;
#else
    if (permissionManager.state != AUTH_WAITING_ESP ||
        !permissionManager.validationSendPending) {
        return;
    }

    if (UNER_App_IsWaitingValidation()) {
        return;
    }

    uint8_t payload[2u + PERMISSION_PIN_DIGITS] = {
        permissionManager.requestId,
        (uint8_t)permissionManager.requestedPermission,
        permissionManager.pinDigits[0],
        permissionManager.pinDigits[1],
        permissionManager.pinDigits[2],
        permissionManager.pinDigits[3],
    };

    if (UNER_App_SendCommand(UNER_CMD_ID_AUTH_VALIDATE_PIN,
                             payload,
                             (uint8_t)sizeof(payload)) == UNER_OK) {
        permissionManager.validationSendPending = 0u;
        Permission_ClearPin();
        Permission_Emit(PERMISSION_EVT_VALIDATION_STARTED);
    }
#endif
}

#if !PERMISSION_EXTERNAL_PIN_GRANT_ONLY
static void Permission_SubmitPin(void)
{
    permissionManager.requestId++;
    if (permissionManager.requestId == 0u) {
        permissionManager.requestId = 1u;
    }

    permissionManager.state = AUTH_WAITING_ESP;
    permissionManager.validationSendPending = 1u;
    permissionManager.requestStartedAt = HAL_GetTick();
    Permission_RequestRender();
    Permission_Emit(PERMISSION_EVT_PIN_SUBMITTED);
    Permission_TrySendValidation();
}
#endif

static void Permission_ResetForRetry(void)
{
    if (permissionManager.state != AUTH_DENIED) {
        return;
    }

    Permission_ClearPin();
    permissionManager.state = AUTH_INPUT_PIN;
    permissionManager.requestStartedAt = HAL_GetTick();
    Permission_RequestRender();
}

static void Permission_DrawText(uint8_t x, uint8_t y, FontDef font, const char *text)
{
    ssd1306_SetColor(White);
    ssd1306_SetCursor(x, y);
    (void)ssd1306_WriteString((char *)text, font);
}

static void Permission_DrawBaselineText(uint8_t x, uint8_t baseline_y, FontDef font, const char *text)
{
    uint8_t y = baseline_y;

    if (baseline_y >= font.FontHeight) {
        y = (uint8_t)(baseline_y - font.FontHeight);
    }

    Permission_DrawText(x, y, font, text);
}

static void Permission_RenderPinDigits(void)
{
    char digitText[2] = {0};
    const FontDef digitFont = OLED_LARGE_FONT;
    const uint8_t frameY = 20u;
    const uint8_t frameW = 18u;
    const uint8_t frameH = 20u;
    const uint8_t frameX[PERMISSION_PIN_DIGITS] = {20u, 43u, 66u, 89u};

    for (uint8_t i = 0u; i < PERMISSION_PIN_DIGITS; i++) {
        const uint8_t textX = (uint8_t)(frameX[i] + ((frameW - digitFont.FontWidth) / 2u));
        const uint8_t textY = (uint8_t)(frameY + ((frameH - digitFont.FontHeight) / 2u));

#if PERMISSION_EXTERNAL_PIN_GRANT_ONLY
        digitText[0] = '*';
#else
        if (permissionManager.state == AUTH_WAITING_ESP) {
            digitText[0] = '*';
        } else {
            digitText[0] = (char)('0' + permissionManager.pinDigits[i]);
        }
#endif

        ssd1306_SetColor(White);
        ssd1306_DrawRect(frameX[i], frameY, frameW, frameH);

#if !PERMISSION_EXTERNAL_PIN_GRANT_ONLY
        if (permissionManager.state == AUTH_INPUT_PIN && i == permissionManager.pinIndex) {
            ssd1306_DrawRect((int16_t)frameX[i] - 2, frameY - 2, frameW + 4, frameH + 4);
        }
#endif

        Permission_DrawText(textX, textY, digitFont, digitText);
    }
}

static void Permission_RenderPinForm(const char *confirmText)
{
    ssd1306_SetColor(White);
    ssd1306_DrawBitmap(33u, 0u, LOCK_ICON_W, LOCK_ICON_H, Icon_Lock_bits);
    Permission_DrawBaselineText(49u, 14u, Font_7x10, "PIN");
    Permission_RenderPinDigits();

    ssd1306_SetColor(White);
    ssd1306_DrawBitmap(27u, 47u, 15u, 16u, Icon_UserBtn_bits);
    Permission_DrawBaselineText(45u, 60u, Font_7x10, confirmText);
}

void Permission_Init(void)
{
    PermissionEventCallback callback = permissionManager.eventCallback;

    memset(&permissionManager, 0, sizeof(permissionManager));
    permissionManager.state = AUTH_IDLE;
    permissionManager.requestedPermission = PERMISSION_NONE;
    permissionManager.eventCallback = callback;
}

void Permission_Task(uint32_t now_ms)
{
    Permission_TrySendValidation();

    if (permissionManager.state == AUTH_WAITING_ESP &&
        ((uint32_t)(now_ms - permissionManager.requestStartedAt) >= PERMISSION_REQUEST_TIMEOUT_MS)) {
        Permission_ApplyTimeoutState();
    }

    for (uint8_t i = (uint8_t)PERMISSION_NONE + 1u; i < (uint8_t)PERMISSION_COUNT; i++) {
        if (Permission_IsExpired(permissionManager.authorizedUntil[i], now_ms)) {
            permissionManager.authorizedUntil[i] = 0u;
            Permission_EmitFor((PermissionId_t)i, PERMISSION_EVT_EXPIRED);
        }
    }
}

bool Permission_IsAuthorized(PermissionId_t permission, uint32_t now_ms)
{
    if (permission == PERMISSION_NONE) {
        return true;
    }

    if (permission >= PERMISSION_COUNT) {
        return false;
    }

    return permissionManager.authorizedUntil[permission] != 0u &&
           !Permission_IsExpired(permissionManager.authorizedUntil[permission], now_ms);
}

bool Permission_IsAuthorizedNow(PermissionId_t permission)
{
    return Permission_IsAuthorized(permission, HAL_GetTick());
}

bool Permission_Request(PermissionId_t permission,
                        PermissionContinuationFn on_granted,
                        void *ctx)
{
    if (permission == PERMISSION_NONE) {
        if (on_granted) {
            on_granted(ctx);
        }
        return true;
    }

    if (permission >= PERMISSION_COUNT) {
        return false;
    }

    if (Permission_IsAuthorizedNow(permission)) {
        if (on_granted) {
            on_granted(ctx);
        }
        return true;
    }

    if (permissionManager.state != AUTH_IDLE) {
        Permission_EmitFor(permission, PERMISSION_EVT_BUSY);
        return false;
    }

    Permission_SaveUiContext();
    permissionManager.requestedPermission = permission;
    permissionManager.pendingAction.onGranted = on_granted;
    permissionManager.pendingAction.ctx = ctx;
    Permission_Emit(PERMISSION_EVT_REQUIRED);
    Permission_StartInput();

    return true;
}

void Permission_Revoke(PermissionId_t permission)
{
    if (permission == PERMISSION_NONE || permission >= PERMISSION_COUNT) {
        return;
    }

    permissionManager.authorizedUntil[permission] = 0u;
    Permission_EmitFor(permission, PERMISSION_EVT_REVOKED);
}

void Permission_RevokeAll(void)
{
    for (uint8_t i = (uint8_t)PERMISSION_NONE + 1u; i < (uint8_t)PERMISSION_COUNT; i++) {
        Permission_Revoke((PermissionId_t)i);
    }
}

void Permission_HandleUserEvent(UserEvent_t ev)
{
    switch (permissionManager.state) {
        case AUTH_INPUT_PIN:
#if PERMISSION_EXTERNAL_PIN_GRANT_ONLY
            if (ev == UE_ENC_LONG_PRESS || ev == UE_LONG_PRESS) {
                Permission_CancelRequest();
            }
#else
            if (ev == UE_ROTATE_CW) {
                permissionManager.pinDigits[permissionManager.pinIndex] =
                    (uint8_t)((permissionManager.pinDigits[permissionManager.pinIndex] + 1u) % 10u);
                Permission_RequestRender();
            } else if (ev == UE_ROTATE_CCW) {
                permissionManager.pinDigits[permissionManager.pinIndex] =
                    (uint8_t)((permissionManager.pinDigits[permissionManager.pinIndex] + 9u) % 10u);
                Permission_RequestRender();
            } else if (ev == UE_ENC_SHORT_PRESS) {
                permissionManager.pinIndex =
                    (uint8_t)((permissionManager.pinIndex + 1u) % PERMISSION_PIN_DIGITS);
                Permission_RequestRender();
            } else if (ev == UE_SHORT_PRESS) {
                Permission_SubmitPin();
            } else if (ev == UE_ENC_LONG_PRESS || ev == UE_LONG_PRESS) {
                Permission_CancelRequest();
            }
#endif
            break;

        case AUTH_DENIED:
            if (ev == UE_ENC_SHORT_PRESS) {
                Permission_ResetForRetry();
            } else if (ev == UE_ENC_LONG_PRESS || ev == UE_SHORT_PRESS || ev == UE_LONG_PRESS) {
                Permission_CancelRequest();
            }
            break;

        case AUTH_WAITING_ESP:
            if (ev == UE_ENC_LONG_PRESS || ev == UE_LONG_PRESS) {
                Permission_CancelRequest();
            }
            break;

        case AUTH_TIMEOUT:
        case AUTH_LOCKED:
        case AUTH_CANCELLED:
            if (ev == UE_ENC_SHORT_PRESS ||
                ev == UE_ENC_LONG_PRESS ||
                ev == UE_SHORT_PRESS ||
                ev == UE_LONG_PRESS) {
                Permission_CancelRequest();
            }
            break;

        case AUTH_IDLE:
        case AUTH_GRANTED:
        case AUTH_EXPIRED:
        default:
            break;
    }
}

void Permission_RenderPinEntry(void)
{
    char line[22];
    ScreenCode_t screen_code = SCREEN_CODE_WARNING_PIN_ENTRY;

    ssd1306_Clear();
    ssd1306_SetColor(White);

    switch (permissionManager.state) {
        case AUTH_WAITING_ESP:
            screen_code = SCREEN_CODE_WARNING_PIN_WAITING;
            MenuSys_SetCurrentScreenCode(&menuSystem, screen_code, SCREEN_REPORT_SOURCE_PERMISSION);
            Permission_RenderPinForm("Validando");
            break;

        case AUTH_DENIED:
            screen_code = SCREEN_CODE_WARNING_PIN_DENIED;
            MenuSys_SetCurrentScreenCode(&menuSystem, screen_code, SCREEN_REPORT_SOURCE_PERMISSION);
            Permission_DrawText(15, 14, OLED_LARGE_FONT, "Rechazado");
            Permission_FormatAttempts(line, sizeof(line), permissionManager.attemptsLeft);
            Permission_DrawText(4, 42, Font_7x10, line);
            Permission_DrawText(4, 58, Font_7x10, "OK: reintentar");
            break;

        case AUTH_TIMEOUT:
            screen_code = SCREEN_CODE_WARNING_PIN_TIMEOUT;
            MenuSys_SetCurrentScreenCode(&menuSystem, screen_code, SCREEN_REPORT_SOURCE_PERMISSION);
            Permission_DrawText(26, 14, OLED_LARGE_FONT, "Sin resp.");
            Permission_DrawText(4, 42, Font_7x10, "ESP no valido");
            Permission_DrawText(4, 58, Font_7x10, "OK: volver");
            break;

        case AUTH_LOCKED:
            screen_code = SCREEN_CODE_WARNING_PIN_BLOCKED;
            MenuSys_SetCurrentScreenCode(&menuSystem, screen_code, SCREEN_REPORT_SOURCE_PERMISSION);
            Permission_DrawText(18, 14, OLED_LARGE_FONT, "Bloqueado");
            Permission_DrawText(4, 42, Font_7x10, "Demasiados fallos");
            Permission_DrawText(4, 58, Font_7x10, "OK: volver");
            break;

        case AUTH_INPUT_PIN:
        default:
            MenuSys_SetCurrentScreenCode(&menuSystem, screen_code, SCREEN_REPORT_SOURCE_PERMISSION);
#if PERMISSION_EXTERNAL_PIN_GRANT_ONLY
            Permission_RenderPinForm("Esperando");
#else
            Permission_RenderPinForm("Confirmar");
#endif
            break;
    }
}

void Permission_OnValidationResult(uint8_t request_id,
                                   PermissionId_t permission,
                                   bool granted,
                                   uint32_t ttl_ms,
                                   uint8_t attempts_left)
{
    if (permissionManager.state != AUTH_WAITING_ESP ||
        permissionManager.requestId != request_id ||
        permissionManager.requestedPermission != permission) {
        return;
    }

    if (granted) {
        if (attempts_left != 0xFFu) {
            permissionManager.attemptsLeft = attempts_left;
        }
        Permission_Grant(ttl_ms);
    } else {
        Permission_ApplyDeniedState(permission, Permission_ResolveDeniedAttempts(attempts_left));
    }
}

bool Permission_GrantCurrentRequest(uint32_t ttl_ms)
{
    if (!Permission_HasActiveRequest()) {
        return false;
    }

    Permission_Grant(ttl_ms);
    return true;
}

bool Permission_ApplyRemoteResult(uint8_t result_code, uint8_t attempts_left)
{
    if (!Permission_HasActiveRequest()) {
        return false;
    }

    switch (result_code) {
        case UNER_AUTH_REMOTE_RESULT_DENIED:
            Permission_ApplyDeniedState(permissionManager.requestedPermission,
                                        Permission_ResolveDeniedAttempts(attempts_left));
            return true;

        case UNER_AUTH_REMOTE_RESULT_TIMEOUT:
            Permission_ApplyTimeoutState();
            return true;

        case UNER_AUTH_REMOTE_RESULT_BLOCKED:
            Permission_ApplyDeniedState(permissionManager.requestedPermission, 0u);
            return true;

        default:
            return false;
    }
}

void Permission_SetEventCallback(PermissionEventCallback callback)
{
    permissionManager.eventCallback = callback;
}

AuthState_t Permission_GetState(void)
{
    return permissionManager.state;
}

PermissionId_t Permission_GetRequestedPermission(void)
{
    return permissionManager.requestedPermission;
}
