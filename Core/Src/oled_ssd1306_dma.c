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
 * @brief Oculta inmediatamente el overlay y repone la capa MAIN completa.
 * @param oled  Puntero al handle
 */
void OLED_HideOverlayNow(OLED_HandleTypeDef *oled) {
    if (!oled) return;

    // Desactivar overlay
    oled->overlay_active          = false;
    oled->overlay_timeout_active  = false;
    oled->allow_overlay_transfer  = false;

    // Resetear temporizador al valor por defecto o al configurado
    oled->overlay_timer_ms = (oled->overlay_time_in_ms != DEFAULT_OVERLAY_TIME_MS)
                               ? oled->overlay_time_in_ms
                               : DEFAULT_OVERLAY_TIME_MS;

    // 1) Borrar y encolar toda la capa MAIN
    //    (OLED_ClearBuffer(false) limpia frame_buffer_main y encola
    //     las 8 páginas MAIN en la sub-cola 0–7)
    OLED_ClearBuffer(oled, false);
}


/**
 * @brief  Llama periódicamente para arrancar el envío de la siguiente página.
 *         Solicita el bus I²C solo si no hay nada enviando y hay páginas
 *         pendientes en la sub-cola activa (MAIN u OVERLAY).
 */
void OLED_Update(OLED_HandleTypeDef *oled) {
    if (!oled || !oled->init_done) return;    // 1) Espera a terminar la init
    if (oled->is_sending)       return;       // 2) Si ya hay transferencia en curso, sal
    // 3) Elige la sub-cola actual según overlay_active
    uint8_t pending = oled->overlay_active
                    ? oled->ovl_count
                    : oled->main_count;
    // 4) Si hay páginas pendientes, pide el bus
    if (pending > 0 && oled->requestBusCb) {
        oled->requestBusCb(I2C_REQ_IS_TX);
    }
}

HAL_StatusTypeDef OLED_SendBuffer(OLED_HandleTypeDef *oled) {
    if (!oled)                 return HAL_ERROR;
    if (!oled->init_done)      return HAL_BUSY;
    if (oled->is_sending)      return HAL_BUSY;

    // Seleccionar sub-cola activa
    bool    useOvl  = oled->overlay_active;
    uint8_t head    = useOvl ? oled->ovl_head  : oled->main_head;
    uint8_t count   = useOvl ? oled->ovl_count : oled->main_count;
    uint8_t baseIdx = useOvl ? MAIN_QDEPTH      : 0;
    uint8_t depth   = MAIN_QDEPTH;

    // Buscar la primera página en estado WAITING
    uint8_t idxFound = 0xFF;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t idx = baseIdx + ((head + i) % depth);
        if (oled->page_queue[idx].transfer_state == PAGE_REQ_WAITING) {
            idxFound = idx;
            break;
        }
    }
    if (idxFound == 0xFF) {
        // No hay páginas nuevas por enviar
        return HAL_OK;
    }

    // Arrancamos la máquina de estados en PAGE_PENDING
    OLED_Page_t *req = &oled->page_queue[idxFound];
    oled->current_page_index = idxFound;

    // Enviar solo el comando "Set Page"
	uint8_t cmds[3] = { PAGEADDR, req->page, req->page };
	if(HAL_I2C_Mem_Write_DMA(
		oled->hi2c,
		oled->oled_dev_address << 1,
		0x00, MEMADD_SIZE_8BIT,
		cmds, 3
	) == HAL_OK){
		req->transfer_state = PAGE_REQ_PAGE_PENDING;
		oled->is_sending = true;
		__NOP();
	}else {
		// Si falla, podrías reintentar o dejar is_sending en false
		return HAL_ERROR;
	}

    // Ya hay una transferencia en curso
    return HAL_OK;
}


/**
 * @brief Callback de HAL-DMA cuando termina I2C_TX.
 *        Avanza la state-machine COL→PAGE→DATA→DONE, y al DONE
 *        desencola todas las entradas DONE de la MISMA sub-cola.
 *        Finalmente, encadena el siguiente envío si queda algo.
 */
