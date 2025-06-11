/* ============================================================================
 * oled_ssd1306_dma.c
 * ============================================================================*/
#include "oled_ssd1306_dma.h"
#include "fonts.h"
#include <string.h>

/**
 * @brief Inicia transferencia DMA de la siguiente página en cola
 * @param oled  puntero al handle
 */
static void OLED_StartNextTransfer(OLED_HandleTypeDef *oled) {
	__NOP();
    if (!oled) return;
    /* Verificar DMA libre */
    if (*(oled->dma_busy_flag)) return;
    /* Verificar cola */
    if (oled->queue_count == 0) return;

    /* Tomar request */
    OLED_Page_t req = oled->page_queue[oled->queue_head];
    uint8_t *buffer = req.use_overlay ?
                        oled->frame_buffer_overlay :
                        oled->frame_buffer_main;
    uint16_t offset = req.page * OLED_WIDTH + req.column_start;

    /* Iniciar DMA I2C del fragmento */
    if (HAL_I2C_Mem_Write_DMA(
            oled->hi2c,
			(oled->oled_dev_address << 1),
            0x40,                   // Control byte de DATOS
            MEMADD_SIZE_8BIT,
            &buffer[offset],
            req.length) == HAL_OK) {
        *(oled->dma_busy_flag) = 1;
    }
}

/**
 * @brief Desencola la página enviada
 * @param oled  puntero al handle
 */
static void OLED_DequeuePage(OLED_HandleTypeDef *oled) {
    if (oled->queue_count == 0) return;
    oled->queue_head = (oled->queue_head + 1) % OLED_QUEUE_DEPTH;
    oled->queue_count--;
}

/**
 * @brief Callback para HAL-DMA (I2C_TX) cuando completa
 */
void OLED_DMA_CompleteCallback(OLED_HandleTypeDef *oled) {
    *(oled->dma_busy_flag) = 0;

    if (!oled->init_done) {
    	__NOP();
        if (oled->init_idx < oled->init_len) {
            // más comandos de init
            oled->requestBusCb();
            __NOP();
        } else {
            // init terminada
            oled->init_done = true;
            // refresco inicial del buffer de datos
            OLED_ClearBuffer(oled, false);
            oled->requestBusCb();
        }
    }
    else {
        // flujo de páginas de datos
        OLED_DequeuePage(oled);
        oled->requestBusCb();
    }
}

void OLED_GrantAccessCallback(OLED_HandleTypeDef *oled) {
    if (!oled->init_done) {
        // enviar un solo comando de init
        uint8_t cmd = oled->init_cmds[oled->init_idx++];
        __NOP();
        if (HAL_I2C_Mem_Write_DMA(
                oled->hi2c,
				oled->oled_dev_address<<1,
                0x00,                 // Control byte de COMANDO
                MEMADD_SIZE_8BIT,
                &cmd,
                1) == HAL_OK) {
            *(oled->dma_busy_flag) = 1;
        }
    }
    else {
        // modo datos normal
        OLED_StartNextTransfer(oled);
    }
}

HAL_StatusTypeDef OLED_Init(OLED_HandleTypeDef *oled,
                            I2C_HandleTypeDef *hi2c,
							uint16_t oled_dev_address,
                            volatile uint8_t  *dma_busy_flag, I2C_Request_Bus_Use requestBusCbFn) {
    /* Asignar referencias */
    oled->hi2c = hi2c;
    oled->dma_busy_flag = dma_busy_flag;
    oled->oled_dev_address = oled_dev_address;
    oled->requestBusCb = requestBusCbFn;

    /* Inicializar flags y cola */
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        oled->page_dirty[p] = true;
    }
    oled->queue_head = oled->queue_tail = oled->queue_count = 0;
    oled->overlay_active = false;
    *(oled->dma_busy_flag) = 0;

    oled->init_cmds = ssd1306_init_seq;
    oled->init_len  = SSD1306_INIT_LEN;
    oled->init_idx  = 0;
    oled->init_done = false;
    oled->font = &Font_7x10;

    return HAL_OK;
}

void OLED_ClearBuffer(OLED_HandleTypeDef *oled, bool clear_overlay) {
    if (clear_overlay)
        memset(oled->frame_buffer_overlay, 0, OLED_BUFFER_SIZE);
    else
        memset(oled->frame_buffer_main,  0, OLED_BUFFER_SIZE);
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        oled->page_dirty[p] = true;
    }
}

void OLED_DrawStr(OLED_HandleTypeDef *oled, uint8_t x, uint8_t y, const char *str) {
    if (!oled || !str || !oled->font) return;
    FontDef *f = oled->font;
    uint8_t cx = x, cy = y;

    while (*str) {
        uint8_t c = (uint8_t)(*str);
        if (c < 32 || c > 126) c = '?';
        const uint16_t *glyph = &f->data[(c - 32) * f->FontHeight];
        for (uint8_t row = 0; row < f->FontHeight; row++) {
            uint16_t bits = glyph[row];
            for (uint8_t col = 0; col < f->FontWidth; col++) {
                if (bits & (1 << (f->FontWidth - 1 - col))) {
                    uint8_t px = cx + col;
                    uint8_t py = cy + row;
                    if (px < OLED_WIDTH && py < OLED_HEIGHT) {
                        uint16_t idx = (py >> 3) * OLED_WIDTH + px;
                        oled->frame_buffer_main[idx] |= (1 << (py & 7));
                        oled->page_dirty[py >> 3] = true;
                    }
                }
            }
        }
        cx += f->FontWidth;
        str++;
        if (cx + f->FontWidth > OLED_WIDTH) break;
    }
}

HAL_StatusTypeDef OLED_SendBuffer(OLED_HandleTypeDef *oled) {
	__NOP();
    bool any = false;
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        if (oled->page_dirty[p]) {
            if (oled->queue_count >= OLED_QUEUE_DEPTH) return HAL_BUSY;
            oled->page_queue[oled->queue_tail++] = (OLED_Page_t){
                .page         = p,
                .column_start = 0,
                .length       = OLED_WIDTH,
                .use_overlay  = oled->overlay_active
            };
            oled->queue_tail %= OLED_QUEUE_DEPTH;
            oled->queue_count++;
            oled->page_dirty[p] = false;
            any = true;
        }
    }
    if (any) {
        // En lugar de arrancar DMA directamente:
        oled->requestBusCb();
    }
    return HAL_OK;
}

void OLED_ActivateOverlay(OLED_HandleTypeDef *oled,
  const uint8_t *data,
  uint16_t duration_ms) {
    memcpy(oled->frame_buffer_overlay, data, OLED_BUFFER_SIZE);
    oled->overlay_active = true;
    oled->overlay_timer_ms = duration_ms;
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        if (oled->queue_count >= OLED_QUEUE_DEPTH) break;
        oled->page_queue[oled->queue_tail] = (OLED_Page_t){
            .page = p,
            .column_start = 0,
            .length = OLED_WIDTH,
            .use_overlay = true
        };
        oled->queue_tail = (oled->queue_tail + 1) % OLED_QUEUE_DEPTH;
        oled->queue_count++;
    }
    OLED_StartNextTransfer(oled);
}

void OLED_SetFont(OLED_HandleTypeDef *oled, FontDef *f) {
    if (oled && f) {
        oled->font = f;
    }
}
/* ============================================================================ */
