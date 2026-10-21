/*
 * wifi_bridge_at.c - non-blocking ESP-01 AT backend using UART DMA.
 */
#include "wifi_bridge_at.h"
#include "wifi_bridge.h"

#if !defined(WIFI_BRIDGE_USE_ESP_AT)
#error "wifi_bridge_at.c requires WIFI_BRIDGE_USE_ESP_AT"
#endif

#include <string.h>

#include "globals.h"
#include "screenWrappers.h"

static size_t WifiBridge_AT_StrLenBounded(const char *text, size_t max_len)
{
    size_t len = 0u;

    if (text == NULL) {
        return 0u;
    }

    while (text[len] != '\0' && len < max_len) {
        len++;
    }

    return len;
}

static void WifiBridge_AT_CopyText(char *dst, size_t dst_len, const char *src)
{
    size_t i = 0u;

    if ((dst == NULL) || (dst_len == 0u)) {
        return;
    }

    if (src != NULL) {
        while (src[i] != '\0' && i + 1u < dst_len) {
            dst[i] = src[i];
            i++;
        }
    }

    dst[i] = '\0';
}

static uint8_t WifiBridge_AT_AppendText(char *buf, size_t buf_len, uint16_t *pos, const char *text)
{
    if ((buf == NULL) || (pos == NULL) || (text == NULL)) {
        return 0u;
    }

    while (*text != '\0') {
        if ((*pos + 1u) >= buf_len) {
            return 0u;
        }
        buf[*pos] = *text;
        *pos = (uint16_t)(*pos + 1u);
        text++;
    }

    buf[*pos] = '\0';
    return 1u;
}

static uint8_t WifiBridge_AT_AppendQuoted(char *buf, size_t buf_len, uint16_t *pos, const char *text)
{
    if (!WifiBridge_AT_AppendText(buf, buf_len, pos, "\"")) {
        return 0u;
    }

    if (text != NULL) {
        while (*text != '\0') {
            if ((*text == '"') || (*text == '\\')) {
                return 0u;
            }
            if ((*pos + 1u) >= buf_len) {
                return 0u;
            }
            buf[*pos] = *text;
            *pos = (uint16_t)(*pos + 1u);
            text++;
        }
        buf[*pos] = '\0';
    }

    return WifiBridge_AT_AppendText(buf, buf_len, pos, "\"");
}

static int16_t WifiBridge_AT_ParseSigned(const char **cursor)
{
    int16_t value = 0;
    int16_t sign = 1;
    const char *p;

    if ((cursor == NULL) || (*cursor == NULL)) {
        return 0;
    }

    p = *cursor;
    if (*p == '-') {
        sign = -1;
        p++;
    }

    while (*p >= '0' && *p <= '9') {
        value = (int16_t)((value * 10) + (*p - '0'));
        p++;
    }

    *cursor = p;
    return (int16_t)(value * sign);
}

static uint8_t WifiBridge_AT_ParseIp(const char *line, uint8_t ip[WIFI_BRIDGE_IP_LEN])
{
    const char *p = line;

    if ((line == NULL) || (ip == NULL)) {
        return 0u;
    }

    while (*p != '\0' && (*p < '0' || *p > '9')) {
        p++;
    }

    for (uint8_t i = 0u; i < WIFI_BRIDGE_IP_LEN; ++i) {
        int16_t part = WifiBridge_AT_ParseSigned(&p);

        if (part < 0 || part > 255) {
            return 0u;
        }
        ip[i] = (uint8_t)part;

        if (i < (WIFI_BRIDGE_IP_LEN - 1u)) {
            if (*p != '.') {
                return 0u;
            }
            p++;
        }
    }

    return 1u;
}

static const char *WifiBridge_AT_EncryptionText(uint8_t enc)
{
    switch (enc) {
        case 0u: return "OPEN";
        case 1u: return "WEP";
        case 2u: return "WPA";
        case 3u: return "WPA2";
        case 4u: return "WPA/WPA2";
        default: return "ENC?";
    }
}

static int8_t WifiBridge_AT_FindNetwork(const char *ssid)
{
    if (ssid == NULL) {
        return -1;
    }

    for (uint8_t i = 0u; i < networksFound && i < WIFI_SCAN_MAX_NETWORKS; ++i) {
        if (strncmp(wifiNetworkSsids[i], ssid, WIFI_SSID_MAX_LEN) == 0) {
            return (int8_t)i;
        }
    }

    return -1;
}