void OLED_DMA_CompleteCallback(OLED_HandleTypeDef *oled, uint8_t is_tx) {
    if (!oled) return;
    __NOP();
    // — Init flow (igual que antes) —
    if (!oled->init_done) {
        if (oled->init_idx < oled->init_len) {
            uint8_t cmd = oled->init_cmds[oled->init_idx];
            HAL_I2C_Mem_Write_DMA(
                oled->hi2c,
                oled->oled_dev_address << 1,
                0x00, MEMADD_SIZE_8BIT,
                &cmd,
                1
            );
            __NOP();
            oled->init_idx++;
        }else{
            oled->init_done = true;
            oled->just_finished_init = true;
        	OLED_ClearBuffer(oled, false);
        	oled->is_sending = false;
        	OLED_SendBuffer(oled);
        }
    }else{
        // — Página normal —
        OLED_Page_t *req = &oled->page_queue[oled->current_page_index];
        __NOP();
        if(oled->page_queue[oled->current_page_index].transfer_state == PAGE_REQ_PAGE_PENDING){
        	uint8_t start = req->column_start;
			uint8_t end   = start + req->length - 1;
			uint8_t cmds[3] = { COLUMNADDR, start, end };
			if(HAL_I2C_Mem_Write_DMA(
				oled->hi2c,
				oled->oled_dev_address<<1,
				0x00, MEMADD_SIZE_8BIT,
				cmds, 3
			) == HAL_OK){
				req->transfer_state = PAGE_REQ_COL_PENDING;
				__NOP();
			}
        }else if(oled->page_queue[oled->current_page_index].transfer_state == PAGE_REQ_COL_PENDING){
            uint8_t *src = req->use_overlay
                ? oled->frame_buffer_overlay
                : oled->frame_buffer_main;
            uint16_t off = req->page * OLED_WIDTH + req->column_start;
            if(HAL_I2C_Mem_Write_DMA(
                oled->hi2c,
                oled->oled_dev_address<<1,
                0x40, MEMADD_SIZE_8BIT,
                &src[off], req->length
            ) == HAL_OK){

				req->transfer_state = PAGE_REQ_DATA_PENDING;
				__NOP();
            }
        }else if(oled->page_queue[oled->current_page_index].transfer_state == PAGE_REQ_DATA_PENDING){
            // acabó DATA → DONE
            req->transfer_state = PAGE_REQ_DONE;

            // liberar bus y permitir siguiente envío
            if (oled->releaseBusCb) {
                oled->releaseBusCb();
            }
            oled->is_sending = false;
            OLED_DequeuePage(oled);
            __NOP(); //COMO LLEGA A ESTE NOP?
        }
    }
}

void OLED_GrantAccessCallback(OLED_HandleTypeDef *oled) {
	__NOP();
    if (!oled) return;

    // 1) Fase de inicialización: solo enviamos el **primer** comando
    if (!oled->init_done) {
        if (oled->init_idx == 0) {
            uint8_t cmd = oled->init_cmds[0];
            HAL_I2C_Mem_Write_DMA(
                oled->hi2c,
                oled->oled_dev_address << 1,
                0x00,               // control byte: COMANDO
                MEMADD_SIZE_8BIT,
                &cmd,
                1
            );
            oled->init_idx = 1;
            __NOP();
        }
        return;
    }else{
		OLED_SendBuffer(oled);
    }
}

/**
 * @brief  Desencola la página actual (debe estar en estado DONE),
 *         avanzando la cabeza de la sub-cola (main u overlay) y la cabeza
 *         de la cola global solo si sus contadores son mayores a 0.
 */
void OLED_DequeuePage(OLED_HandleTypeDef *oled)
{
    // 1) Pedido actual
    OLED_Page_t *req = &oled->page_queue[oled->current_page_index];

    // 2) Sólo movemos la cabeza de la sub-cola que corresponde:
    if (req->use_overlay) {
        // Overlay emplea slots 8..15 en el array, pero ovl_head es 0..7
        if (oled->ovl_count > 0) {
            oled->ovl_head  = (oled->ovl_head  + 1) % OVERLAY_QDEPTH;
            oled->ovl_count--;
        }
    } else {
        // Main emplea slots 0..7
        if (oled->main_count > 0) {
            oled->main_head = (oled->main_head + 1) % MAIN_QDEPTH;
            oled->main_count--;
        }
    }

    // 3) Ya no tocamos ni queue_head ni queue_count aquí,
    //    porque tu lógica de selección (SendBuffer/Update)
    //    sólo mira main_count/ovl_count.
}

