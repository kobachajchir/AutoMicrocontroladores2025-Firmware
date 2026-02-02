/*
 * uner_v2.h
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#ifndef INC_UNER_V2_H_
#define INC_UNER_V2_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UNER_HEADER_LEN 4U
#define UNER_TOKEN ':'
#define UNER_VERSION 0x02U

#define UNER_OFF_HEADER 0U
#define UNER_OFF_LEN 4U
#define UNER_OFF_TOKEN 5U
#define UNER_OFF_VER 6U
#define UNER_OFF_ROUTE 7U
#define UNER_OFF_CMD 8U
#define UNER_OFF_PAYLOAD 9U

#define UNER_OVERHEAD 10U
#define UNER_MAX_PAYLOAD 255U

#define UNER_ROUTE_SRC(route) (((route) >> 4) & 0x0FU)
#define UNER_ROUTE_DST(route) ((route) & 0x0FU)
#define UNER_ROUTE_MAKE(src, dst) ((((src) & 0x0FU) << 4) | ((dst) & 0x0FU))

typedef enum {
    UNER_NODE_MCU = 0x01U,
    UNER_NODE_PC_QT = 0x02U,
    UNER_NODE_WEB_APP = 0x03U,
    UNER_NODE_REMOTE_NRF = 0x04U,
    UNER_NODE_BROADCAST = 0x0FU
} UNER_NodeId;

typedef enum {
    UNER_STATUS_OK = 0,
    UNER_STATUS_ERR_NULL = -1,
    UNER_STATUS_ERR_LEN = -2,
    UNER_STATUS_ERR_BUFFER = -3,
    UNER_STATUS_ERR_BUSY = -4,
    UNER_STATUS_ERR_TRANSPORT = -5,
    UNER_STATUS_ERR_CHECKSUM = -6,
    UNER_STATUS_ERR_FORMAT = -7
} UNER_Status;

typedef enum {
    UNER_STATE_H0 = 0,
    UNER_STATE_H1,
    UNER_STATE_H2,
    UNER_STATE_H3,
    UNER_STATE_LEN,
    UNER_STATE_TOKEN,
    UNER_STATE_VER,
    UNER_STATE_ROUTE,
    UNER_STATE_CMD,
    UNER_STATE_PAYLOAD,
    UNER_STATE_CHK
} UNER_ParserState;

typedef struct {
    uint8_t src;
    uint8_t dst;
    uint8_t route;
    uint8_t cmd;
    uint8_t payload_len;
    uint8_t *payload;
    uint8_t transport_id;
} UNER_Packet;

typedef struct {
    uint32_t ok;
    uint32_t chk_fail;
    uint32_t len_fail;
    uint32_t token_fail;
    uint32_t ver_fail;
    uint32_t header_resync;
    uint32_t queue_overflow;
} UNER_CoreStats;

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_V2_H_ */