static void WifiBridge_AT_StoreScanResult(const char *ssid,
                                          uint8_t enc,
                                          int8_t rssi,
                                          uint8_t channel)
{
    int8_t existing;
    uint8_t index;

    if ((ssid == NULL) || (ssid[0] == '\0')) {
        return;
    }

    existing = WifiBridge_AT_FindNetwork(ssid);
    if (existing >= 0) {
        index = (uint8_t)existing;
    } else {
        if (networksFound >= WIFI_SCAN_MAX_NETWORKS) {
            return;
        }
        index = networksFound++;
    }

    WifiBridge_AT_CopyText(wifiNetworkSsids[index],
                           sizeof(wifiNetworkSsids[index]),
                           ssid);
    WifiBridge_AT_CopyText(wifiNetworkEncryptions[index],
                           sizeof(wifiNetworkEncryptions[index]),
                           WifiBridge_AT_EncryptionText(enc));
    wifiNetworkSignalStrengths[index] = rssi;
    wifiNetworkChannels[index] = channel;
    wifiNetworkEncryptionTypes[index] = enc;
    wifiNetworkDetailValid[index] = 1u;
}

static void WifiBridge_AT_ParseCwlap(const char *line)
{
    const char *p;
    const char *q1;
    const char *q2;
    char ssid[WIFI_BRIDGE_SSID_MAX_LEN + 1u];
    uint8_t enc;
    int8_t rssi;
    uint8_t channel = 0u;
    size_t ssid_len;

    p = strchr(line, '(');
    if (p == NULL) {
        return;
    }
    p++;

    enc = (uint8_t)WifiBridge_AT_ParseSigned(&p);
    q1 = strchr(p, '"');
    if (q1 == NULL) {
        return;
    }
    q2 = strchr(q1 + 1, '"');
    if (q2 == NULL) {
        return;
    }

    ssid_len = (size_t)(q2 - q1 - 1);
    if (ssid_len > WIFI_BRIDGE_SSID_MAX_LEN) {
        ssid_len = WIFI_BRIDGE_SSID_MAX_LEN;
    }
    memcpy(ssid, q1 + 1, ssid_len);
    ssid[ssid_len] = '\0';

    p = q2 + 1;
    if (*p == ',') {
        p++;
    }
    rssi = (int8_t)WifiBridge_AT_ParseSigned(&p);

    q1 = strchr(p, '"');
    if (q1 != NULL) {
        q2 = strchr(q1 + 1, '"');
        if (q2 != NULL) {
            p = q2 + 1;
            if (*p == ',') {
                p++;
                channel = (uint8_t)WifiBridge_AT_ParseSigned(&p);
            }
        }
    }

    WifiBridge_AT_StoreScanResult(ssid, enc, rssi, channel);
}

static void WifiBridge_AT_SetStaIp(const uint8_t ip[WIFI_BRIDGE_IP_LEN])
{
    espStaIp.block1 = ip[0];
    espStaIp.block2 = ip[1];
    espStaIp.block3 = ip[2];
    espStaIp.block4 = ip[3];
    espWifiConnection.staIp.block1 = ip[0];
    espWifiConnection.staIp.block2 = ip[1];
    espWifiConnection.staIp.block3 = ip[2];
    espWifiConnection.staIp.block4 = ip[3];
    espWifiConnection.staIpValid = 1u;
}

static void WifiBridge_AT_Emit(WifiBridge_Handle *h, WifiBridge_EventId id, uint8_t status)
{
    WifiBridge_Event evt;

    if (h == NULL) {
        return;
    }

    memset(&evt, 0, sizeof(evt));
    evt.id = id;
    evt.mode = h->mode;
    evt.status = status;
    evt.connected = WIFI_FLAG_IS_SET(h, WIFI_FLAG_CONNECTED) ? 1u : 0u;
    evt.got_ip = WIFI_FLAG_IS_SET(h, WIFI_FLAG_GOT_IP) ? 1u : 0u;
    memcpy(evt.ip, h->ip, sizeof(evt.ip));
    WifiBridge_AT_CopyText(evt.ssid, sizeof(evt.ssid), h->ssid);
    WifiBridge_EmitEvent(h, &evt);
}

