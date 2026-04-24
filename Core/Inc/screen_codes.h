/*
 * screen_codes.h
 *
 * Stable numeric identifiers for user-visible UI screens.
 */

#ifndef INC_SCREEN_CODES_H_
#define INC_SCREEN_CODES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t ScreenCode_t;

#define SCREEN_CODE(menu, submenu, page) \
    ((((uint32_t)(menu) & 0xFFU) << 16) | \
     (((uint32_t)(submenu) & 0xFFU) << 8)  | \
     ((uint32_t)(page) & 0xFFU))

#define SCREEN_MENU(code)    (((uint32_t)(code) >> 16) & 0xFFU)
#define SCREEN_SUBMENU(code) (((uint32_t)(code) >> 8)  & 0xFFU)
#define SCREEN_PAGE(code)    ((uint32_t)(code) & 0xFFU)

typedef enum {
    SCREEN_REPORT_SOURCE_UNKNOWN      = 0u,
    SCREEN_REPORT_SOURCE_MENU         = 1u,
    SCREEN_REPORT_SOURCE_RENDER       = 2u,
    SCREEN_REPORT_SOURCE_NOTIFICATION = 3u,
    SCREEN_REPORT_SOURCE_PERMISSION   = 4u,
    SCREEN_REPORT_SOURCE_SYSTEM       = 5u,
} ScreenReportSource_t;

#define SCREEN_CODE_NONE                                      SCREEN_CODE(0x00u, 0x00u, 0x00u)

/* 0x01xxxx: core UI, dashboard, and top-level menu shell. */
#define SCREEN_CODE_CORE_STARTUP                              SCREEN_CODE(0x01u, 0x00u, 0x01u)
#define SCREEN_CODE_CORE_DASHBOARD                            SCREEN_CODE(0x01u, 0x01u, 0x01u)
#define SCREEN_CODE_CORE_MODE_CHANGE                          SCREEN_CODE(0x01u, 0x01u, 0x02u)
#define SCREEN_CODE_CORE_MAIN_MENU                            SCREEN_CODE(0x01u, 0x02u, 0x01u)

/* 0x02xxxx: connectivity, WiFi, ESP link, USB, and web bridge states. */
#define SCREEN_CODE_CONNECTIVITY_WIFI_MENU                    SCREEN_CODE(0x02u, 0x01u, 0x01u)
#define SCREEN_CODE_CONNECTIVITY_WIFI_STATUS                  SCREEN_CODE(0x02u, 0x02u, 0x01u)
#define SCREEN_CODE_CONNECTIVITY_WIFI_SEARCHING               SCREEN_CODE(0x02u, 0x02u, 0x02u)
#define SCREEN_CODE_CONNECTIVITY_WIFI_RESULTS                 SCREEN_CODE(0x02u, 0x02u, 0x03u)
#define SCREEN_CODE_CONNECTIVITY_WIFI_NOT_CONNECTED           SCREEN_CODE(0x02u, 0x02u, 0x04u)
#define SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTING              SCREEN_CODE(0x02u, 0x02u, 0x05u)
#define SCREEN_CODE_CONNECTIVITY_WIFI_CONNECTED               SCREEN_CODE(0x02u, 0x02u, 0x06u)
#define SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_COMPLETE         SCREEN_CODE(0x02u, 0x02u, 0x07u)
#define SCREEN_CODE_CONNECTIVITY_WIFI_SEARCH_CANCELED         SCREEN_CODE(0x02u, 0x02u, 0x08u)
#define SCREEN_CODE_CONNECTIVITY_WIFI_DETAILS                 SCREEN_CODE(0x02u, 0x02u, 0x09u)
#define SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_WEB         SCREEN_CODE(0x02u, 0x02u, 0x0Au)
#define SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_SUCCEEDED   SCREEN_CODE(0x02u, 0x02u, 0x0Bu)
#define SCREEN_CODE_CONNECTIVITY_WIFI_CREDENTIALS_FAILED      SCREEN_CODE(0x02u, 0x02u, 0x0Cu)

#define SCREEN_CODE_CONNECTIVITY_ESP_MENU                     SCREEN_CODE(0x02u, 0x03u, 0x01u)
#define SCREEN_CODE_CONNECTIVITY_ESP_CHECKING                 SCREEN_CODE(0x02u, 0x03u, 0x02u)
#define SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_REQUEST         SCREEN_CODE(0x02u, 0x03u, 0x03u)
#define SCREEN_CODE_CONNECTIVITY_ESP_RESET_SENT               SCREEN_CODE(0x02u, 0x03u, 0x04u)
#define SCREEN_CODE_CONNECTIVITY_ESP_CHECK_REQUIRED           SCREEN_CODE(0x02u, 0x03u, 0x05u)
#define SCREEN_CODE_CONNECTIVITY_ESP_BOOT_RECEIVED            SCREEN_CODE(0x02u, 0x03u, 0x06u)
#define SCREEN_CODE_CONNECTIVITY_ESP_FIRMWARE_RECEIVED        SCREEN_CODE(0x02u, 0x03u, 0x07u)
#define SCREEN_CODE_CONNECTIVITY_ESP_MODE_CHANGED             SCREEN_CODE(0x02u, 0x03u, 0x08u)
#define SCREEN_CODE_CONNECTIVITY_ESP_AP_STARTED               SCREEN_CODE(0x02u, 0x03u, 0x09u)
#define SCREEN_CODE_CONNECTIVITY_ESP_REBOOT_MODE              SCREEN_CODE(0x02u, 0x03u, 0x0Au)

