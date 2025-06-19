/* ============================================================================
 * oled_ssd1306_dma.c
 * ============================================================================*/
#include "oled_ssd1306_dma.h"
#include "fonts.h"
#include "globals.h"
#include <string.h>
#include <stdlib.h>
#include "math.h"

#if OLED_MAX_PAGES == 0
#  error "OLED_MAX_PAGES está a 0 — revisa dónde lo defines"
#endif

/**
 * @brief Desencola la página enviada
 * @param oled  puntero al handle
 */
static void OLED_DequeuePage(OLED_HandleTypeDef *oled) {
    if (oled->queue_count == 0) return;

    // Marcamos como transferida
    oled->page_queue[oled->queue_head].transfer_state = PAGE_REQ_DONE;

    // Avanzamos la cabeza de la cola
    oled->queue_head = (oled->queue_head + 1) % OLED_QUEUE_DEPTH;
    oled->queue_count--;

    // Avanzamos el índice de página actual
    oled->current_page_index = oled->queue_head;

    __NOP();
}

void OLED_HideOverlayNow(OLED_HandleTypeDef *oled) {
    if (!oled) return;

    oled->overlay_active = false;
    oled->overlay_timeout_active = false;
    oled->allow_overlay_transfer = false;

    oled->overlay_timer_ms = (oled->overlay_time_in_ms != DEFAULT_OVERLAY_TIME_MS)
                                ? oled->overlay_time_in_ms
                                : DEFAULT_OVERLAY_TIME_MS;

    // Marcar todas las páginas del buffer main como dirty
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        oled->page_dirty_main[p] = true;
    }

    // Encolar páginas dirty del buffer main
    OLED_SendBuffer(oled);
}

HAL_StatusTypeDef OLED_SendOverlayBuffer(OLED_HandleTypeDef *oled) {
    if (!oled) return HAL_ERROR;

    // 1. Activa flag que permite encolar y transferir páginas overlay
    oled->allow_overlay_transfer = true;

    // 2. Marca todas las páginas overlay como dirty
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        oled->page_dirty_overlay[p] = true;
    }

    // 3. Llama a SendBuffer para encolarlas (si hay espacio)
    return OLED_SendBuffer(oled);
}

HAL_StatusTypeDef OLED_SendBuffer(OLED_HandleTypeDef *oled) {
    __NOP();
    if (!oled) return HAL_ERROR;

    // MAIN buffer
    bool found;
    found = false;
    if (!oled->overlay_active) {
		for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
			if (oled->page_dirty_main[p]) {
				for (uint8_t i = 0; i < oled->queue_count; i++) {
					uint8_t idx = (oled->queue_head + i) % OLED_QUEUE_DEPTH;
					OLED_Page_t *q = &oled->page_queue[idx];
					if (q->page == p && !q->use_overlay && q->transfer_state == PAGE_REQ_WAITING) {
						// Combinamos rangos
						uint8_t new_start = 0, new_end = OLED_WIDTH;
						uint8_t old_start = q->column_start;
						uint8_t old_end = q->column_start + q->length;

						q->column_start = (new_start < old_start) ? new_start : old_start;
						q->length = ((new_end > old_end) ? new_end : old_end) - q->column_start;

						found = true;
						break;
					}
				}

				if (!found) {
					if (oled->queue_count >= OLED_QUEUE_DEPTH) return HAL_BUSY;
					oled->page_queue[oled->queue_tail] = (OLED_Page_t){
						.page = p,
						.column_start = 0,
						.length = OLED_WIDTH,
						.use_overlay = false,
						.transfer_state = PAGE_REQ_WAITING
					};
					oled->queue_tail = (oled->queue_tail + 1) % OLED_QUEUE_DEPTH;
					oled->queue_count++;
				}

				oled->page_dirty_main[p] = false;
			}
		}
    }

    // OVERLAY buffer (solo si se permite)
    if (oled->allow_overlay_transfer) {
        found = false;
        for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
            if (oled->page_dirty_overlay[p]) {
                for (uint8_t i = 0; i < oled->queue_count; i++) {
                    uint8_t idx = (oled->queue_head + i) % OLED_QUEUE_DEPTH;
                    OLED_Page_t *q = &oled->page_queue[idx];
                    if (q->page == p && q->use_overlay && q->transfer_state == PAGE_REQ_WAITING) {
                        // Combinamos rangos
                        uint8_t new_start = 0, new_end = OLED_WIDTH;
                        uint8_t old_start = q->column_start;
                        uint8_t old_end = q->column_start + q->length;

                        q->column_start = (new_start < old_start) ? new_start : old_start;
                        q->length = ((new_end > old_end) ? new_end : old_end) - q->column_start;

                        found = true;
                        break;
                    }
                }

                if (!found) {
                    if (oled->queue_count >= OLED_QUEUE_DEPTH) return HAL_BUSY;
                    oled->page_queue[oled->queue_tail] = (OLED_Page_t){
                        .page = p,
                        .column_start = 0,
                        .length = OLED_WIDTH,
                        .use_overlay = true,
                        .transfer_state = PAGE_REQ_WAITING
                    };
                    oled->queue_tail = (oled->queue_tail + 1) % OLED_QUEUE_DEPTH;
                    oled->queue_count++;
                }

                oled->page_dirty_overlay[p] = false;
            }
        }
    }

    oled->current_page_index = oled->queue_head;

    return HAL_OK;
}

