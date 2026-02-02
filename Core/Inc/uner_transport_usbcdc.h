/*
 * uner_transport_usbcdc.h
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#ifndef INC_UNER_TRANSPORT_USBCDC_H_
#define INC_UNER_TRANSPORT_USBCDC_H_

#include "uner_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t dummy;
} UNER_TransportUsbCdc;

void UNER_TransportUsbCdc_Init(UNER_Transport *transport, UNER_TransportUsbCdc *context);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_TRANSPORT_USBCDC_H_ */