static void WifiBridge_AT_CommandDone(WifiBridge_Handle *h, uint8_t ok)
{
    if (h == NULL) {
        return;
    }

    WIFI_FLAG_CLEAR(h, WIFI_FLAG_BUSY);
    if (ok) {
        WIFI_FLAG_SET(h, WIFI_FLAG_GOT_OK);
        WIFI_FLAG_CLEAR(h, WIFI_FLAG_GOT_ERROR);
    } else {
        WIFI_FLAG_SET(h, WIFI_FLAG_GOT_ERROR);
        WifiBridge_AT_Emit(h, WIFI_BRIDGE_EVT_ERROR, h->status);
    }

    if (ok) {
        switch (h->at_active_op) {
            case WIFI_BRIDGE_AT_OP_SET_STA:
                h->mode = WIFI_BRIDGE_MODE_STA;
                WifiBridge_AT_Emit(h, WIFI_BRIDGE_EVT_MODE_CHANGED, 0u);
                break;
            case WIFI_BRIDGE_AT_OP_SET_AP:
                h->mode = WIFI_BRIDGE_MODE_AP;
                WifiBridge_AT_Emit(h, WIFI_BRIDGE_EVT_AP_STARTED, 0u);
                break;
            case WIFI_BRIDGE_AT_OP_DISCONNECT:
                WIFI_FLAG_CLEAR(h, WIFI_FLAG_CONNECTED);
                WIFI_FLAG_CLEAR(h, WIFI_FLAG_GOT_IP);
                WifiBridge_AT_Emit(h, WIFI_BRIDGE_EVT_STA_DISCONNECTED, 0u);
                break;
            case WIFI_BRIDGE_AT_OP_SCAN:
                wifiScanResultsPending = 1u;
                WifiBridge_AT_Emit(h, WIFI_BRIDGE_EVT_SCAN_DONE, h->at_scan_count);
                break;
            default:
                break;
        }
    }

    h->at_active_op = WIFI_BRIDGE_AT_OP_NONE;
}

static void WifiBridge_AT_ParseLine(WifiBridge_Handle *h, const char *line)
{
    if ((h == NULL) || (line == NULL) || (line[0] == '\0')) {
        return;
    }

    if (strcmp(line, "OK") == 0) {
        WifiBridge_AT_CommandDone(h, 1u);
    } else if ((strcmp(line, "ERROR") == 0) ||
               (strcmp(line, "FAIL") == 0) ||
               (strcmp(line, "SEND FAIL") == 0)) {
        WifiBridge_AT_CommandDone(h, 0u);
    } else if (strcmp(line, "ready") == 0) {
        SET_FLAG(systemFlags2, ESP_PRESENT);
        WifiBridge_AT_Emit(h, WIFI_BRIDGE_EVT_BOOT, 0u);
    } else if (strncmp(line, "busy p", 6u) == 0) {
        WIFI_FLAG_SET(h, WIFI_FLAG_BUSY);
    } else if (strcmp(line, "WIFI CONNECTED") == 0) {
        WIFI_FLAG_SET(h, WIFI_FLAG_CONNECTED);
        WifiBridge_AT_Emit(h, WIFI_BRIDGE_EVT_STA_CONNECTED, 0u);
    } else if (strcmp(line, "WIFI GOT IP") == 0) {
        WIFI_FLAG_SET(h, WIFI_FLAG_CONNECTED);
        WIFI_FLAG_SET(h, WIFI_FLAG_GOT_IP);
        WifiBridge_AT_Emit(h, WIFI_BRIDGE_EVT_GOT_IP, 0u);
    } else if (strcmp(line, "WIFI DISCONNECT") == 0) {
        WIFI_FLAG_CLEAR(h, WIFI_FLAG_CONNECTED);
        WIFI_FLAG_CLEAR(h, WIFI_FLAG_GOT_IP);
        WifiBridge_AT_Emit(h, WIFI_BRIDGE_EVT_STA_DISCONNECTED, 0u);
    } else if (strcmp(line, ">") == 0) {
        h->status = '>';
    } else if (strcmp(line, "SEND OK") == 0) {
        WIFI_FLAG_SET(h, WIFI_FLAG_GOT_OK);
    } else if (strncmp(line, "+CWLAP:", 7u) == 0) {
        WifiBridge_AT_ParseCwlap(line);
        h->at_scan_count = networksFound;
    } else if (strncmp(line, "+CIPSTATUS:", 11u) == 0) {
        const char *p = line + 11u;
        h->status = (uint8_t)WifiBridge_AT_ParseSigned(&p);
    } else if (strncmp(line, "+CIFSR:", 7u) == 0) {
        uint8_t ip[WIFI_BRIDGE_IP_LEN];

        if (WifiBridge_AT_ParseIp(line, ip)) {
            memcpy(h->ip, ip, sizeof(h->ip));
            WifiBridge_AT_SetStaIp(ip);
            WIFI_FLAG_SET(h, WIFI_FLAG_GOT_IP);
            WifiBridge_AT_Emit(h, WIFI_BRIDGE_EVT_GOT_IP, 0u);
        }
    } else if (strncmp(line, "+IPD,", 5u) == 0) {
        WIFI_FLAG_SET(h, WIFI_FLAG_RX_PENDING);
    }
}

