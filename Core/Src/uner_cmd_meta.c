/*
 * uner_cmd_meta.c - UNER command metadata table
 */
#include "uner_cmd_meta.h"

// Event handlers (weak). Override by providing strong definitions elsewhere.
__weak void evt_boot_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

__weak void evt_mode_changed_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

__weak void evt_sta_connected_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

__weak void evt_sta_disconnected_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

__weak void evt_ap_client_join_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

__weak void evt_ap_client_leave_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

__weak void evt_webserver_up_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

__weak void evt_webserver_client_conn_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

__weak void evt_webserver_client_disconn_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

__weak void evt_lastwifi_notfound_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

__weak void evt_app_mpu_readings_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

__weak void evt_app_tcrt_readings_handler(const uint8_t *payload, uint8_t length)
{
    (void)payload;
    (void)length;
}

// Command metadata table (flash, no malloc).
const UNER_CmdMeta uner_cmd_table[] = {
    // Commands (host -> ESP)
    { 0x10, UNER_FLAG_NEED_ACK | UNER_FLAG_NEED_RESP, 100, NULL, "SET_MODE_AP" },
    { 0x11, UNER_FLAG_NEED_ACK | UNER_FLAG_NEED_RESP, 300, NULL, "SET_MODE_STA" },
    { 0x12, UNER_FLAG_NEED_ACK | UNER_FLAG_NEED_RESP, 100, NULL, "SET_CREDENTIALS" },
    { 0x13, UNER_FLAG_NEED_ACK | UNER_FLAG_NEED_RESP, 100, NULL, "CLEAR_CREDENTIALS" },
    { 0x14, UNER_FLAG_NEED_ACK | UNER_FLAG_NEED_RESP, 200, NULL, "START_SCAN" },
    { 0x15, UNER_FLAG_NEED_RESP, 100, NULL, "GET_SCAN_RESULTS" },
    { 0x16, UNER_FLAG_NEED_ACK, 50, NULL, "REBOOT_ESP" },
    { 0x17, UNER_FLAG_NEED_ACK | UNER_FLAG_NEED_RESP, 200, NULL, "FACTORY_RESET" },
    { 0x30, UNER_FLAG_NEED_RESP, 100, NULL, "GET_STATUS" },
    { 0x31, UNER_FLAG_NEED_RESP, 50, NULL, "PING" },
    { 0x40, UNER_FLAG_NEED_ACK | UNER_FLAG_NEED_RESP, 100, NULL, "GET_PREFERENCES" },
    { 0xE0, 0u, 0u, NULL, "ACK" },
    { 0xE1, 0u, 0u, NULL, "NACK" },

    // Events (ESP -> host)
    { 0x80, UNER_FLAG_IS_EVENT, 0u, evt_boot_handler, "EVT_BOOT" },
    { 0x81, UNER_FLAG_NEED_ACK | UNER_FLAG_IS_EVENT, 50u, evt_mode_changed_handler, "EVT_MODE_CHANGED" },
    { 0x82, UNER_FLAG_IS_EVENT, 0u, evt_sta_connected_handler, "EVT_STA_CONNECTED" },
    { 0x83, UNER_FLAG_IS_EVENT, 0u, evt_sta_disconnected_handler, "EVT_STA_DISCONNECTED" },
    { 0x84, UNER_FLAG_IS_EVENT, 0u, evt_ap_client_join_handler, "EVT_AP_CLIENT_JOIN" },
    { 0x85, UNER_FLAG_IS_EVENT, 0u, evt_ap_client_leave_handler, "EVT_AP_CLIENT_LEAVE" },
    { 0x88, UNER_FLAG_IS_EVENT, 0u, evt_webserver_up_handler, "EVT_WEBSERVER_UP" },
    { 0x89, UNER_FLAG_IS_EVENT, 0u, evt_webserver_client_conn_handler, "EVT_WEBSERVER_CLIENT_CONNECTED" },
    { 0x8A, UNER_FLAG_IS_EVENT, 0u, evt_webserver_client_disconn_handler, "EVT_WEBSERVER_CLIENT_DISCONNECTED" },
    { 0x8B, UNER_FLAG_NEED_ACK | UNER_FLAG_IS_EVENT, 50u, evt_lastwifi_notfound_handler, "EVT_LASTWIFI_NOTFOUND" },
    { 0x90, UNER_FLAG_NEED_ACK | UNER_FLAG_IS_EVENT, 50u, evt_app_mpu_readings_handler, "EVT_APP_GET_MPU_READINGS" },
    { 0x91, UNER_FLAG_NEED_ACK | UNER_FLAG_IS_EVENT, 50u, evt_app_tcrt_readings_handler, "EVT_APP_GET_TCRT_READINGS" },
};

const uint16_t uner_cmd_table_count = (uint16_t)(sizeof(uner_cmd_table) / sizeof(uner_cmd_table[0]));
