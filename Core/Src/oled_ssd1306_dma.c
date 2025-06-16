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
    if (oled->queue_count == 0){
    	__NOP();
    	return;
    }

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
    __NOP();
}

bool OLED_Is_Ready(OLED_HandleTypeDef *oled){
	return oled->init_done;
}

/**
 * @brief Callback para HAL-DMA (I2C_TX) cuando completa
 */
void OLED_DMA_CompleteCallback(OLED_HandleTypeDef *oled) {
    *(oled->dma_busy_flag) = 0;

    if (!oled->init_done) {
        if (oled->init_idx < oled->init_len) {
            // más comandos de init
            oled->requestBusCb();

        } else {
            // init terminada
            oled->init_done = true;
            __NOP();
            // refresco inicial del buffer de datos
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
		oled->page_dirty_main[p]    = true;
		oled->page_dirty_overlay[p] = false;
	}
    oled->queue_head = oled->queue_tail = oled->queue_count = 0;
    oled->overlay_active = false;
    *(oled->dma_busy_flag) = 0;

    oled->init_cmds = ssd1306_init_seq;
    oled->init_len  = SSD1306_INIT_LEN;
    oled->init_idx  = 0;
    oled->init_done = false;
    oled->font = &Font_7x10;
    oled->cursor_x = 0;
    oled->cursor_y = 0;
    oled->bitmap_opaque = true;
    oled->font_normal   = true;

    return HAL_OK;
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

static void _setPixel(OLED_HandleTypeDef *oled,
                      uint8_t x, uint8_t y,
                      bool use_overlay)
{
    uint8_t page = y >> 3;
    uint8_t bit  = 1 << (y & 0x07);
    uint8_t *buf = use_overlay ? oled->frame_buffer_overlay : oled->frame_buffer_main;
    buf[page * OLED_WIDTH + x] ^= bit;
    if (use_overlay) oled->page_dirty_overlay[page] = true;
    else            oled->page_dirty_main[page]    = true;
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
    for (uint8_t yy = y; yy < y + h; yy++) {
        for (uint8_t xx = x; xx < x + w; xx++) {
            if (xx < OLED_WIDTH && yy < OLED_HEIGHT) {
                uint16_t idx = (yy >> 3) * OLED_WIDTH + xx;
                uint8_t bit  = 1 << (yy & 7);
                uint8_t *buf = use_overlay
                    ? oled->frame_buffer_overlay
                    : oled->frame_buffer_main;
                if (oled->bitmap_opaque) buf[idx] |= bit;
                else                      buf[idx] &= ~bit;
                if (use_overlay)
                    oled->page_dirty_overlay[yy >> 3] = true;
                else
                    oled->page_dirty_main[yy >> 3]    = true;
            }
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

    while (*str) {
        uint8_t c = (uint8_t)(*str++);
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
                        uint8_t *buf = use_overlay
                            ? oled->frame_buffer_overlay
                            : oled->frame_buffer_main;
                        buf[idx] |= (1 << (py & 7));
                        if (use_overlay)
                            oled->page_dirty_overlay[py >> 3] = true;
                        else
                            oled->page_dirty_main[py >> 3]    = true;
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

HAL_StatusTypeDef OLED_SendBuffer(OLED_HandleTypeDef *oled) {
	__NOP();
    bool any = false;
    // Primero páginas main
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        if (oled->page_dirty_main[p]) {
            if (oled->queue_count >= OLED_QUEUE_DEPTH) return HAL_BUSY;
            oled->page_queue[oled->queue_tail++] = (OLED_Page_t){
                .page         = p,
                .column_start = 0,
                .length       = OLED_WIDTH,
                .use_overlay  = false
            };
            oled->queue_tail %= OLED_QUEUE_DEPTH;
            oled->queue_count++;
            oled->page_dirty_main[p] = false;
            any = true;
        }
    }
    // Luego páginas overlay
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        if (oled->page_dirty_overlay[p]) {
            if (oled->queue_count >= OLED_QUEUE_DEPTH) return HAL_BUSY;
            oled->page_queue[oled->queue_tail++] = (OLED_Page_t){
                .page         = p,
                .column_start = 0,
                .length       = OLED_WIDTH,
                .use_overlay  = true
            };
            oled->queue_tail %= OLED_QUEUE_DEPTH;
            oled->queue_count++;
            oled->page_dirty_overlay[p] = false;
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

/* ============================================================================ */