/**
 * @brief Función periódica no bloqueante que gestiona el envío de páginas.
 *        Debe llamarse desde OLED_MainTask().
 */
void OLED_Update(OLED_HandleTypeDef *oled) {
    if (!oled || !oled->init_done) return;

    if (oled->queue_count == 0) {
        // Si estaba habilitado el overlay y ya se envió todo
        if (oled->allow_overlay_transfer) {
            oled->allow_overlay_transfer = false;
            // No tocamos oled->overlay_active acá, se gestiona desde el main task
        }
        return;
    }

    OLED_Page_t *req = &oled->page_queue[oled->current_page_index];

    // ⛔ No enviar MAIN si overlay está activo
    if (oled->overlay_active && !req->use_overlay) {
        oled->queue_head = (oled->queue_head + 1) % OLED_QUEUE_DEPTH;
        oled->queue_count--;
        oled->current_page_index = oled->queue_head;
        return;
    }

    // ⛔ No enviar OVERLAY si no está permitido
    if (!oled->allow_overlay_transfer && req->use_overlay) {
        oled->queue_head = (oled->queue_head + 1) % OLED_QUEUE_DEPTH;
        oled->queue_count--;
        oled->current_page_index = oled->queue_head;
        return;
    }

    // ✅ Avanzamos al siguiente estado
    if (req->transfer_state == PAGE_REQ_WAITING) {
        req->transfer_state = PAGE_REQ_COL_PENDING;
        __NOP();
        if (oled->requestBusCb) {
            oled->requestBusCb(I2C_REQ_IS_TX);  // pide acceso al bus I2C
        }
    }
}

/**
 * @brief Callback para HAL-DMA (I2C_TX) cuando completa
 */
void OLED_DMA_CompleteCallback(OLED_HandleTypeDef *oled, uint8_t is_tx) {
    if (!oled) return;

    if (oled->releaseBusCb) {
		oled->releaseBusCb();  // Esto hace que I2C_Manager_OnDMAComplete se entere
	}

    if (!oled->init_done) {
        if (oled->init_idx < oled->init_len) {
            if (oled->requestBusCb) oled->requestBusCb(I2C_REQ_IS_TX);
            return;
        }else if (!oled->just_finished_init) {
            oled->just_finished_init = true;
            if (oled->requestBusCb) oled->requestBusCb(I2C_REQ_IS_TX);
            return;
        }

        oled->just_finished_init = false;
        oled->init_done = true;
        return;
    }else{
		// flujo normal
		OLED_Page_t *req = &oled->page_queue[oled->current_page_index];
		__NOP();
		switch (req->transfer_state) {
			case PAGE_REQ_COL_PENDING:
				req->transfer_state = PAGE_REQ_PAGE_PENDING;
				oled->requestBusCb(I2C_REQ_IS_TX);  // pedir acceso para el segundo comando
				__NOP();
				break;

			case PAGE_REQ_PAGE_PENDING:
				req->transfer_state = PAGE_REQ_DATA_PENDING;
				oled->requestBusCb(I2C_REQ_IS_TX);  // pedir acceso para enviar los datos
				__NOP();
				break;

			case PAGE_REQ_DATA_PENDING:
				req->transfer_state = PAGE_REQ_DONE;
				OLED_DequeuePage(oled);  // ahora sí: datos ya enviados
				__NOP();
				break;

			default:
				break;
		}
    }
}

