/*
 * uner_transport.h
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#ifndef INC_UNER_TRANSPORT_H_
#define INC_UNER_TRANSPORT_H_

#include "uner_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UNER_Transport UNER_Transport;

typedef void (*UNER_TransportPoll)(UNER_Transport *transport, UNER_Core *core);
typedef UNER_Status (*UNER_TransportSend)(UNER_Transport *transport, const uint8_t *buf, uint16_t len);

struct UNER_Transport {
    uint8_t id;
    uint8_t max_payload;
    void *context;
    UNER_TransportPoll poll_rx;
    UNER_TransportSend try_send;
};

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_TRANSPORT_H_ */
