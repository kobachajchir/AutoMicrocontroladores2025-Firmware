// uner_protocol.c
#include "uner_protocol.h"

void uner_init(UNERProtocolParserState *parser) {
    if (!parser) return;
    parser->head = 0;
    parser->tail = 0;
    parser->state = UNER_STATE_WAIT_HEADER;
    parser->header_index = 0;
    parser->len = 0;
    parser->cmd = 0;
    parser->payload_index = 0;
    parser->checksum = 0;
    parser->packet_ready = false;
    parser->onPacketReady = NULL;
    parser->send_bytes = NULL;
}

UNERProtocolStatus uner_push_byte(UNERProtocolParserState *parser, uint8_t byte) {
    if (!parser) return UNER_ERR_NULL_PTR;
    uint16_t next_head = (parser->head + 1) % UNER_PCK_MAX_TOTAL;
    if (next_head == parser->tail) return UNER_ERR_BUFFER_OVERFLOW;
    parser->buffer[parser->head] = byte;
    parser->head = next_head;
    return UNER_OK;
}

static bool uner_pop_byte(UNERProtocolParserState *parser, uint8_t *out) {
    if (!parser || parser->tail == parser->head) return false;
    *out = parser->buffer[parser->tail];
    parser->tail = (parser->tail + 1) % UNER_PCK_MAX_TOTAL;
    return true;
}

static void uner_reset_parser_state(UNERProtocolParserState *parser) {
    parser->state = UNER_STATE_WAIT_HEADER;
    parser->header_index = 0;
    parser->payload_index = 0;
    parser->len = 0;
    parser->cmd = 0;
    parser->checksum = 0;
    parser->tail = parser->head;
}

UNERProtocolStatus uner_parse(UNERProtocolParserState *parser) {
    if (!parser) return UNER_ERR_NULL_PTR;
    uint8_t byte;
    while (uner_pop_byte(parser, &byte)) {
        switch (parser->state) {
            case UNER_STATE_WAIT_HEADER:
                parser->header_buf[parser->header_index++] = byte;
                if (parser->header_index == UNER_HEADER_LENGTH) {
                    int sum = 0;
                    for (int i = 0; i < UNER_HEADER_LENGTH; ++i)
                        sum += parser->header_buf[i] * (i + 1);
                    if (sum == UNER_HEADER_TARGET_SUM) {
                        parser->state = UNER_STATE_WAIT_LEN;
                    } else {
                        uner_reset_parser_state(parser);
                    }
                }
                break;
            case UNER_STATE_WAIT_LEN:
                parser->len = byte;
                if (parser->len < 1 || parser->len > (1 + UNER_PCK_MAX_PAYLOAD)) {
                    uner_reset_parser_state(parser);
                } else {
                    parser->state = UNER_STATE_WAIT_CMD;
                }
                break;
            case UNER_STATE_WAIT_CMD:
                parser->cmd = byte;
                parser->payload_index = 0;
                if (parser->len == 1) {
                    parser->state = UNER_STATE_WAIT_CHK;
                } else {
                    parser->state = UNER_STATE_WAIT_PAYLOAD;
                }
                break;
            case UNER_STATE_WAIT_PAYLOAD:
                parser->receivePck.payload[parser->payload_index++] = byte;
                if (parser->payload_index >= (parser->len - 1)) {
                    parser->state = UNER_STATE_WAIT_CHK;
                }
                break;
            case UNER_STATE_WAIT_CHK: {
                parser->checksum = byte;
                uint8_t chk = parser->cmd;
                for (int i = 0; i < parser->payload_index; ++i)
                    chk ^= parser->receivePck.payload[i];

                if (chk == parser->checksum) {
                    parser->receivePck.cmd = parser->cmd;
                    parser->receivePck.payload_len = parser->payload_index;
                    parser->receivePck.cks = parser->checksum;
                    parser->packet_ready = true;
                    if (parser->onPacketReady) {
                        parser->onPacketReady(&parser->receivePck);
                    }
                }
                uner_reset_parser_state(parser);
                break;
            }
            default:
                uner_reset_parser_state(parser);
                break;
        }
    }
    return UNER_OK;
}

UNERProtocolStatus uner_build_packet(const UNERProtocolPck *pck, uint8_t *out_buf, size_t out_buf_size) {
    if (!pck || !out_buf) return UNER_ERR_NULL_PTR;
    if (pck->payload_len > UNER_PCK_MAX_PAYLOAD) return UNER_ERR_PAYLOAD_TOO_LONG;

    size_t total_len = 5 + 1 + 1 + pck->payload_len + 1;
    if (out_buf_size < total_len) return UNER_ERR_BUFFER_TOO_SMALL;

    out_buf[0] = 'U'; out_buf[1] = 'N'; out_buf[2] = 'E'; out_buf[3] = 'R'; out_buf[4] = ':';
    out_buf[5] = 1 + pck->payload_len;
    out_buf[6] = pck->cmd;
    for (uint8_t i = 0; i < pck->payload_len; ++i)
        out_buf[7 + i] = pck->payload[i];

    uint8_t chk = pck->cmd;
    for (uint8_t i = 0; i < pck->payload_len; ++i)
        chk ^= pck->payload[i];
    out_buf[7 + pck->payload_len] = chk;

    return (UNERProtocolStatus)total_len;
}

UNERProtocolStatus uner_send_packet(UNERProtocolParserState *parser, const UNERProtocolPck *pck) {
    if (!parser || !parser->send_bytes) return UNER_ERR_NULL_PTR;
    uint8_t buffer[UNER_PCK_MAX_TOTAL];
    UNERProtocolStatus len = uner_build_packet(pck, buffer, sizeof(buffer));
    if (len < 0) return len;

    UNERProtocolStatus result = parser->send_bytes(buffer, (uint16_t)len);
    return result == UNER_OK ? UNER_OK : result;
}

bool uner_packet_ready(const UNERProtocolParserState *parser) {
    return parser && parser->packet_ready;
}

void uner_clear_flag(UNERProtocolParserState *parser) {
    if (parser) parser->packet_ready = false;
}

const UNERProtocolPck *uner_get_last_packet(const UNERProtocolParserState *parser) {
    return parser ? &parser->receivePck : NULL;
}