//OLED_DMA_CompleteCallback

void OLED_GrantAccessCallback(OLED_HandleTypeDef *oled) {
	__NOP();
    if (!oled->init_done) {
    	__NOP();
        // enviar un solo comando de init
    	if (oled->init_idx < oled->init_len) {
			uint8_t cmd = oled->init_cmds[oled->init_idx++];
			if (HAL_I2C_Mem_Write_DMA(
					oled->hi2c,
					oled->oled_dev_address<<1,
					0x00,                 // Control byte de COMANDO
					MEMADD_SIZE_8BIT,
					&cmd,
					1) == HAL_OK) {
			}
    	}
    	if(oled->just_finished_init){
			OLED_ClearBuffer(oled, false);      // Borra el framebuffer principal
			OLED_SendBuffer(oled);
			HAL_I2C_Mem_Write_DMA(
				oled->hi2c,
				oled->oled_dev_address << 1,
				0x40,  // Control byte: DATA
				MEMADD_SIZE_8BIT,
				oled->frame_buffer_main,
				OLED_WIDTH * OLED_PAGES
			);
			__NOP();
    	}
    } else {
    	OLED_Page_t *req = &oled->page_queue[oled->current_page_index];
    	__NOP();
		switch (req->transfer_state) {
			case PAGE_REQ_COL_PENDING:
					OLED_SendCommand_SetColumn(oled, req->column_start);
				break;
			case PAGE_REQ_PAGE_PENDING:
					OLED_SendCommand_SetPage(oled, req->page);
				break;
			case PAGE_REQ_DATA_PENDING:
					OLED_SendData(oled, req); // función abajo
				break;
			default:
				break;
		}
    }
}

HAL_StatusTypeDef OLED_Init(OLED_HandleTypeDef *oled,
                            I2C_HandleTypeDef *hi2c,
							uint16_t oled_dev_address,
                            volatile uint8_t  *dma_busy_flag, I2C_Request_Bus_Use requestBusCbFn, I2C_Release_Bus_Use releaseBusUseFn) {
    /* Asignar referencias */
    oled->hi2c = hi2c;
    oled->dma_busy_flag = dma_busy_flag;
    oled->oled_dev_address = oled_dev_address;
    oled->requestBusCb = requestBusCbFn;
    oled->releaseBusCb = releaseBusUseFn;

    /* Inicializar flags y cola */
	for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
		oled->page_dirty_main[p]    = true;
		oled->page_dirty_overlay[p] = false;
	}
    oled->queue_head = oled->queue_tail = oled->queue_count = 0;

    oled->init_cmds = ssd1306_init_seq;
    oled->init_len  = SSD1306_INIT_LEN;
    oled->init_idx  = 0;
    oled->init_done = false;
    oled->font = &Font_7x10;
    oled->cursor_x = 0;
    oled->cursor_y = 0;
    oled->first_Fn_Draw = false;
    oled->allow_overlay_transfer = false;
    oled->overlay_active = false;
    oled->overlay_timeout_active = false;
    oled->overlay_time_in_ms = DEFAULT_OVERLAY_TIME_MS;
    oled->overlay_timer_ms = DEFAULT_OVERLAY_TIME_MS;
    oled->overlay_hide_now = false;
    __NOP();

    return HAL_OK;
}

