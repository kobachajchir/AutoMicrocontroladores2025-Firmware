/*
 * uner_frame.c
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#include "uner_frame.h"

UNER_Status UNER_BuildFrame(uint8_t *out_buf,
                            uint16_t out_max,
                            uint8_t src,
                            uint8_t dst,
                            uint8_t cmd,
                            const uint8_t *payload,
                            uint8_t payload_len,
                            uint16_t *out_len)
{
    if (!out_buf || !out_len) {
        return UNER_STATUS_ERR_NULL;
    }
    if (payload_len > UNER_MAX_PAYLOAD) {
        return UNER_STATUS_ERR_LEN;
    }
    uint16_t total_len = (uint16_t)(UNER_OVERHEAD + payload_len);
    if (out_max < total_len) {
        return UNER_STATUS_ERR_BUFFER;
    }

    uint8_t chk = 0;
    out_buf[0] = 'U';
    out_buf[1] = 'N';
    out_buf[2] = 'E';
    out_buf[3] = 'R';
    out_buf[4] = payload_len;
    out_buf[5] = UNER_TOKEN;
    out_buf[6] = UNER_VERSION;
    out_buf[7] = UNER_ROUTE_MAKE(src, dst);
    out_buf[8] = cmd;

    for (uint8_t i = 0; i < 9; ++i) {
        chk ^= out_buf[i];
    }

    for (uint8_t i = 0; i < payload_len; ++i) {
        uint8_t value = payload ? payload[i] : 0U;
        out_buf[9 + i] = value;
        chk ^= value;
    }

    out_buf[9 + payload_len] = chk;
    *out_len = total_len;
    return UNER_STATUS_OK;
}
