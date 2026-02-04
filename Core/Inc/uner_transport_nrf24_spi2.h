/*
 * uner_transport_nrf24_spi2.h - UNER nRF24 transport (SPI2)
 */
#ifndef INC_UNER_TRANSPORT_NRF24_SPI2_H_
#define INC_UNER_TRANSPORT_NRF24_SPI2_H_

#include <stdint.h>
#include "uner_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t (*UNER_NrfIsRxReadyFn)(void *ctx);
typedef uint8_t (*UNER_NrfReadPayloadFn)(void *ctx, uint8_t *out, uint8_t max_len);
typedef uint8_t (*UNER_NrfSendPayloadFn)(void *ctx, const uint8_t *data, uint8_t len);

typedef struct {
    UNER_Transport base;
    uint8_t *rx_buf;
    uint8_t rx_buf_len;
    volatile uint8_t *irq_pending;
    UNER_NrfIsRxReadyFn is_rx_ready;
    UNER_NrfReadPayloadFn read_payload;
    UNER_NrfSendPayloadFn send_payload;
    void *driver_ctx;
} UNER_TransportNrf24;

UNER_Status UNER_TransportNrf24_Init(
    UNER_TransportNrf24 *transport,
    uint8_t transport_id,
    uint8_t max_payload,
    uint8_t *rx_buf,
    uint8_t rx_buf_len,
    volatile uint8_t *irq_pending,
    UNER_NrfIsRxReadyFn is_rx_ready,
    UNER_NrfReadPayloadFn read_payload,
    UNER_NrfSendPayloadFn send_payload,
    void *driver_ctx);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_TRANSPORT_NRF24_SPI2_H_ */
