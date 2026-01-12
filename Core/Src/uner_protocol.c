// uner_protocol.c
#include "uner_protocol.h"

/* ==================== Helpers: ring ==================== */

static inline uint16_t ring_count(const UNERProtocolParserState *p) {
    return (uint16_t)(p->head - p->tail);
}

static inline bool ring_push(UNERProtocolParserState *p, uint8_t b) {
    if (ring_count(p) >= (UNER_RX_RING_CAPACITY - 1u)) return false;
    p->buffer[p->head & (UNER_RX_RING_CAPACITY - 1u)] = b;
    p->head++;
    return true;
}

static inline bool ring_pop(UNERProtocolParserState *p, uint8_t *out) {
    if (p->head == p->tail) return false;
    *out = p->buffer[p->tail & (UNER_RX_RING_CAPACITY - 1u)];
    p->tail++;
    return true;
}

/* ==================== Reset de la máquina ==================== */

static void uner_reset_parser_state(UNERProtocolParserState *parser) {
    parser->state           = UNER_STATE_WAIT_HEADER;
    parser->header_progress = 0;
    parser->cks_running     = 0;
    parser->len             = 0;
    parser->expected_payload= 0;
    parser->cmd             = 0;
    parser->payload_index   = 0;
}

/* En re-sincronización, permite re-considerar el byte como posible 'U' */
static void uner_resync_and_consider_u(UNERProtocolParserState *parser, uint8_t current) {
    uner_reset_parser_state(parser);
    if (current == (uint8_t)'U') {
        parser->header_progress = 1;
        parser->cks_running     = (uint8_t)'U';
        parser->state           = UNER_STATE_WAIT_HEADER;
    }
}

/* ==================== API ==================== */

void uner_init(UNERProtocolParserState *parser) {
    if (!parser) return;
    parser->head = 0;
    parser->tail = 0;
    parser->packet_ready = false;
    parser->onPacketReady = NULL;
    parser->send_bytes = NULL;
    uner_reset_parser_state(parser);
}

UNERProtocolStatus uner_push_byte(UNERProtocolParserState *parser, uint8_t byte) {
    if (!parser) return UNER_ERR_NULL_PTR;
    if (!ring_push(parser, byte)) return UNER_ERR_BUFFER_OVERFLOW;
    return UNER_OK;
}

UNERProtocolStatus uner_parse(UNERProtocolParserState *parser) {
    if (!parser) return UNER_ERR_NULL_PTR;
    UNERProtocolStatus ret = UNER_NO_PACKET;

    uint8_t b;
    while (ring_pop(parser, &b)) {
        switch (parser->state) {

        case UNER_STATE_WAIT_HEADER: {
            /* Avance progresivo "U","N","E","R" (o "U","N","E","R",":", si legacy) */
#if UNER_ENABLE_LEGACY_HEADER_UNER_COLON
            const uint8_t hdr[UNER_HEADER_LENGTH_LEGACY] = {'U','N','E','R',':'};
#else
            const uint8_t hdr[UNER_HEADER_LENGTH_CORRECT]= {'U','N','E','R'};
#endif

            if (b == hdr[parser->header_progress]) {
                parser->cks_running ^= b;                 /* XOR desde HEADER */
                parser->header_progress++;
                if (parser->header_progress >= UNER_HEADER_LENGTH) {
#if UNER_ENABLE_LEGACY_HEADER_UNER_COLON
                    /* Header legacy ya incluye ':' => pasamos a LEN directamente */
                    parser->state = UNER_STATE_WAIT_LEN;
#else
                    /* Header correcto tiene 4 bytes => ahora esperamos LEN */
                    parser->state = UNER_STATE_WAIT_LEN;
#endif
                    parser->header_progress = 0;
                }
            } else {
                uner_resync_and_consider_u(parser, b);
            }
        } break;

        case UNER_STATE_WAIT_LEN:
            parser->len = b;
            parser->cks_running ^= b; /* XOR incluye Length */
            if (parser->len < UNER_LENGTH_MIN || parser->len > UNER_LENGTH_MAX) {
                uner_resync_and_consider_u(parser, b);
            } else {
#if UNER_ENABLE_LEGACY_HEADER_UNER_COLON
                /* token ya fue parte del header en modo legacy */
                parser->state = UNER_STATE_WAIT_CMD;
#else
                parser->state = UNER_STATE_WAIT_TOKEN;
#endif
            }
            break;

        case UNER_STATE_WAIT_TOKEN:
            if (b != UNER_TOKEN) {
                /* Token inválido → re-sincronizar */
                return uner_resync_and_consider_u(parser, b), UNER_ERR_TOKEN_MISMATCH;
            }
            parser->cks_running ^= b; /* XOR incluye TOKEN */
            parser->state = UNER_STATE_WAIT_CMD;
            break;

        case UNER_STATE_WAIT_CMD:
            parser->cmd = b;
            parser->cks_running ^= b; /* XOR incluye CMD */
            /* expected_payload = len - 2 (CMD y CHK) */
            parser->expected_payload = (uint8_t)(parser->len - 2u);
            parser->payload_index = 0;
            parser->state = (parser->expected_payload == 0u)
                          ? UNER_STATE_WAIT_CHK
                          : UNER_STATE_WAIT_PAYLOAD;
            break;

        case UNER_STATE_WAIT_PAYLOAD:
            if (parser->payload_index < UNER_PCK_MAX_PAYLOAD) {
                parser->receivePck.payload[parser->payload_index++] = b;
                parser->cks_running ^= b; /* XOR incluye payload */
                if (parser->payload_index >= parser->expected_payload) {
                    parser->state = UNER_STATE_WAIT_CHK;
                }
            } else {
                /* No debería pasar (length validado), re-sync */
                uner_resync_and_consider_u(parser, b);
            }
            break;

        case UNER_STATE_WAIT_CHK: {
            const uint8_t cks_rx = b;
            if (cks_rx != parser->cks_running) {
                /* Checksum inválido */
                uner_resync_and_consider_u(parser, b);
                return UNER_ERR_CHECKSUM_MISMATCH;
            }
            /* Paquete válido */
            parser->receivePck.cmd         = parser->cmd;
            parser->receivePck.payload_len = parser->expected_payload;
            parser->receivePck.cks         = cks_rx;
            parser->packet_ready           = true;

            if (parser->onPacketReady) {
                parser->onPacketReady(&parser->receivePck);
            }

            /* Listo para próximo */
            uner_reset_parser_state(parser);
            ret = UNER_OK;
        } break;

        default:
            uner_reset_parser_state(parser);
            break;
        }
    }

    return ret;
}

