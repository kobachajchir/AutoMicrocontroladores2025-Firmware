/*
 * uner_cmd_meta.h - UNER command metadata table
 */
#ifndef INC_UNER_CMD_META_H_
#define INC_UNER_CMD_META_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UNER_FLAG_NEED_ACK   0x01u
#define UNER_FLAG_NEED_RESP  0x02u
#define UNER_FLAG_IS_EVENT   0x04u

typedef void (*UNER_EventHandler)(const uint8_t *payload, uint8_t length);

typedef struct {
    uint8_t cmd_id;
    uint8_t flags;
    uint16_t timeout_ticks;
    UNER_EventHandler handler;
    const char *name;
} UNER_CmdMeta;

extern const UNER_CmdMeta uner_cmd_table[];
extern const uint16_t uner_cmd_table_count;

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_CMD_META_H_ */
