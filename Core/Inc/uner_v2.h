/*
 * uner_v2.h - UNER v2 frame definitions
 */
#ifndef INC_UNER_V2_H_
#define INC_UNER_V2_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Frame layout (UNER v2) */
#define UNER_HDR_LEN           4u
#define UNER_OVERHEAD          10u
#define UNER_OFF_LEN           4u
#define UNER_OFF_TOKEN         5u
#define UNER_OFF_VER           6u
#define UNER_OFF_ROUTE         7u
#define UNER_OFF_CMD           8u
#define UNER_OFF_PAYLOAD       9u
#define UNER_MIN_FRAME         10u

#define UNER_TOKEN             ((uint8_t)':')
#define UNER_VERSION           0x02u
#define UNER_CMD_ACK           0xE0u
#define UNER_CMD_NACK          0xE1u

/* Node IDs (nibbles) */
#define UNER_NODE_MCU           0x1u
#define UNER_NODE_PC_QT         0x2u
#define UNER_NODE_WEB_APP       0x3u
#define UNER_NODE_REMOTE_NRF    0x4u
#define UNER_NODE_BROADCAST     0xFu

/* Transport IDs */
typedef enum {
    UNER_TR_UART1_ESP = 0,
    UNER_TR_NRF24_SPI2 = 1,
    UNER_TR_USB_CDC = 2,
} UNER_TransportId;

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_V2_H_ */