void initOLEDSequence(OLED_HandleTypeDef *oled)
{
    if (!oled) return;
    //solicitamos el bus I2C y disparamos el primer comando de init
    oled->requestBusCb(I2C_REQ_IS_TX);
    __NOP();
}

void OLED_ClearBuffer(OLED_HandleTypeDef *oled, bool clear_overlay) {
	if (clear_overlay) {
		memset(oled->frame_buffer_overlay, 0, OLED_BUFFER_SIZE);
		for (uint8_t p = 0; p < OLED_MAX_PAGES; p++){
			oled->page_dirty_overlay[p] = true;
		}
	} else {
		memset(oled->frame_buffer_main, 0, OLED_BUFFER_SIZE);
		for (uint8_t p = 0; p < OLED_MAX_PAGES; p++){
			oled->page_dirty_main[p] = true;
		}
	}
}

void OLED_SetCursor(OLED_HandleTypeDef *oled, uint8_t x, uint8_t y) {
    if (!oled) return;
    oled->cursor_x = x;
    oled->cursor_y = y;
}

void OLED_SetFont(OLED_HandleTypeDef *oled, FontDef *f) {
    if (!oled || !f) return;
    oled->font = f;
}

static void _setPixel(OLED_HandleTypeDef *oled, uint8_t x, uint8_t y, bool use_overlay)
{
    if (!oled || x >= OLED_WIDTH || y >= OLED_HEIGHT) return;

    uint8_t page = y >> 3;
    uint8_t bit  = 1 << (y & 0x07);
    uint8_t *buf = use_overlay ? oled->frame_buffer_overlay
                               : oled->frame_buffer_main;

    buf[page * OLED_WIDTH + x] |= bit;

    // Marcar página como sucia
    bool *dirty = use_overlay ? oled->page_dirty_overlay : oled->page_dirty_main;
    dirty[page] = true;

    // Actualizar columnas afectadas
    OLED_Page_t *queue = oled->page_queue;
    for (int i = 0; i < OLED_QUEUE_DEPTH; i++) {
        if (queue[i].page == page && queue[i].use_overlay == use_overlay) {
            if (x < queue[i].column_start) queue[i].column_start = x; ///Si column start es 0, puede ser que recien se encolo, deberia buscar la posicion del primer bit distinto de 0 y esar ese como inicio (asi no enviamos tantos bytes vacios y optimizamos)
            if (x > (queue[i].column_start + queue[i].length - 1)) {
                queue[i].length = x - queue[i].column_start + 1;
            }
            return; // ya existe la entrada, actualizada
        }
    }

    // Si no hay entrada existente, se agregará más tarde al encolar
}

void OLED_DrawPixel(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y,
                    bool use_overlay)
{
    _setPixel(oled, x, y, use_overlay);
}

void OLED_DrawHLine(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y, uint8_t w,
                    bool use_overlay)
{
    for (uint8_t i = 0; i < w; i++) {
        _setPixel(oled, x + i, y, use_overlay);
    }
}

void OLED_DrawVLine(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y, uint8_t h,
                    bool use_overlay)
{
    for (uint8_t i = 0; i < h; i++) {
        _setPixel(oled, x, y + i, use_overlay);
    }
}

