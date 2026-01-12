/*
 * uner_protocol.h  — UNER Protocol (portable, sin malloc)
 * Compatibilidad con tu API y tipos originales + correcciones de protocolo.
 */

#ifndef INC_UNER_PROTOCOL_H_
#define INC_UNER_PROTOCOL_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================== Opciones de compatibilidad ================== */
/* Si alguna vez emitiste "UNER:" como header de 5 bytes, habilitá esto.
 * Si está en 0 (por defecto), se usa el header correcto de 4 bytes "UNER"
 * y el TOKEN ':' va DESPUÉS del Length, como dicta la especificación PDF. */
#ifndef UNER_ENABLE_LEGACY_HEADER_UNER_COLON
#define UNER_ENABLE_LEGACY_HEADER_UNER_COLON 0
#endif

/* ================== Parámetros generales ================== */

#define USART1_RX_DMA_BUF_LEN        32

/* Máximos del protocolo */
#define UNER_PCK_MAX_PAYLOAD         30

/* Overhead CORRECTO según PDF:
 * 4(HEADER) + 1(LEN) + 1(TOKEN) + 1(CMD) + 1(CHK) = 8 */
#define UNER_PCK_OVERHEAD            8
#define UNER_PCK_MAX_TOTAL          (UNER_PCK_OVERHEAD + UNER_PCK_MAX_PAYLOAD)  /* 38 */

/* Header según PDF */
#define UNER_HEADER_LENGTH_CORRECT   4  /* "UNER" */
#define UNER_TOKEN                   ((uint8_t)':')

/* Para legacy ("UNER:") si se activa: */
#define UNER_HEADER_LENGTH_LEGACY    5  /* "UNER:" */

/* Elegir largo de header efectivo segun compatibilidad */
#if UNER_ENABLE_LEGACY_HEADER_UNER_COLON
  #define UNER_HEADER_LENGTH         UNER_HEADER_LENGTH_LEGACY
#else
  #define UNER_HEADER_LENGTH         UNER_HEADER_LENGTH_CORRECT
#endif

/* Rango de Length segun PDF: Length = 1(CMD) + N(PAY) + 1(CHK) => [2..32] */
#define UNER_LENGTH_MIN              2u
#define UNER_LENGTH_MAX              (1u + UNER_PCK_MAX_PAYLOAD + 1u)  /* 32 */

/* Un ring decente para RX (>=2x paquete máx) */
#ifndef UNER_RX_RING_CAPACITY
#define UNER_RX_RING_CAPACITY        128u
#endif

/* ============== Errores y estados (se respeta tu convención) ============== */
typedef int8_t UNERProtocolStatus;

#define UNER_OK                         0
#define UNER_ERR_NULL_PTR             -1
#define UNER_ERR_PAYLOAD_TOO_LONG     -2
#define UNER_ERR_BUFFER_TOO_SMALL     -3
#define UNER_ERR_INVALID_LENGTH       -4
#define UNER_ERR_HEADER_MISMATCH      -5
#define UNER_ERR_TOKEN_MISMATCH       -9
#define UNER_ERR_CHECKSUM_MISMATCH    -6
#define UNER_ERR_BUFFER_OVERFLOW      -7
#define UNER_ERR_TX_FAIL              -8
#define UNER_NO_PACKET                 2   /* adicional para polling */
#define UNER_IN_PROGRESS               1

/* ============== Estructuras (se respetan tus nombres) ============== */

/* Comando y datos recibidos/enviados */
typedef struct {
    uint8_t cmd;
    uint8_t payload[UNER_PCK_MAX_PAYLOAD];
    uint8_t payload_len;
    uint8_t cks;   /* checksum recibido (diagnóstico) */
} UNERProtocolPck;

/* Máquina de estados del parser */
typedef enum {
    UNER_STATE_WAIT_HEADER = 0,
    UNER_STATE_WAIT_LEN,
    UNER_STATE_WAIT_TOKEN,
    UNER_STATE_WAIT_CMD,
    UNER_STATE_WAIT_PAYLOAD,
    UNER_STATE_WAIT_CHK
} UNERParserStateEnum;

/* Estructura del parser (librería reutilizable) */
typedef struct {
    /* Ring buffer RX */
    uint8_t  buffer[UNER_RX_RING_CAPACITY];
    uint16_t head;
    uint16_t tail;

    /* Máquina de estados */
    UNERParserStateEnum state;

    /* Para detectar "UNER" progresivamente (sin sumas ambiguas) */
    uint8_t header_progress;   /* 0..(UNER_HEADER_LENGTH_CORRECT-1) */
    uint8_t cks_running;       /* XOR desde HEADER hasta último payload */

    /* Campos en curso */
    uint8_t len;               /* Length bruto leído del stream */
    uint8_t expected_payload;  /* N = len - 2 (CMD y CHK) */
    uint8_t cmd;
    uint8_t payload_index;

    /* Paquete final */
    UNERProtocolPck receivePck;
    volatile bool packet_ready;

    /* Callbacks */
    void (*onPacketReady)(UNERProtocolPck *pck);

    /* HAL TX (opcional). Devuelve UNER_OK o error */
    UNERProtocolStatus (*send_bytes)(const uint8_t *data, uint16_t len);
} UNERProtocolParserState;

/* ================== API ================== */

/* Inicializa estructura */
void uner_init(UNERProtocolParserState *parser);

/* Inserta un byte crudo en el ring */
UNERProtocolStatus uner_push_byte(UNERProtocolParserState *parser, uint8_t byte);

/* Procesa el ring hasta formar paquetes */
UNERProtocolStatus uner_parse(UNERProtocolParserState *parser);

/* Construye un paquete UNER binario a partir de estructura
 * Retorna longitud total (>=UNER_PCK_OVERHEAD) o <0 si error */
UNERProtocolStatus uner_build_packet(const UNERProtocolPck *pck, uint8_t *out_buf, size_t out_buf_size);

/* Envía un paquete (si send_bytes está definido) */
UNERProtocolStatus uner_send_packet(UNERProtocolParserState *parser, const UNERProtocolPck *pck);

/* Polling del último paquete válido */
bool uner_packet_ready(const UNERProtocolParserState *parser);
void uner_clear_flag(UNERProtocolParserState *parser);
const UNERProtocolPck *uner_get_last_packet(const UNERProtocolParserState *parser);

/* Herramienta: checksum XOR (header..payload) */
uint8_t uner_calc_checksum(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_PROTOCOL_H_ */

