/*
 * uner_transport.h - UNER v2 transport interface
 */
#ifndef INC_UNER_TRANSPORT_H_
#define INC_UNER_TRANSPORT_H_

#include <stdint.h>
#include "uner_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UNER_Transport UNER_Transport;

typedef UNER_Status (*UNER_TransportPollRxFn)(UNER_Transport *transport, UNER_Core *core);
typedef UNER_Status (*UNER_TransportTrySendFn)(UNER_Transport *transport, const uint8_t *buf, uint16_t len);

struct UNER_Transport {
    uint8_t id;
    uint8_t max_payload;
    UNER_TransportPollRxFn poll_rx;
    UNER_TransportTrySendFn try_send;
    void *ctx;
};

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_TRANSPORT_H_ */
