/*
 * uner_protocol.h
 *
 *  Created on: Jul 28, 2025
 *      Author: kobac
 */

#ifndef INC_UNER_PROTOCOL_H_
#define INC_UNER_PROTOCOL_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USART1_RX_DMA_BUF_LEN 32

// Tamaños máximos
#define UNER_PCK_MAX_PAYLOAD     30
#define UNER_PCK_OVERHEAD        7   // Header (5) + Len + CMD + CHK
#define UNER_PCK_MAX_TOTAL      (UNER_PCK_MAX_PAYLOAD + UNER_PCK_OVERHEAD)

// Header esperado: 'U', 'N', 'E', 'R', ':'
#define UNER_HEADER_LENGTH       5
#define UNER_HEADER_TARGET_SUM  1076

// Errores y estados
typedef int8_t UNERProtocolStatus;

#define UNER_OK                         0
#define UNER_ERR_NULL_PTR             -1
#define UNER_ERR_PAYLOAD_TOO_LONG     -2
#define UNER_ERR_BUFFER_TOO_SMALL     -3
#define UNER_ERR_INVALID_LENGTH       -4
#define UNER_ERR_HEADER_MISMATCH      -5
#define UNER_ERR_CHECKSUM_MISMATCH    -6
#define UNER_ERR_BUFFER_OVERFLOW      -7
#define UNER_ERR_TX_FAIL  -8

// Comando y datos recibidos/enviados
typedef struct {
    uint8_t cmd;
    uint8_t payload[UNER_PCK_MAX_PAYLOAD];
    uint8_t payload_len;
    uint8_t cks;
} UNERProtocolPck;

// Máquina de estados del parser
typedef enum {
    UNER_STATE_WAIT_HEADER = 0,
    UNER_STATE_WAIT_LEN,
    UNER_STATE_WAIT_CMD,
    UNER_STATE_WAIT_PAYLOAD,
    UNER_STATE_WAIT_CHK
} UNERParserStateEnum;

// Estructura del parser (librería reutilizable)
typedef struct {
    // Buffer circular de recepción
    uint8_t buffer[UNER_PCK_MAX_TOTAL];
    uint16_t head;
    uint16_t tail;

    // Máquina de estados
    UNERParserStateEnum state;
    uint8_t header_buf[UNER_HEADER_LENGTH];
    uint8_t header_index;

    // Recepción temporal
    uint8_t len;
    uint8_t cmd;
    uint8_t payload_index;
    uint8_t checksum;

    // Resultado final
    UNERProtocolPck receivePck;
    bool packet_ready;

    // Callback cuando se recibe paquete válido
    void (*onPacketReady)(UNERProtocolPck *pck);

    // Función HAL para enviar bytes (opcional)
    UNERProtocolStatus (*send_bytes)(const uint8_t *data, uint16_t len);
} UNERProtocolParserState;

// Inicializa estructura
void uner_init(UNERProtocolParserState *parser);

// Inserta un byte crudo en el buffer circular
UNERProtocolStatus uner_push_byte(UNERProtocolParserState *parser, uint8_t byte);

// Procesa el buffer de entrada byte a byte
UNERProtocolStatus uner_parse(UNERProtocolParserState *parser);

// Construye un paquete UNER binario a partir de estructura
UNERProtocolStatus uner_build_packet(const UNERProtocolPck *pck, uint8_t *out_buf, size_t out_buf_size);

// Envia un paquete (si send_bytes está definido)
UNERProtocolStatus uner_send_packet(UNERProtocolParserState *parser, const UNERProtocolPck *pck);

// Acceso al último paquete recibido válido
bool uner_packet_ready(const UNERProtocolParserState *parser);
void uner_clear_flag(UNERProtocolParserState *parser);
const UNERProtocolPck *uner_get_last_packet(const UNERProtocolParserState *parser);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_PROTOCOL_H_ */