static void WifiBridge_AT_ConsumeBytes(WifiBridge_Handle *h, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0u; i < len; ++i) {
        char ch = (char)data[i];

        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            h->at_line[h->at_line_len] = '\0';
            WifiBridge_AT_ParseLine(h, h->at_line);
            h->at_line_len = 0u;
            h->at_line[0] = '\0';
            continue;
        }

        if (h->at_line_len + 1u < WIFI_BRIDGE_AT_LINE_SIZE) {
            h->at_line[h->at_line_len++] = ch;
        } else {
            h->at_line_len = 0u;
            h->at_line[0] = '\0';
        }
    }
}

static WifiBridge_Result WifiBridge_AT_Send(WifiBridge_Handle *h,
                                            WifiBridge_AT_Operation op,
                                            const char *cmd)
{
    size_t len;

    if ((h == NULL) || (h->huart == NULL) || (cmd == NULL)) {
        return WIFI_BRIDGE_BAD_ARG;
    }

    if (WIFI_FLAG_IS_SET(h, WIFI_FLAG_TX_BUSY) || WIFI_FLAG_IS_SET(h, WIFI_FLAG_BUSY)) {
        return WIFI_BRIDGE_BUSY;
    }

    len = WifiBridge_AT_StrLenBounded(cmd, WIFI_BRIDGE_AT_TX_SIZE);
    if ((len == 0u) || (len >= WIFI_BRIDGE_AT_TX_SIZE)) {
        return WIFI_BRIDGE_BAD_ARG;
    }

    memcpy(h->at_tx_buf, cmd, len);
    h->at_tx_len = (uint16_t)len;
    h->at_active_op = op;
    h->at_deadline_ms = HAL_GetTick() + WIFI_BRIDGE_AT_TIMEOUT_MS;
    WIFI_FLAG_SET(h, WIFI_FLAG_BUSY);
    WIFI_FLAG_SET(h, WIFI_FLAG_TX_BUSY);
    WIFI_FLAG_CLEAR(h, WIFI_FLAG_GOT_OK);
    WIFI_FLAG_CLEAR(h, WIFI_FLAG_GOT_ERROR);

    if (HAL_UART_Transmit_DMA(h->huart, (uint8_t *)h->at_tx_buf, h->at_tx_len) != HAL_OK) {
        WIFI_FLAG_CLEAR(h, WIFI_FLAG_BUSY);
        WIFI_FLAG_CLEAR(h, WIFI_FLAG_TX_BUSY);
        h->at_active_op = WIFI_BRIDGE_AT_OP_NONE;
        return WIFI_BRIDGE_BUSY;
    }

    return WIFI_BRIDGE_OK;
}

