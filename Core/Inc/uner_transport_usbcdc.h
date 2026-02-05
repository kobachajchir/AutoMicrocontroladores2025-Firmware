/*
 * uner_transport_usbcdc.h - UNER USB CDC transport (stub)
 */
#ifndef INC_UNER_TRANSPORT_USBCDC_H_
#define INC_UNER_TRANSPORT_USBCDC_H_

#include <stdint.h>
#include "uner_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    UNER_Transport base;
} UNER_TransportUsbCdc;

UNER_Status UNER_TransportUsbCdc_Init(UNER_TransportUsbCdc *transport, uint8_t transport_id);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_TRANSPORT_USBCDC_H_ */
