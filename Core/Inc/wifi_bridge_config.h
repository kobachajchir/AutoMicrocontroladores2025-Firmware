/*
 * wifi_bridge_config.h - compile-time backend selection for WifiBridge.
 */
#ifndef INC_WIFI_BRIDGE_CONFIG_H_
#define INC_WIFI_BRIDGE_CONFIG_H_

#if defined(WIFI_BRIDGE_USE_UNER) && defined(WIFI_BRIDGE_USE_ESP_AT)
#error "Only one WifiBridge backend can be enabled"
#endif

#if !defined(WIFI_BRIDGE_USE_UNER) && !defined(WIFI_BRIDGE_USE_ESP_AT)
#error "Select one WifiBridge backend"
#endif

#define WIFI_BRIDGE_SSID_MAX_LEN          32u
#define WIFI_BRIDGE_PASS_MAX_LEN          64u
#define WIFI_BRIDGE_IP_LEN                 4u

#define WIFI_BRIDGE_AT_RX_DMA_SIZE       512u
#define WIFI_BRIDGE_AT_TX_SIZE           128u
#define WIFI_BRIDGE_AT_LINE_SIZE         160u
#define WIFI_BRIDGE_AT_TIMEOUT_MS       8000u

/*
 * Factory AT firmware has no UNER web credential flow. If the existing UI asks
 * to connect from a scanned SSID, the AT backend uses this build-time password.
 * Leave empty for open networks or define it from the build system.
 */
#ifndef WIFI_BRIDGE_AT_DEFAULT_PASSWORD
#define WIFI_BRIDGE_AT_DEFAULT_PASSWORD ""
#endif

#endif /* INC_WIFI_BRIDGE_CONFIG_H_ */