static WifiBridge_Result WifiBridge_AT_SendJoin(WifiBridge_Handle *h)
{
    uint16_t pos = 0u;
    const WifiBridge_Credentials *credentials = WifiBridge_GetRuntimeCredentials();

    if ((h == NULL) ||
        (credentials == NULL) ||
        (credentials->flags.bits.ssid_set == 0u)) {
        return WIFI_BRIDGE_BAD_ARG;
    }

    WifiBridge_AT_CopyText(h->ssid, sizeof(h->ssid), credentials->ssid);

    h->at_tx_buf[0] = '\0';
    if (!WifiBridge_AT_AppendText(h->at_tx_buf, sizeof(h->at_tx_buf), &pos, "AT+CWJAP_CUR=") ||
        !WifiBridge_AT_AppendQuoted(h->at_tx_buf, sizeof(h->at_tx_buf), &pos, credentials->ssid) ||
        !WifiBridge_AT_AppendText(h->at_tx_buf, sizeof(h->at_tx_buf), &pos, ",") ||
        !WifiBridge_AT_AppendQuoted(h->at_tx_buf, sizeof(h->at_tx_buf), &pos, credentials->password) ||
        !WifiBridge_AT_AppendText(h->at_tx_buf, sizeof(h->at_tx_buf), &pos, "\r\n")) {
        return WIFI_BRIDGE_BAD_ARG;
    }

    return WifiBridge_AT_Send(h, WIFI_BRIDGE_AT_OP_CONNECT, h->at_tx_buf);
}

WifiBridge_Result WifiBridge_AT_Init(WifiBridge_Handle *h, UART_HandleTypeDef *huart)
{
    if (h == NULL) {
        return WIFI_BRIDGE_BAD_ARG;
    }

    memset(h, 0, sizeof(*h));
    h->huart = huart;
    return WIFI_BRIDGE_OK;
}

WifiBridge_Result WifiBridge_AT_Start(WifiBridge_Handle *h)
{
    if ((h == NULL) || (h->huart == NULL)) {
        return WIFI_BRIDGE_BAD_ARG;
    }

    if (HAL_UART_Receive_DMA(h->huart, h->at_rx_dma, WIFI_BRIDGE_AT_RX_DMA_SIZE) != HAL_OK) {
        return WIFI_BRIDGE_BUSY;
    }

    __HAL_UART_ENABLE_IT(h->huart, UART_IT_IDLE);
    WifiBridge_AT_ResetRxPos(h);
    return WIFI_BRIDGE_OK;
}

void WifiBridge_AT_Process(WifiBridge_Handle *h)
{
    if (h == NULL) {
        return;
    }

    if (WIFI_FLAG_IS_SET(h, WIFI_FLAG_BUSY) &&
        h->at_deadline_ms != 0u &&
        ((int32_t)(HAL_GetTick() - h->at_deadline_ms) >= 0)) {
        WIFI_FLAG_CLEAR(h, WIFI_FLAG_BUSY);
        WIFI_FLAG_CLEAR(h, WIFI_FLAG_TX_BUSY);
        WIFI_FLAG_SET(h, WIFI_FLAG_GOT_ERROR);
        h->at_active_op = WIFI_BRIDGE_AT_OP_NONE;
        WifiBridge_AT_Emit(h, WIFI_BRIDGE_EVT_ERROR, WIFI_BRIDGE_TIMEOUT);
    }
}

void WifiBridge_AT_RxCheck(WifiBridge_Handle *h)
{
    uint16_t pos;

    if ((h == NULL) || (h->huart == NULL) || (h->huart->hdmarx == NULL)) {
        return;
    }

    pos = (uint16_t)(WIFI_BRIDGE_AT_RX_DMA_SIZE -
                     __HAL_DMA_GET_COUNTER(h->huart->hdmarx));
    if (pos >= WIFI_BRIDGE_AT_RX_DMA_SIZE) {
        pos = 0u;
    }

    if (pos == h->at_rx_last_pos) {
        return;
    }

    if (pos > h->at_rx_last_pos) {
        WifiBridge_AT_ConsumeBytes(h,
                                   &h->at_rx_dma[h->at_rx_last_pos],
                                   (uint16_t)(pos - h->at_rx_last_pos));
    } else {
        WifiBridge_AT_ConsumeBytes(h,
                                   &h->at_rx_dma[h->at_rx_last_pos],
                                   (uint16_t)(WIFI_BRIDGE_AT_RX_DMA_SIZE - h->at_rx_last_pos));
        if (pos > 0u) {
            WifiBridge_AT_ConsumeBytes(h, h->at_rx_dma, pos);
        }
    }

    h->at_rx_last_pos = pos;
}