/* ==================== Build / Send ==================== */

uint8_t uner_calc_checksum(const uint8_t *data, size_t len) {
    uint8_t x = 0;
    for (size_t i = 0; i < len; ++i) x ^= data[i];
    return x;
}

UNERProtocolStatus uner_build_packet(const UNERProtocolPck *pck, uint8_t *out_buf, size_t out_buf_size) {
    if (!pck || !out_buf) return UNER_ERR_NULL_PTR;
    if (pck->payload_len > UNER_PCK_MAX_PAYLOAD) return UNER_ERR_PAYLOAD_TOO_LONG;

    const size_t total = (size_t)(UNER_PCK_OVERHEAD + pck->payload_len);
    if (out_buf_size < total) return UNER_ERR_BUFFER_TOO_SMALL;

    size_t i = 0;

#if UNER_ENABLE_LEGACY_HEADER_UNER_COLON
    /* "UNER:" legacy */
    out_buf[i++] = 'U'; out_buf[i++] = 'N'; out_buf[i++] = 'E'; out_buf[i++] = 'R'; out_buf[i++] = ':';
#else
    /* "UNER" correcto */
    out_buf[i++] = 'U'; out_buf[i++] = 'N'; out_buf[i++] = 'E'; out_buf[i++] = 'R';
    /* Length = 1(CMD)+N(PAY)+1(CHK) */
#endif

    /* Length */
    const uint8_t len_field = (uint8_t)(1u + pck->payload_len + 1u);
    out_buf[i++] = len_field;

#if !UNER_ENABLE_LEGACY_HEADER_UNER_COLON
    /* Token ':' según PDF */
    out_buf[i++] = UNER_TOKEN;
#endif

    /* CMD */
    out_buf[i++] = pck->cmd;

    /* Payload */
    for (uint8_t k = 0; k < pck->payload_len; ++k) {
        out_buf[i++] = pck->payload[k];
    }

    /* Checksum = XOR(header..payload) */
    const uint8_t cks = uner_calc_checksum(out_buf, i);
    out_buf[i++] = cks;

    return (UNERProtocolStatus)i; /* longitud total */
}

UNERProtocolStatus uner_send_packet(UNERProtocolParserState *parser, const UNERProtocolPck *pck) {
    if (!parser || !pck) return UNER_ERR_NULL_PTR;
    if (!parser->send_bytes) return UNER_ERR_TX_FAIL;

    uint8_t frame[UNER_PCK_MAX_TOTAL];
    UNERProtocolStatus n = uner_build_packet(pck, frame, sizeof(frame));
    if (n < 0) return n;

    UNERProtocolStatus tx = parser->send_bytes(frame, (uint16_t)n);
    return (tx == UNER_OK) ? UNER_OK : tx;
}

/* ==================== Polling helpers ==================== */

bool uner_packet_ready(const UNERProtocolParserState *parser) {
    return parser && parser->packet_ready;
}

void uner_clear_flag(UNERProtocolParserState *parser) {
    if (parser) parser->packet_ready = false;
}

const UNERProtocolPck *uner_get_last_packet(const UNERProtocolParserState *parser) {
    return parser ? &parser->receivePck : NULL;
}