#define SCREEN_CODE_CONNECTIVITY_USB_CONNECTED                SCREEN_CODE(0x02u, 0x04u, 0x01u)
#define SCREEN_CODE_CONNECTIVITY_USB_DISCONNECTED             SCREEN_CODE(0x02u, 0x04u, 0x02u)

#define SCREEN_CODE_CONNECTIVITY_WEB_SERVER_UP                SCREEN_CODE(0x02u, 0x05u, 0x01u)
#define SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_CONNECTED         SCREEN_CODE(0x02u, 0x05u, 0x02u)
#define SCREEN_CODE_CONNECTIVITY_WEB_CLIENT_DISCONNECTED      SCREEN_CODE(0x02u, 0x05u, 0x03u)

/* 0x03xxxx: sensor menus and live sensor views. */
#define SCREEN_CODE_SENSORS_MENU                              SCREEN_CODE(0x03u, 0x01u, 0x01u)
#define SCREEN_CODE_SENSORS_IR_VALUES                         SCREEN_CODE(0x03u, 0x02u, 0x01u)
#define SCREEN_CODE_SENSORS_MPU_VALUES                        SCREEN_CODE(0x03u, 0x03u, 0x01u)
#define SCREEN_CODE_SENSORS_RADAR                             SCREEN_CODE(0x03u, 0x04u, 0x01u)

/* 0x04xxxx: motor and motion/control views. */
#define SCREEN_CODE_MOTOR_TEST                                SCREEN_CODE(0x04u, 0x01u, 0x01u)

/* 0x05xxxx: settings and information pages. */
#define SCREEN_CODE_SETTINGS_MENU                             SCREEN_CODE(0x05u, 0x01u, 0x01u)
#define SCREEN_CODE_SETTINGS_ABOUT_PROJECT                    SCREEN_CODE(0x05u, 0x02u, 0x01u)
#define SCREEN_CODE_SETTINGS_ABOUT_REPO                       SCREEN_CODE(0x05u, 0x02u, 0x02u)
#define SCREEN_CODE_SETTINGS_WARNING_TIME                     SCREEN_CODE(0x05u, 0x03u, 0x01u)

/* 0x06xxxx: diagnostics, controller, and protocol status notifications. */
#define SCREEN_CODE_DIAG_CONTROLLER_CONNECTED                 SCREEN_CODE(0x06u, 0x01u, 0x01u)
#define SCREEN_CODE_DIAG_CONTROLLER_DISCONNECTED              SCREEN_CODE(0x06u, 0x01u, 0x02u)
#define SCREEN_CODE_DIAG_COMMAND_RECEIVED                     SCREEN_CODE(0x06u, 0x02u, 0x01u)
#define SCREEN_CODE_DIAG_PING_RECEIVED                        SCREEN_CODE(0x06u, 0x02u, 0x02u)
#define SCREEN_CODE_DIAG_ESP_CONN_SUCCEEDED                   SCREEN_CODE(0x06u, 0x03u, 0x01u)
#define SCREEN_CODE_DIAG_ESP_CONN_FAILED                      SCREEN_CODE(0x06u, 0x03u, 0x02u)

/* 0x07xxxx: test/service/development screens. */
#define SCREEN_CODE_SERVICE_TEST_SCREEN                       SCREEN_CODE(0x07u, 0x01u, 0x01u)

/* 0x08xxxx: warnings, locks, permission states, and fallback-like states. */
#define SCREEN_CODE_WARNING_LOCKED                            SCREEN_CODE(0x08u, 0x01u, 0x01u)
#define SCREEN_CODE_WARNING_PIN_INCORRECT                     SCREEN_CODE(0x08u, 0x01u, 0x02u)
#define SCREEN_CODE_WARNING_PIN_MODIFIED                      SCREEN_CODE(0x08u, 0x01u, 0x03u)
#define SCREEN_CODE_WARNING_PIN_ENTRY                         SCREEN_CODE(0x08u, 0x01u, 0x04u)
#define SCREEN_CODE_WARNING_PIN_WAITING                       SCREEN_CODE(0x08u, 0x01u, 0x05u)
#define SCREEN_CODE_WARNING_PIN_DENIED                        SCREEN_CODE(0x08u, 0x01u, 0x06u)
#define SCREEN_CODE_WARNING_PIN_TIMEOUT                       SCREEN_CODE(0x08u, 0x01u, 0x07u)
#define SCREEN_CODE_WARNING_PIN_BLOCKED                       SCREEN_CODE(0x08u, 0x01u, 0x08u)
#define SCREEN_CODE_WARNING_PERMISSION_DENIED                 SCREEN_CODE(0x08u, 0x01u, 0x09u)

#ifdef __cplusplus
}
#endif

#endif /* INC_SCREEN_CODES_H_ */
