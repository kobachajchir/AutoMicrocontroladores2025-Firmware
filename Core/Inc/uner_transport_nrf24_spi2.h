/*
 * uner_transport_nrf24_spi2.h
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#ifndef INC_UNER_TRANSPORT_NRF24_SPI2_H_
#define INC_UNER_TRANSPORT_NRF24_SPI2_H_

#include "uner_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool (*rx_ready)(void *user_ctx);
    bool (*read_payload)(void *user_ctx, uint8_t *buf, uint8_t *len);
    bool (*send_payload)(void *user_ctx, const uint8_t *buf, uint8_t len);
    void *user_ctx;
} UNER_TransportNrf24;

void UNER_TransportNrf24_Init(UNER_Transport *transport, UNER_TransportNrf24 *context);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_TRANSPORT_NRF24_SPI2_H_ */