void WifiBridge_AT_OnTxComplete(WifiBridge_Handle *h)
{
    if (h != NULL) {
        WIFI_FLAG_CLEAR(h, WIFI_FLAG_TX_BUSY);
    }
}

void WifiBridge_AT_ResetRxPos(WifiBridge_Handle *h)
{
    if ((h == NULL) || (h->huart == NULL) || (h->huart->hdmarx == NULL)) {
        return;
    }

    h->at_rx_last_pos = (uint16_t)(WIFI_BRIDGE_AT_RX_DMA_SIZE -
                                   __HAL_DMA_GET_COUNTER(h->huart->hdmarx));
    if (h->at_rx_last_pos >= WIFI_BRIDGE_AT_RX_DMA_SIZE) {
        h->at_rx_last_pos = 0u;
    }
}

WifiBridge_Result WifiBridge_AT_SetModeSTA(WifiBridge_Handle *h)
{
    return WifiBridge_AT_Send(h, WIFI_BRIDGE_AT_OP_SET_STA, "AT+CWMODE_CUR=1\r\n");
}

WifiBridge_Result WifiBridge_AT_SetModeAP(WifiBridge_Handle *h)
{
    return WifiBridge_AT_Send(h, WIFI_BRIDGE_AT_OP_SET_AP, "AT+CWMODE_CUR=2\r\n");
}

WifiBridge_Result WifiBridge_AT_SetCredentials(WifiBridge_Handle *h,
                                               const char *ssid,
                                               const char *pass)
{
    if ((h == NULL) || (ssid == NULL) || (pass == NULL)) {
        return WIFI_BRIDGE_BAD_ARG;
    }

    WifiBridge_Result status = WifiBridge_SetRuntimeCredentials(ssid, pass);
    if (status == WIFI_BRIDGE_OK) {
        WifiBridge_AT_CopyText(h->ssid, sizeof(h->ssid), wifiBridgeCredentials.ssid);
    }

    return status;
}

WifiBridge_Result WifiBridge_AT_Connect(WifiBridge_Handle *h)
{
    return WifiBridge_AT_SendJoin(h);
}

WifiBridge_Result WifiBridge_AT_Disconnect(WifiBridge_Handle *h)
{
    return WifiBridge_AT_Send(h, WIFI_BRIDGE_AT_OP_DISCONNECT, "AT+CWQAP\r\n");
}

WifiBridge_Result WifiBridge_AT_StartScan(WifiBridge_Handle *h)
{
    WiFiScan_ClearResults();
    if (h != NULL) {
        h->at_scan_count = 0u;
    }
    return WifiBridge_AT_Send(h, WIFI_BRIDGE_AT_OP_SCAN, "AT+CWLAP\r\n");
}

WifiBridge_Result WifiBridge_AT_StopScan(WifiBridge_Handle *h)
{
    (void)h;
    return WIFI_BRIDGE_OK;
}

WifiBridge_Result WifiBridge_AT_GetScanResults(WifiBridge_Handle *h)
{
    return WifiBridge_AT_StartScan(h);
}

WifiBridge_Result WifiBridge_AT_GetStatus(WifiBridge_Handle *h)
{
    return WifiBridge_AT_Send(h, WIFI_BRIDGE_AT_OP_STATUS, "AT+CIPSTATUS\r\nAT+CIFSR\r\n");
}