void OLED_DrawLineXY(OLED_HandleTypeDef *oled,
                     uint8_t x0, uint8_t y0,
                     uint8_t x1, uint8_t y1,
                     bool use_overlay)
{
    // Implementación Bresenham
    int dx = abs((int)x1 - (int)x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = abs((int)y1 - (int)y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        // Dibujar pixel
        if (x0 < OLED_WIDTH && y0 < OLED_HEIGHT) {
            _setPixel(oled, x0, y0, use_overlay);
        }
        // Condición de término
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void OLED_DrawBox(OLED_HandleTypeDef *oled,
                  uint8_t x, uint8_t y,
                  uint8_t w, uint8_t h,
                  bool use_overlay)
{
    if (!oled) return;

    uint8_t *buf = use_overlay
        ? oled->frame_buffer_overlay
        : oled->frame_buffer_main;
    bool *dirty = use_overlay
        ? oled->page_dirty_overlay
        : oled->page_dirty_main;

    for (uint8_t yy = y; yy < y + h; yy++) {
        if (yy >= OLED_HEIGHT) continue;
        uint8_t page = yy >> 3;

        for (uint8_t xx = x; xx < x + w; xx++) {
            if (xx >= OLED_WIDTH) continue;

            uint16_t idx = page * OLED_WIDTH + xx;
            uint8_t bit  = 1 << (yy & 7);

            if (oled->bitmap_opaque) buf[idx] |= bit;
            else                     buf[idx] &= ~bit;

            dirty[page] = true;

            // Actualizar columnas afectadas para la página
            OLED_Page_t *queue = oled->page_queue;
            for (int i = 0; i < OLED_QUEUE_DEPTH; i++) {
                if (queue[i].page == page && queue[i].use_overlay == use_overlay) {
                    // Ya existe: combinar
                    if (xx < queue[i].column_start) queue[i].column_start = xx;
                    uint8_t right = queue[i].column_start + queue[i].length - 1;
                    if (xx > right) queue[i].length = xx - queue[i].column_start + 1;
                    goto next_pixel; // no seguir buscando
                }
            }

        next_pixel:;
        }
    }
}


void OLED_DrawFrame(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y,
                    uint8_t w, uint8_t h,
                    bool use_overlay)
{
    OLED_DrawHLine(oled, x, y, w, use_overlay);
    OLED_DrawHLine(oled, x, y + h - 1, w, use_overlay);
    OLED_DrawVLine(oled, x, y, h, use_overlay);
    OLED_DrawVLine(oled, x + w - 1, y, h, use_overlay);
}

void OLED_DrawCircle(OLED_HandleTypeDef *oled,
                     uint8_t x0, uint8_t y0,
                     uint8_t rad,
                     bool use_overlay)
{
    int16_t f = 1 - rad;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * rad;
    int16_t x = 0;
    int16_t y = rad;

    _setPixel(oled, x0, y0 + rad, use_overlay);
    _setPixel(oled, x0, y0 - rad, use_overlay);
    _setPixel(oled, x0 + rad, y0, use_overlay);
    _setPixel(oled, x0 - rad, y0, use_overlay);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        _setPixel(oled, x0 + x, y0 + y, use_overlay);
        _setPixel(oled, x0 - x, y0 + y, use_overlay);
        _setPixel(oled, x0 + x, y0 - y, use_overlay);
        _setPixel(oled, x0 - x, y0 - y, use_overlay);
        _setPixel(oled, x0 + y, y0 + x, use_overlay);
        _setPixel(oled, x0 - y, y0 + x, use_overlay);
        _setPixel(oled, x0 + y, y0 - x, use_overlay);
        _setPixel(oled, x0 - y, y0 - x, use_overlay);
    }
}

void OLED_DrawDisc(OLED_HandleTypeDef *oled,
                   uint8_t x0, uint8_t y0,
                   uint8_t rad,
                   bool use_overlay)
{
    for (int16_t dy = -rad; dy <= rad; dy++) {
        int16_t dx = (int16_t)floorf(sqrtf((rad * rad) - (dy * dy)));
        OLED_DrawHLine(oled, x0 - dx, y0 + dy, dx * 2 + 1, use_overlay);
    }
}

void OLED_DrawArc(OLED_HandleTypeDef *oled,
                  uint8_t x0, uint8_t y0,
                  uint8_t rad,
                  uint8_t start, uint8_t end,
                  bool use_overlay)
{
    float a0 = (start / 256.0f) * 2 * M_PI;
    float a1 = (end   / 256.0f) * 2 * M_PI;
    for (float a = a0; a <= a1; a += 0.01f) {
        int16_t x = x0 + (int16_t)(rad * cosf(a));
        int16_t y = y0 + (int16_t)(rad * sinf(a));
        _setPixel(oled, x, y, use_overlay);
    }
}

void OLED_DrawEllipse(OLED_HandleTypeDef *oled,
                      uint8_t x0, uint8_t y0,
                      uint8_t rx, uint8_t ry,
                      bool use_overlay)
{
    for (int16_t deg = 0; deg < 360; deg++) {
        float radang = deg * (M_PI / 180.0f);
        int16_t x = x0 + (int16_t)(rx * cosf(radang));
        int16_t y = y0 + (int16_t)(ry * sinf(radang));
        _setPixel(oled, x, y, use_overlay);
    }
}

void OLED_DrawFilledEllipse(OLED_HandleTypeDef *oled,
                            uint8_t x0, uint8_t y0,
                            uint8_t rx, uint8_t ry,
                            bool use_overlay)
{
    for (int16_t dy = -ry; dy <= ry; dy++) {
        int16_t dx = (int16_t)(rx * sqrtf(1.0f - (dy / (float)ry) * (dy / (float)ry)));
        OLED_DrawHLine(oled, x0 - dx, y0 + dy, dx * 2 + 1, use_overlay);
    }
}

void OLED_DrawTriangle(OLED_HandleTypeDef *oled,
                       int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2,
                       bool use_overlay)
{
    OLED_DrawLineXY(oled, x0, y0, x1, y1, use_overlay);
    OLED_DrawLineXY(oled, x1, y1, x2, y2, use_overlay);
    OLED_DrawLineXY(oled, x2, y2, x0, y0, use_overlay);
}

void OLED_DrawBitmap(OLED_HandleTypeDef *oled,
                     uint8_t x, uint8_t y,
                     uint8_t cnt, uint8_t h,
                     const uint8_t *bitmap,
                     bool use_overlay)
{
    OLED_DrawXBM(oled, x, y, cnt * 8, h, bitmap, use_overlay);
}

void OLED_DrawXBM(OLED_HandleTypeDef *oled,
                  uint8_t x, uint8_t y,
                  uint8_t w, uint8_t h,
                  const uint8_t *bitmap,
                  bool use_overlay)
{
    for (uint8_t row = 0; row < h; row++) {
        for (uint8_t col = 0; col < w; col++) {
            uint8_t byte = bitmap[(col / 8) + row * (w / 8)];
            if (byte & (1 << (col % 8))) {
                _setPixel(oled, x + col, y + row, use_overlay);
            }
        }
    }
}

void OLED_DrawStr(OLED_HandleTypeDef *oled,
                  const char *str,
                  bool use_overlay)
{
    if (!oled || !str || !oled->font) return;
    FontDef *f = oled->font;
    uint8_t cx = oled->cursor_x;
    uint8_t cy = oled->cursor_y;

    uint8_t min_col = OLED_WIDTH - 1;
    uint8_t max_col = 0;

    while (*str) {
        uint8_t c = (uint8_t)(*str++);
        if (c < 32 || c > 126) c = '?';
        const uint16_t *glyph = &f->data[(c - 32) * f->FontHeight];

        for (uint8_t row = 0; row < f->FontHeight; row++) {
            uint16_t bits = glyph[row];
            uint16_t mask = 0x8000;

            for (uint8_t col = 0; col < f->FontWidth; col++) {
                if (bits & (mask >> col)) {
                    uint8_t px = cx + col;
                    uint8_t py = cy + row;
                    if (px >= OLED_WIDTH || py >= OLED_HEIGHT) continue;

                    uint8_t page = py >> 3;
                    uint16_t idx = page * OLED_WIDTH + px;
                    uint8_t *buf = use_overlay
                        ? oled->frame_buffer_overlay
                        : oled->frame_buffer_main;
                    bool *dirty = use_overlay
                        ? oled->page_dirty_overlay
                        : oled->page_dirty_main;

                    buf[idx] |= (1 << (py & 7));
                    dirty[page] = true;

                    // Marcar columnas afectadas
                    if (px < min_col) min_col = px;
                    if (px > max_col) max_col = px;

                    // Actualizar entrada en la cola si existe
                    OLED_Page_t *queue = oled->page_queue;
                    for (int i = 0; i < OLED_QUEUE_DEPTH; i++) {
                        if (queue[i].page == page && queue[i].use_overlay == use_overlay) {
                            if (px < queue[i].column_start)
                                queue[i].column_start = px;
                            uint8_t right = queue[i].column_start + queue[i].length - 1;
                            if (px > right)
                                queue[i].length = px - queue[i].column_start + 1;
                            break;
                        }
                    }
                }
            }
        }

        cx += f->FontWidth;
        oled->cursor_x = cx;
        if (cx + f->FontWidth > OLED_WIDTH) break;
    }
}

void OLED_SetBitmapMode(OLED_HandleTypeDef *oled, bool opaque) {
    if (!oled) return;
    oled->bitmap_opaque = opaque;
}

void OLED_SetFontMode(OLED_HandleTypeDef *oled, bool normal) {
    if (!oled) return;
    oled->font_normal = normal;
}

void OLED_SendCommand_SetColumn(OLED_HandleTypeDef *oled, uint8_t col_start) {
    uint8_t cmds[3] = {
        0x21,              // Set Column Address
        col_start,
        127                // Hasta fin de página
    };
    HAL_I2C_Mem_Write_DMA(oled->hi2c,
                          oled->oled_dev_address<<1,
                          0x00, // control byte: COMANDO
                          MEMADD_SIZE_8BIT,
                          cmds, 3);
}

void OLED_SendCommand_SetPage(OLED_HandleTypeDef *oled, uint8_t page) {
    uint8_t cmds[3] = {
        0x22,   // Set Page Address
        page,
        page
    };
    HAL_I2C_Mem_Write_DMA(oled->hi2c,
                          oled->oled_dev_address<<1,
                          0x00,
                          MEMADD_SIZE_8BIT,
                          cmds, 3);
}

void OLED_SendData(OLED_HandleTypeDef *oled, OLED_Page_t *req) {
    uint16_t offset = req->page * OLED_WIDTH + req->column_start;
    uint8_t *src = req->use_overlay ? oled->frame_buffer_overlay : oled->frame_buffer_main;
    HAL_I2C_Mem_Write_DMA(oled->hi2c,
                          oled->oled_dev_address<<1,
                          0x40,  // control byte: DATA
                          MEMADD_SIZE_8BIT,
                          &src[offset],
                          req->length);
}

void OLED_ChangeOverlayTime(OLED_HandleTypeDef *oled, uint16_t duration_ms) {
	if (duration_ms == 0) {
		oled->overlay_timer_ms = DEFAULT_OVERLAY_TIME_MS;
		oled->overlay_time_in_ms = DEFAULT_OVERLAY_TIME_MS;
	} else {
		oled->overlay_timer_ms = duration_ms;
		oled->overlay_time_in_ms = duration_ms;
	}
}

void OLED_ActivateOverlay(OLED_HandleTypeDef *oled, uint16_t duration_ms) {
    if (!oled) return;

    oled->overlay_active = true;
    oled->allow_overlay_transfer = true;

    // Configura el modo de ocultamiento
    if (duration_ms == 0) {
        oled->overlay_timeout_active = false;
    } else {
        oled->overlay_timeout_active = true;
        oled->overlay_timer_ms = duration_ms;
        oled->overlay_time_in_ms = duration_ms;
    }

    // Encola todas las páginas overlay
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        if (oled->queue_count >= OLED_QUEUE_DEPTH) break;

        oled->page_queue[oled->queue_tail] = (OLED_Page_t){
            .page = p,
            .column_start = 0,
            .length = OLED_WIDTH,
            .use_overlay = true,
            .transfer_state = PAGE_REQ_WAITING
        };
        oled->queue_tail = (oled->queue_tail + 1) % OLED_QUEUE_DEPTH;
        oled->queue_count++;
    }

    // El envío real comenzará en OLED_Update() (llamado desde OLED_MainTask)
}


/* ============================================================================ */