HAL_StatusTypeDef OLED_Init(OLED_HandleTypeDef *oled,
                            I2C_HandleTypeDef *hi2c,
                            uint16_t oled_dev_address,
                            volatile uint8_t *dma_busy_flag,
                            I2C_Request_Bus_Use requestBusCbFn,
                            I2C_Release_Bus_Use releaseBusUseFn,
							On_OLED_Ready on_ready_fn)
{
    /* 1) Asignar referencias al bus I²C/DMA */
    oled->hi2c            = hi2c;
    oled->dma_busy_flag   = dma_busy_flag;
    oled->oled_dev_address= oled_dev_address;
    oled->requestBusCb    = requestBusCbFn;
    oled->releaseBusCb    = releaseBusUseFn;
    oled->oled_just_init_fn = on_ready_fn;

    /* 2) Inicializar cola circular MAIN (0–7) y OVERLAY (8–15) */
    oled->queue_head      = 0;
    oled->queue_count     = 0;
    oled->main_head       = 0;
    oled->main_count      = 0;
    oled->ovl_head        = 0;
    oled->current_page_index = 0;

    /* 3) Inicializar secuencia de arranque */
    oled->init_cmds       = ssd1306_init_seq_128x64;
    oled->init_len        = SSD1306_INIT_LEN;
    oled->init_idx        = 0;
    oled->init_done       = false;

    /* 4) Configuración inicial de dibujo */
    oled->font            = &Font_7x10;
    oled->cursor_x        = 0;
    oled->cursor_y        = 0;
    oled->bitmap_opaque   = true;
    oled->font_normal     = true;

    /* 5) Estado de overlay */
    oled->allow_overlay_transfer = false;
    oled->overlay_active        = false;
    oled->overlay_timeout_active= false;
    oled->overlay_time_in_ms    = DEFAULT_OVERLAY_TIME_MS;
    oled->overlay_timer_ms      = DEFAULT_OVERLAY_TIME_MS;
    oled->overlay_hide_now      = false;

    oled->is_sending = false;

    oled->ovl_head   = 0;
    oled->ovl_count  = 0;   // ← inicializar también el contador de overlay
    oled->current_page_index = 0;
    oled->just_finished_init = false;

    return HAL_OK;
}

/**
 * @brief  Inicia la secuencia de inicialización I²C+DMA para el SSD1306 128×64.
 *         Ajusta las tablas y arranca el envío del primer comando.
 */
void initOLEDSequence(OLED_HandleTypeDef *oled)
{
    if (!oled) return;

    // Asignar tabla de init y longitud
    oled->init_cmds = ssd1306_init_seq_128x64;
    oled->init_len  = sizeof(ssd1306_init_seq_128x64);
    oled->init_idx  = 0;
    oled->init_done = false;
    // Solicitar bus I²C para enviar el primer comando de init
    if (oled->requestBusCb) {
        oled->requestBusCb(I2C_REQ_IS_TX);
    }
}

/**
 * @brief  Limpia uno de los frame buffers y encola cada página completa
 *         (todas las columnas) en la sub-cola MAIN u OVERLAY.
 */