WifiBridge_Result WifiBridge_AT_SendCommand(WifiBridge_Handle *h,
                                            uint8_t cmd,
                                            const uint8_t *payload,
                                            uint8_t len)
{
    (void)payload;
    (void)len;

    switch (cmd) {
        case WIFI_BRIDGE_CMD_ID_SET_MODE_AP:
            return WifiBridge_AT_SetModeAP(h);
        case WIFI_BRIDGE_CMD_ID_SET_MODE_STA:
            return WifiBridge_AT_SetModeSTA(h);
        case WIFI_BRIDGE_CMD_ID_START_SCAN:
        case WIFI_BRIDGE_CMD_ID_GET_SCAN_RESULTS:
            return WifiBridge_AT_StartScan(h);
        case WIFI_BRIDGE_CMD_ID_STOP_SCAN:
            return WifiBridge_AT_StopScan(h);
        case WIFI_BRIDGE_CMD_ID_GET_STATUS:
            return WifiBridge_AT_GetStatus(h);
        case WIFI_BRIDGE_CMD_ID_CONNECT_WIFI:
            return WifiBridge_AT_Connect(h);
        case WIFI_BRIDGE_CMD_ID_DISCONNECT_WIFI:
            return WifiBridge_AT_Disconnect(h);
        case WIFI_BRIDGE_CMD_ID_REBOOT_ESP:
            return WifiBridge_AT_Send(h, WIFI_BRIDGE_AT_OP_NONE, "AT+RST\r\n");
        case WIFI_BRIDGE_CMD_ID_REQUEST_FIRMWARE:
            return WifiBridge_AT_Send(h, WIFI_BRIDGE_AT_OP_NONE, "AT+GMR\r\n");
        case WIFI_BRIDGE_CMD_ID_PING:
            return WifiBridge_AT_Send(h, WIFI_BRIDGE_AT_OP_NONE, "AT\r\n");
        default:
            return WIFI_BRIDGE_UNSUPPORTED;
    }
}

WifiBridge_Result WifiBridge_AT_SendWifiCredentialRequest(WifiBridge_Handle *h,
                                                          const char *ssid)
{
    WifiBridge_Result status;

    if (h == NULL) {
        return WIFI_BRIDGE_BAD_ARG;
    }

    status = WifiBridge_SetRuntimeSsid(ssid);
    if (status != WIFI_BRIDGE_OK) {
        return status;
    }

    WifiBridge_AT_CopyText(h->ssid, sizeof(h->ssid), wifiBridgeCredentials.ssid);
    return WifiBridge_AT_Connect(h);
}

WifiBridge_Result WifiBridge_AT_RequestWifiNetworkDetail(WifiBridge_Handle *h,
                                                         const char *ssid)
{
    int8_t index;

    (void)h;
    index = WifiBridge_AT_FindNetwork(ssid);
    if (index < 0) {
        return WIFI_BRIDGE_UNSUPPORTED;
    }

    wifiNetworkDetailValid[(uint8_t)index] = 1u;
    return WIFI_BRIDGE_OK;
}

WifiBridge_Result WifiBridge_AT_SendEspReboot(WifiBridge_Handle *h, uint8_t boot_mode)
{
    (void)boot_mode;
    return WifiBridge_AT_Send(h, WIFI_BRIDGE_AT_OP_NONE, "AT+RST\r\n");
}

WifiBridge_Result WifiBridge_AT_ReportScreenChanged(WifiBridge_Handle *h,
                                                    uint32_t screen_code,
                                                    uint8_t source)
{
    (void)h;
    (void)screen_code;
    (void)source;
    return WIFI_BRIDGE_OK;
}

WifiBridge_Result WifiBridge_AT_ReportMenuSelectionChanged(WifiBridge_Handle *h,
                                                           uint32_t screen_code,
                                                           uint8_t selected_index,
                                                           uint8_t item_count,
                                                           uint8_t source)
{
    (void)h;
    (void)screen_code;
    (void)selected_index;
    (void)item_count;
    (void)source;
    return WIFI_BRIDGE_OK;
}

WifiBridge_Result WifiBridge_AT_ReportCarModeChanged(WifiBridge_Handle *h, uint8_t mode)
{
    (void)h;
    (void)mode;
    return WIFI_BRIDGE_OK;
}

WifiBridge_Result WifiBridge_AT_RequestBinaryAsset(WifiBridge_Handle *h, uint8_t asset_id)
{
    (void)h;
    (void)asset_id;
    return WIFI_BRIDGE_UNSUPPORTED;
}

uint8_t WifiBridge_AT_GetBinaryAsset(WifiBridge_Handle *h,
                                     uint8_t asset_id,
                                     WifiBridge_BinaryAssetView *asset)
{
    (void)h;
    (void)asset_id;
    (void)asset;
    return 0u;
}

uint8_t WifiBridge_AT_IsWaitingValidation(WifiBridge_Handle *h)
{
    (void)h;
    return 0u;
}

uint8_t WifiBridge_AT_GetWaitingCommandId(WifiBridge_Handle *h)
{
    (void)h;
    return 0u;
}