void OLED_ClearBuffer(OLED_HandleTypeDef *oled, bool clear_overlay)
{
    if (!oled) return;

    // 1) Reiniciar la sub-cola y el conteo global
    if (clear_overlay) {
        oled->ovl_head  = 0;
        oled->ovl_count = 0;
    } else {
        oled->main_head  = 0;
        oled->main_count = 0;
    }
    // Tras vaciar una sub-cola, la cola global también pierde esas entradas:
    oled->queue_count = 0;
    oled->queue_head  = 0;

    // 2) Borrar el RAM-buffer correspondiente
    uint8_t *buf = clear_overlay
        ? oled->frame_buffer_overlay
        : oled->frame_buffer_main;
    memset(buf, 0, OLED_BUFFER_SIZE);

    // 3) Encolar **todas** las páginas (0..OLED_MAX_PAGES−1)
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        uint8_t pos = clear_overlay
            // Overlay va en ranuras [8..15]
            ? MAIN_QDEPTH + oled->ovl_count
            // Main en ranuras [0..7]
            :        oled->main_count;

        oled->page_queue[pos] = (OLED_Page_t){
            .page           = p,
            .column_start   = 0,
            .length         = OLED_WIDTH,
            .use_overlay    = clear_overlay,
            .transfer_state = PAGE_REQ_WAITING
        };

        if (clear_overlay) {
            oled->ovl_count++;
        } else {
            oled->main_count++;
        }
        oled->queue_count++;
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

/**
 * @brief  Pone un píxel en el búfer (main u overlay) y encola o fusiona
 *         la solicitud para enviar solo el rango mínimo con datos.
 * @param  oled         Puntero al handle.
 * @param  x            Coordenada horizontal (0..OLED_WIDTH-1).
 * @param  y            Coordenada vertical   (0..OLED_HEIGHT-1).
 * @param  use_overlay  false → main, true → overlay.
 */
static void _setPixel(OLED_HandleTypeDef *oled,
                      uint8_t x, uint8_t y,
                      bool use_overlay)
{
    if (!oled || x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }

    // 1) Calcula página y bit
    uint8_t page = y >> 3;                   // y/8 → página 0..7
    uint8_t bit  = 1 << (y & 0x07);          // y%8 → máscara de bit

    // 2) Selecciona búfer RAM
    uint8_t *buf = use_overlay
        ? oled->frame_buffer_overlay
        : oled->frame_buffer_main;

    // 3) Enciende el bit en el búfer
    buf[page * OLED_WIDTH + x] |= bit;

    // 4) Escaneo borde-a-borde para rango mínimo
    uint8_t *page_buf = &buf[page * OLED_WIDTH];
    int16_t first_col = -1, last_col = -1;
    // izquierda → derecha
    for (int i = 0; i < OLED_WIDTH; i++) {
        if (page_buf[i]) { first_col = i; break; }
    }
    if (first_col < 0) {
        // Página vacía: nada que enviar
        return;
    }
    // derecha → izquierda
    for (int i = OLED_WIDTH - 1; i >= 0; i--) {
        if (page_buf[i]) { last_col = i; break; }
    }
    uint8_t length = last_col - first_col + 1;

    // 5) Determina sub-cola y fusiona si existe
    if (!use_overlay) {
        // —— MAIN sub-cola ——
        for (uint8_t i = 0; i < oled->main_count; i++) {
            uint8_t idx = (oled->main_head + i) % MAIN_QDEPTH;
            OLED_Page_t *req = &oled->page_queue[idx];
            if (req->page == page
             && req->use_overlay == false
             && req->transfer_state == PAGE_REQ_WAITING)
            {
                req->column_start = first_col;
                req->length       = length;
                return;
            }
        }
        // Si no existía y hay espacio en MAIN
        if (oled->main_count < MAIN_QDEPTH) {
            uint8_t pos = (oled->main_head + oled->main_count) % MAIN_QDEPTH;
            oled->page_queue[pos] = (OLED_Page_t){
                .page           = page,
                .column_start   = first_col,
                .length         = length,
                .use_overlay    = false,
                .transfer_state = PAGE_REQ_WAITING
            };
            oled->main_count++;
            oled->queue_count++;
        }
    } else {
        // —— OVERLAY sub-cola ——
        for (uint8_t i = 0; i < oled->ovl_count; i++) {
            uint8_t idx = MAIN_QDEPTH + ((oled->ovl_head + i) % OVERLAY_QDEPTH);
            OLED_Page_t *req = &oled->page_queue[idx];
            if (req->page == page
             && req->use_overlay == true
             && req->transfer_state == PAGE_REQ_WAITING)
            {
                req->column_start = first_col;
                req->length       = length;
                return;
            }
        }
        // Si no existía y hay espacio en OVERLAY
        if (oled->ovl_count < OVERLAY_QDEPTH) {
            uint8_t pos = MAIN_QDEPTH + ((oled->ovl_head + oled->ovl_count) % OVERLAY_QDEPTH);
            oled->page_queue[pos] = (OLED_Page_t){
                .page           = page,
                .column_start   = first_col,
                .length         = length,
                .use_overlay    = true,
                .transfer_state = PAGE_REQ_WAITING
            };
            oled->ovl_count++;
            oled->queue_count++;
        }
    }
}


void OLED_DrawPixel(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y,
                    bool use_overlay)
{
    _setPixel(oled, x, y, use_overlay);
}

// --- Líneas  --------------------------------------------------------------------------------

void OLED_DrawHLine(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y, uint8_t w,
                    bool use_overlay)
{
    if (!oled || y >= OLED_HEIGHT) return;
    for (uint8_t i = 0; i < w; i++) {
        if (x + i < OLED_WIDTH)
            OLED_DrawPixel(oled, x + i, y, use_overlay);
    }
}

void OLED_DrawVLine(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y, uint8_t h,
                    bool use_overlay)
{
    if (!oled || x >= OLED_WIDTH) return;
    for (uint8_t i = 0; i < h; i++) {
        if (y + i < OLED_HEIGHT)
            OLED_DrawPixel(oled, x, y + i, use_overlay);
    }
}

// Bresenham
void OLED_DrawLineXY(OLED_HandleTypeDef *oled,
                     uint8_t x0, uint8_t y0,
                     uint8_t x1, uint8_t y1,
                     bool use_overlay)
{
    if (!oled) return;
    int dx = abs((int)x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs((int)y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (true) {
        if (x0 < OLED_WIDTH && y0 < OLED_HEIGHT)
            OLED_DrawPixel(oled, x0, y0, use_overlay);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

// --- Rectángulos ----------------------------------------------------------------------------

void OLED_DrawBox(OLED_HandleTypeDef *oled,
                  uint8_t x, uint8_t y,
                  uint8_t w, uint8_t h,
                  bool use_overlay)
{
    if (!oled) return;
    for (uint8_t yy = y; yy < y + h && yy < OLED_HEIGHT; yy++) {
        for (uint8_t xx = x; xx < x + w && xx < OLED_WIDTH; xx++) {
            OLED_DrawPixel(oled, xx, yy, use_overlay);
        }
    }
}

void OLED_DrawFrame(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y,
                    uint8_t w, uint8_t h,
                    bool use_overlay)
{
    OLED_DrawHLine(oled, x,     y,     w, use_overlay);
    OLED_DrawHLine(oled, x,     y + h - 1, w, use_overlay);
    OLED_DrawVLine(oled, x,     y,     h, use_overlay);
    OLED_DrawVLine(oled, x + w - 1, y,     h, use_overlay);
}

// --- Círculos y discos ----------------------------------------------------------------------

void OLED_DrawCircle(OLED_HandleTypeDef *oled,
                     uint8_t x0, uint8_t y0,
                     uint8_t rad,
                     bool use_overlay)
{
    if (!oled) return;
    int f = 1 - rad, ddF_x = 1, ddF_y = -2 * rad;
    int x = 0, y = rad;
    OLED_DrawPixel(oled, x0,    y0 + rad, use_overlay);
    OLED_DrawPixel(oled, x0,    y0 - rad, use_overlay);
    OLED_DrawPixel(oled, x0 + rad, y0,    use_overlay);
    OLED_DrawPixel(oled, x0 - rad, y0,    use_overlay);
    while (x < y) {
        if (f >= 0) {
            y--; ddF_y += 2; f += ddF_y;
        }
        x++; ddF_x += 2; f += ddF_x;
        OLED_DrawPixel(oled, x0 + x, y0 + y, use_overlay);
        OLED_DrawPixel(oled, x0 - x, y0 + y, use_overlay);
        OLED_DrawPixel(oled, x0 + x, y0 - y, use_overlay);
        OLED_DrawPixel(oled, x0 - x, y0 - y, use_overlay);
        OLED_DrawPixel(oled, x0 + y, y0 + x, use_overlay);
        OLED_DrawPixel(oled, x0 - y, y0 + x, use_overlay);
        OLED_DrawPixel(oled, x0 + y, y0 - x, use_overlay);
        OLED_DrawPixel(oled, x0 - y, y0 - x, use_overlay);
    }
}

void OLED_DrawDisc(OLED_HandleTypeDef *oled,
                   uint8_t x0, uint8_t y0,
                   uint8_t rad,
                   bool use_overlay)
{
    if (!oled) return;
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = (int)floorf(sqrtf((rad * rad) - (dy * dy)));
        OLED_DrawHLine(oled, x0 - dx, y0 + dy, dx * 2 + 1, use_overlay);
    }
}

// --- Arcos y elipses -------------------------------------------------------------------------

void OLED_DrawArc(OLED_HandleTypeDef *oled,
                  uint8_t x0, uint8_t y0,
                  uint8_t rad,
                  uint8_t start, uint8_t end,
                  bool use_overlay)
{
    if (!oled) return;
    float a0 = (start / 256.0f) * 2 * M_PI;
    float a1 = (end   / 256.0f) * 2 * M_PI;
    for (float a = a0; a <= a1; a += 0.01f) {
        int16_t x = x0 + (int16_t)(rad * cosf(a));
        int16_t y = y0 + (int16_t)(rad * sinf(a));
        if (x >= 0 && x < OLED_WIDTH && y >= 0 && y < OLED_HEIGHT)
            OLED_DrawPixel(oled, x, y, use_overlay);
    }
}

void OLED_DrawEllipse(OLED_HandleTypeDef *oled,
                      uint8_t x0, uint8_t y0,
                      uint8_t rx, uint8_t ry,
                      bool use_overlay)
{
    if (!oled) return;
    for (int16_t deg = 0; deg < 360; deg++) {
        float r = deg * (M_PI / 180.0f);
        int16_t x = x0 + (int16_t)(rx * cosf(r));
        int16_t y = y0 + (int16_t)(ry * sinf(r));
        if (x >= 0 && x < OLED_WIDTH && y >= 0 && y < OLED_HEIGHT)
            OLED_DrawPixel(oled, x, y, use_overlay);
    }
}

void OLED_DrawFilledEllipse(OLED_HandleTypeDef *oled,
                            uint8_t x0, uint8_t y0,
                            uint8_t rx, uint8_t ry,
                            bool use_overlay)
{
    if (!oled) return;
    for (int16_t dy = -ry; dy <= ry; dy++) {
        int16_t dx = (int16_t)(rx * sqrtf(1.0f - (dy / (float)ry) * (dy / (float)ry)));
        OLED_DrawHLine(oled, x0 - dx, y0 + dy, dx * 2 + 1, use_overlay);
    }
}

// --- Triángulos -----------------------------------------------------------------------------

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

// --- Mapas de bits --------------------------------------------------------------------------

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
    if (!oled) return;
    for (uint8_t row = 0; row < h; row++) {
        for (uint8_t col = 0; col < w; col++) {
            uint8_t byte = bitmap[(col / 8) + row * (w / 8)];
            if (byte & (1 << (col % 8))) {
                OLED_DrawPixel(oled, x + col, y + row, use_overlay);
            }
        }
    }
}

// --- Texto ----------------------------------------------------------------------------------

void OLED_DrawStr(OLED_HandleTypeDef *oled,
                  const char *str,
                  bool use_overlay)
{
    if (!oled || !str || !oled->font) return;
    FontDef *f = oled->font;
    uint8_t cx = oled->cursor_x;
    uint8_t cy = oled->cursor_y;

    while (*str) {
        uint8_t c = (uint8_t)*str++;
        if (c < 32 || c > 126) c = '?';
        // Apunta al primer byte de este glifo
        const uint16_t *glyph = &f->data[(c - 32) * f->FontHeight];

        // Para cada fila del glifo
        for (uint8_t row = 0; row < f->FontHeight; row++) {
            uint16_t bits = glyph[row];
            // Arrancamos en el MSB correspondiente al ancho de la fuente
            uint16_t mask = 1 << (f->FontWidth - 1);
            for (uint8_t col = 0; col < f->FontWidth; col++) {
                if (bits & mask) {
                    OLED_DrawPixel(oled, cx + col, cy + row, use_overlay);
                }
                mask >>= 1;  // avanzamos de MSB a LSB
            }
        }

        // Avanzamos el cursor en X
        cx += f->FontWidth;
        if (cx + f->FontWidth > OLED_WIDTH) break;
    }
    // Salvamos la nueva posición
    oled->cursor_x = cx;
}

/**
 * @brief  Limpia un rectángulo de la pantalla y encola solo las regiones afectadas.
 * @param  oled         Puntero al handle
 * @param  x            Coordenada X de la esquina superior izquierda
 * @param  y            Coordenada Y de la esquina superior izquierda
 * @param  w            Ancho del rectángulo (px)
 * @param  h            Alto  del rectángulo (px)
 * @param  use_overlay  false=limpia en buffer MAIN, true=limpia en OVERLAY
 */
void OLED_ClearBox(OLED_HandleTypeDef *oled,
                   uint8_t x, uint8_t y,
                   uint8_t w, uint8_t h,
                   bool use_overlay)
{
    if (!oled) return;
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;

    // Limitar a los bordes
    uint8_t x_end = (x + w > OLED_WIDTH)  ? OLED_WIDTH  : x + w;
    uint8_t y_end = (y + h > OLED_HEIGHT) ? OLED_HEIGHT : y + h;

    // 1) Borrar bits en el framebuffer
    uint8_t *buf = use_overlay
        ? oled->frame_buffer_overlay
        : oled->frame_buffer_main;

    for (uint8_t yy = y; yy < y_end; yy++) {
        uint8_t page = yy >> 3;
        uint8_t mask = ~(1 << (yy & 0x07));
        uint16_t base = page * OLED_WIDTH;
        for (uint8_t xx = x; xx < x_end; xx++) {
            buf[base + xx] &= mask;
        }
    }

    // 2) Encolar cada página afectada con el rango [x .. x_end-1]
    uint8_t page_start =  y     >> 3;
    uint8_t page_last  = (y_end-1) >> 3;
    uint8_t first_col  = x;
    uint8_t length     = x_end - x;

    // Sub-cola MAIN u OVERLAY
    uint8_t head   = use_overlay ? oled->ovl_head  : oled->main_head;
    uint8_t count  = use_overlay ? oled->ovl_count : oled->main_count;
    uint8_t base   = use_overlay ? MAIN_QDEPTH      : 0;
    uint8_t depth  = MAIN_QDEPTH;  // ambas de tamaño 8

    for (uint8_t p = page_start; p <= page_last; p++) {
        bool merged = false;

        // Intentar fusionar con una request pendiente
        for (uint8_t i = 0; i < count; i++) {
            uint8_t idx = base + ((head + i) % depth);
            OLED_Page_t *req = &oled->page_queue[idx];
            if (req->page == p
             && req->use_overlay == use_overlay
             && req->transfer_state == PAGE_REQ_WAITING)
            {
                // Reemplazar con el nuevo rango completo
                req->column_start = first_col;
                req->length       = length;
                merged = true;
                break;
            }
        }

        // Si no estaba en cola, agregarlo
        if (!merged && count < depth) {
            uint8_t pos = base + ((head + count) % depth);
            oled->page_queue[pos] = (OLED_Page_t){
                .page           = p,
                .column_start   = first_col,
                .length         = length,
                .use_overlay    = use_overlay,
                .transfer_state = PAGE_REQ_WAITING
            };
            if (use_overlay) {
                oled->ovl_count++;
            } else {
                oled->main_count++;
            }
            oled->queue_count++;
            count++;
        }
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

void OLED_ChangeOverlayTime(OLED_HandleTypeDef *oled, uint16_t duration_ms) {
	if (duration_ms == 0) {
		oled->overlay_timer_ms = DEFAULT_OVERLAY_TIME_MS;
		oled->overlay_time_in_ms = DEFAULT_OVERLAY_TIME_MS;
	} else {
		oled->overlay_timer_ms = duration_ms;
		oled->overlay_time_in_ms = duration_ms;
	}
}

/**
 * @brief Activa el overlay, lo limpia y encola todas sus páginas completas.
 * @param oled        Puntero al handle
 * @param duration_ms Duración en ms (0 = sin timeout)
 */
void OLED_ActivateOverlay(OLED_HandleTypeDef *oled, uint16_t duration_ms) {
    if (!oled) return;

    // 1) Activar overlay y permitir su transferencia
    oled->overlay_active         = true;
    oled->allow_overlay_transfer = true;

    // 2) Configurar timeout de overlay
    if (duration_ms == 0) {
        oled->overlay_timeout_active = false;
    } else {
        oled->overlay_timeout_active = true;
        oled->overlay_time_in_ms     = duration_ms;
        oled->overlay_timer_ms       = duration_ms;
    }

    // 3) Limpiar y encolar toda la capa OVERLAY
    //    OLED_ClearBuffer(oled, true) hace:
    //      - memset(frame_buffer_overlay, 0, ...)
    //      - encola páginas 0..7 en la sub-cola OVERLAY (índices 8–15)
    OLED_ClearBuffer(oled, true);
}



/* ============================================================================ */
