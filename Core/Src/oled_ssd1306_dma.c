/* ============================================================================
 * oled_ssd1306_dma.c
 * ============================================================================*/
#include "oled_ssd1306_dma.h"
#include "fonts.h"
#include "globals.h"
#include <string.h>
#include <stdlib.h>
#include "math.h"

void OLED_RefreshBoxFromMain(OLED_HandleTypeDef *oled,
                                    uint8_t x, uint8_t y,
                                    uint8_t w, uint8_t h)
{
    if (!oled) return;

    // 1) Límites
    uint8_t x_end = (x + w > OLED_WIDTH) ? OLED_WIDTH : (x + w);
    uint8_t y_end = (y + h > OLED_HEIGHT) ? OLED_HEIGHT : (y + h);

    // 2) Páginas afectadas
    uint8_t page_start =  y      >> 3;
    uint8_t page_last  = (y_end - 1) >> 3;

    for (uint8_t p = page_start; p <= page_last; p++) {
        OLED_Page_t *req = &oled->page_queue[p];
        uint8_t new_start = x;
        uint8_t new_end   = x_end - 1;

        if (req->transfer_state == PAGE_REQ_IDLE) {
            // slot libre → encolamos directamente
            req->page           = p;
            req->column_start   = new_start;
            req->length         = new_end - new_start + 1;
            req->use_overlay    = false;
            req->transfer_state = PAGE_REQ_WAITING;
            if (oled->main_count < MAIN_QDEPTH) {
                oled->main_count++;
                oled->queue_count++;
            }

        } else if (req->transfer_state == PAGE_REQ_WAITING) {
            // ya había una petición waiting → fusionamos rangos
            uint8_t old_start = req->column_start;
            uint8_t old_end   = old_start + req->length - 1;
            uint8_t merged_start = old_start < new_start ? old_start : new_start;
            uint8_t merged_end   = old_end   > new_end   ? old_end   : new_end;
            req->column_start = merged_start;
            req->length       = merged_end - merged_start + 1;

        } else {
            // si ya está en PAGE_PENDING o DATA_PENDING o DONE,
            // no tocamos nada: dejamos que complete su flujo
        }
    }

    // 3) Si no hay transferencia activa, arranca el envío
    if (!oled->is_sending && oled->queue_count > 0) {
        OLED_Update(oled);
    }
}


/**
 * @brief Oculta inmediatamente el overlay en el rectángulo [x..x+w, y..y+h]
 *        y repone la capa MAIN sólo en esa zona.
 */
void OLED_HideOverlayNow(OLED_HandleTypeDef *oled,
                         uint8_t x, uint8_t y,
                         uint8_t w, uint8_t h)
{
    if (!oled) return;

    // 1) desactivar overlay
    oled->overlay_active         = false;
    oled->overlay_timeout_active = false;
    oled->allow_overlay_transfer = false;

    // 2) limpiar sólo la región overlay
    OLED_ClearBox(oled, x, y, w, h, true);

    // 3) re-enviar sólo esa misma región desde el MAIN
    OLED_RefreshBoxFromMain(oled, x, y, w, h);
}

/**
 * @brief  Llama periódicamente para arrancar el envío de la siguiente página.
 *         Solicita el bus I²C solo si no hay nada enviando y hay páginas
 *         pendientes en la sub-cola activa (MAIN u OVERLAY).
 */
void OLED_Update(OLED_HandleTypeDef *oled) {
    if (!oled || !oled->init_done) return;
    if (oled->is_sending)          return;
    if (oled->queue_count == 0)    return;  // nada que enviar
    // pedimos bus para arrancar el GrantAccess → SendBuffer()
    if (oled->requestBusCb) {
        oled->requestBusCb(I2C_REQ_IS_TX);
    }
}

HAL_StatusTypeDef OLED_SendBuffer(OLED_HandleTypeDef *oled) {
    if (!oled)                 return HAL_ERROR;
    if (!oled->init_done)      return HAL_BUSY;
    if (oled->is_sending)      return HAL_BUSY;

    // 1) Determina la sección de la cola fija
    bool useOvl   = oled->overlay_active && oled->allow_overlay_transfer;
    uint8_t baseIdx = useOvl ? MAIN_QDEPTH : 0;           // 0 o 8
    uint8_t endIdx  = baseIdx + MAIN_QDEPTH;              // 8 o 16

    // ← **Breakpoint #1** aquí, justo al entrar:
    //    revisá baseIdx, endIdx, overlay_active, allow_overlay_transfer,
    //    init_done, is_sending

    // 2) Busca linealmente el primer slot WAITING
    uint8_t idxFound = 0xFF;
    for (uint8_t idx = baseIdx; idx < endIdx; idx++) {
    	__NOP();
        // ← **Breakpoint #2** dentro del bucle, para cada idx:
        //    mirá oled->page_queue[idx].transfer_state
        if (oled->page_queue[idx].transfer_state == PAGE_REQ_WAITING) {
            idxFound = idx;
            break;
        }
    }
    if (idxFound == 0xFF) {
    	__NOP();
        // ← **Aquí ves** que idxFound nunca cambió: no hay ningún PAGE_REQ_WAITING
        return HAL_OK;
    }

    // 3) Prepara y lanza el comando SET PAGE
    OLED_Page_t *req = &oled->page_queue[idxFound];
    oled->current_page_index = idxFound;
    __NOP();
    // ← **Breakpoint #3** justo después de asignar current_page_index:
    //    comprobá que idxFound ≠ 0xFF y req->page tenga el valor de página correcto.

    uint8_t page_cmd = 0xB0 | (req->page & 0x07);
    if (HAL_I2C_Mem_Write_DMA(
            oled->hi2c,
            oled->oled_dev_address << 1,
            0x00,               // control byte: COMANDO
            MEMADD_SIZE_8BIT,
            &page_cmd,          // un solo byte de comando
            1
        ) == HAL_OK) {
        req->transfer_state = PAGE_REQ_PAGE_PENDING;
        oled->is_sending    = true;
        __NOP();
    } else {
        return HAL_ERROR;
    }

    return HAL_OK;
}


/**
 * @brief Callback de HAL-DMA cuando termina I2C_TX.
 *        Avanza la state-machine COL→PAGE→DATA→DONE, y al DONE
 *        desencola todas las entradas DONE de la MISMA sub-cola.
 *        Finalmente, encadena el siguiente envío si queda algo.
 */
void OLED_DMA_CompleteCallback(OLED_HandleTypeDef *oled, uint8_t is_tx) {
    if (!oled || !is_tx) return;    // sólo procesar en TX
    __NOP();

    // — Init flow —
    if (!oled->init_done) {
        if (oled->init_idx < oled->init_len) {
            uint8_t cmd = oled->init_cmds[oled->init_idx++];
            HAL_I2C_Mem_Write_DMA(
                oled->hi2c,
                oled->oled_dev_address << 1,
                0x00, MEMADD_SIZE_8BIT,
                &cmd, 1
            );
            __NOP();
        } else {
            // fin de init
            oled->init_done        = true;
            oled->just_finished_init = true;
            __NOP();
            OLED_ClearBuffer(oled, false);
            __NOP();
            oled->is_sending = false;
            OLED_SendBuffer(oled);
        }
    }else{
		// — Página normal —
		OLED_Page_t *req = &oled->page_queue[oled->current_page_index];
		switch (req->transfer_state) {
			case PAGE_REQ_PAGE_PENDING: {
				// enviamos COLUMNADDR
				uint8_t start = req->column_start;
				uint8_t end   = start + req->length - 1;
				uint8_t cmds[3] = { COLUMNADDR, start, end };
				if (HAL_I2C_Mem_Write_DMA(
						oled->hi2c,
						oled->oled_dev_address << 1,
						0x00, MEMADD_SIZE_8BIT,
						cmds, 3
					) == HAL_OK)
				{
					req->transfer_state = PAGE_REQ_COL_PENDING;
					__NOP();
				}
				break;
			}

			case PAGE_REQ_COL_PENDING: {
				// enviamos DATA
				uint8_t *src = req->use_overlay
					? oled->frame_buffer_overlay
					: oled->frame_buffer_main;
				uint16_t off = req->page * OLED_WIDTH + req->column_start;
				if (HAL_I2C_Mem_Write_DMA(
						oled->hi2c,
						oled->oled_dev_address << 1,
						0x40, MEMADD_SIZE_8BIT,
						&src[off], req->length
					) == HAL_OK)
				{
					req->transfer_state = PAGE_REQ_DATA_PENDING;
					__NOP();
				}
				break;
			}

			case PAGE_REQ_DATA_PENDING: {
				// terminamos DATA → DONE
				req->transfer_state = PAGE_REQ_DONE;


				// liberamos bus
				if (oled->releaseBusCb) {
					oled->releaseBusCb();
				}
				__NOP();

				if(oled->init_done && !oled->just_finished_init){
					// aumentamos contador de páginas enviadas
					if(oled->pages_to_send != OLED_PAGES_TO_SEND_NO_LIMIT){
						if (oled->pages_sent_count < oled->pages_to_send) {
							oled->pages_sent_count++;
							__NOP();
						}
						// si ya enviamos todas las páginas, avisamos
						if (oled->pages_sent_count == oled->pages_to_send) {
							if (oled->renderCompleteCb) {
								__NOP();
								oled->renderCompleteCb();
							}
							oled->pages_sent_count = 0;
						}
					}
				}
				// desencolar y preparar siguiente página
				OLED_DequeuePage(oled);
				oled->is_sending = false;
			    if (oled->requestBusCb) {
			        oled->requestBusCb(I2C_REQ_IS_TX);
			    }
				break;
			}

			default:
				break;
		}
    }
}


void OLED_GrantAccessCallback(OLED_HandleTypeDef *oled) {
    __NOP();
    if (!oled) return;

    // 1) ¿Sigue en la fase de init?
    if (!oled->init_done) {
        if (oled->init_idx < oled->init_len) {
            uint8_t cmd = oled->init_cmds[oled->init_idx++];
            HAL_I2C_Mem_Write_DMA(
                oled->hi2c,
                oled->oled_dev_address << 1,
                0x00,               // control byte: COMANDO
                MEMADD_SIZE_8BIT,
                &cmd,
                1
            );
            __NOP();
        }
        return;
    }

    // 2) Sólo arrancamos un nuevo envío si no estamos en medio de uno
    //    y tenemos páginas pendientes (MAIN u OVERLAY)
    if (!oled->is_sending && oled->queue_count > 0) {
        OLED_SendBuffer(oled);
    }
}


/**
 * @brief  Desencola la página que acaba de terminar de enviarse,
 *         marcándola como IDLE para que pueda volver a llenarse más tarde.
 */
void OLED_DequeuePage(OLED_HandleTypeDef *oled)
{
    if (!oled) return;

    uint8_t idx = oled->current_page_index;
    OLED_Page_t *req = &oled->page_queue[idx];

    // 1) Solo desencolamos si acabó de enviarse
    if (req->transfer_state != PAGE_REQ_DONE) {
        return;
    }

    // Guarda si era overlay o main
    bool useOvl = req->use_overlay;

    // 2) Marcamos el slot como “vacío” (IDLE)
    req->transfer_state = PAGE_REQ_IDLE;

    // 2.1) Limpiar la entrada en la cola (resetea campos relevantes)
    req->page         = 0;
    req->column_start = 0;
    req->length       = 0;
    req->use_overlay  = false;

    // 3) Ajustamos el contador correspondiente
    if (useOvl) {
        if (oled->ovl_count > 0) oled->ovl_count--;
    } else {
        if (oled->main_count > 0) oled->main_count--;
    }

    // 4) Si acabamos la init en MAIN con la última página, lanzamos callback
    if (!useOvl
     && oled->just_finished_init
     && idx == (MAIN_QDEPTH - 1))
    {
        __NOP();
        oled->just_finished_init = false;
        if (oled->oled_just_init_fn) {
            oled->oled_just_init_fn();
        }
    }

    // 5) Busca el siguiente slot WAITING en la sección activa
    uint8_t base  = useOvl ? MAIN_QDEPTH : 0;
    uint8_t end   = base + (useOvl ? OVERLAY_QDEPTH : MAIN_QDEPTH);
    uint8_t nextIx = 0xFF;
    for (uint8_t i = base; i < end; i++) {
        if (oled->page_queue[i].transfer_state == PAGE_REQ_WAITING) {
            nextIx = i;
            break;
        }
    }

    // 6) Actualizamos current_page_index
    oled->current_page_index = nextIx;

    // 7) Reducimos el contador global de páginas pendientes
    if (oled->queue_count > 0) {
        oled->queue_count--;
		__NOP();
    }
}

HAL_StatusTypeDef OLED_Init(OLED_HandleTypeDef *oled,
                            I2C_HandleTypeDef *hi2c,
                            uint16_t oled_dev_address,
                            volatile uint8_t *dma_busy_flag,
                            I2C_Request_Bus_Use requestBusCbFn,
                            I2C_Release_Bus_Use releaseBusUseFn,
							On_OLED_Ready on_ready_fn,
							On_OLED_RenderPagesComplete on_render_cmplt_fn)
{
    /* 1) Asignar referencias al bus I²C/DMA */
    oled->hi2c            = hi2c;
    oled->dma_busy_flag   = dma_busy_flag;
    oled->oled_dev_address= oled_dev_address;
    oled->requestBusCb    = requestBusCbFn;
    oled->releaseBusCb    = releaseBusUseFn;
    oled->oled_just_init_fn = on_ready_fn;
    oled->renderCompleteCb  = on_render_cmplt_fn;

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
    oled->font            = &Font_6x12_Min;
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

    oled->pages_to_send     = OLED_PAGES_TO_SEND;
    oled->pages_sent_count  = 0;
    return HAL_OK;
}

/**
 * @brief  Limpia uno de los frame buffers y encola cada página completa
 *         (todas las columnas) en la sub-cola MAIN u OVERLAY.
 */
void OLED_ClearBuffer(OLED_HandleTypeDef *oled, bool clear_overlay)
{
    if (!oled) return;

    // 1) Borrar el RAM-buffer
    uint8_t *buf = clear_overlay
        ? oled->frame_buffer_overlay
        : oled->frame_buffer_main;
    memset(buf, 0, OLED_BUFFER_SIZE);

    if (clear_overlay) {
        // — OVERLAY: slots 8..15 —
        oled->ovl_count = OLED_MAX_PAGES;
        for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
            uint8_t idx = MAIN_QDEPTH + p;
            oled->page_queue[idx] = (OLED_Page_t){
                .page           = p,
                .column_start   = 0,
                .length         = OLED_WIDTH,
                .use_overlay    = true,
                .transfer_state = PAGE_REQ_WAITING
            };
        }
        // además, limpiá MAIN para no arrastrar nada
        for (uint8_t i = 0; i < MAIN_QDEPTH; i++) {
            oled->page_queue[i].transfer_state = PAGE_REQ_IDLE;
        }
        oled->main_count = 0;
    } else {
        // — MAIN: slots 0..7 —
        oled->main_count = OLED_MAX_PAGES;
        for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
            uint8_t idx = p;
            oled->page_queue[idx] = (OLED_Page_t){
                .page           = p,
                .column_start   = 0,
                .length         = OLED_WIDTH,
                .use_overlay    = false,
                .transfer_state = PAGE_REQ_WAITING
            };
        }
        // limpiá OVERLAY también
        for (uint8_t i = MAIN_QDEPTH; i < MAIN_QDEPTH+OVERLAY_QDEPTH; i++) {
            oled->page_queue[i].transfer_state = PAGE_REQ_IDLE;
        }
        oled->ovl_count = 0;
    }

    // 2) Recalcular contador global y resetear cabezas
    oled->queue_count           = oled->main_count + oled->ovl_count;
    oled->main_head             = 0;
    oled->ovl_head              = 0;

    // 3) Arrancar siempre desde el primer slot válido (no 0xFF)
    oled->current_page_index    = 0;
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
void _setPixel(OLED_HandleTypeDef *oled,
                      uint8_t x, uint8_t y,
                      bool use_overlay)
{
    if (!oled || x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }

    // 1) Calcula página y bit
    uint8_t page = y >> 3;                // 0..7
    uint8_t bit  = 1 << (y & 0x07);

    // 2) Enciende el bit en el framebuffer correspondiente
    uint8_t *buf = use_overlay
        ? oled->frame_buffer_overlay
        : oled->frame_buffer_main;
    //buf[page * OLED_WIDTH + x] &= ~bit;  // Limpiar el bit
	buf[page * OLED_WIDTH + x] |= bit;   // Poner el bit

    // 3) Rango mínimo de columnas que tienen datos
    uint8_t *page_buf = &buf[page * OLED_WIDTH];
    int16_t first_col = -1, last_col = -1;
    for (int i = 0; i < OLED_WIDTH; i++) {
        if (page_buf[i]) { first_col = i; break; }
    }
    if (first_col < 0) return;  // nada que enviar
    for (int i = OLED_WIDTH - 1; i >= 0; i--) {
        if (page_buf[i]) { last_col = i; break; }
    }
    uint8_t length = last_col - first_col + 1;

    // 4) Índice FIJO en la cola: main→slot=page, overlay→slot=MAIN_QDEPTH+page
    uint8_t idx = use_overlay
        ? (MAIN_QDEPTH + page)
        : page;

    // 5) Si el slot estaba IDLE, ahora lo encolamos y actualizamos los contadores
    OLED_Page_t *req = &oled->page_queue[idx];
    if (req->transfer_state == PAGE_REQ_IDLE) {
        // Primera vez que se encola esta página
        if (use_overlay) {
            if (oled->ovl_count < OVERLAY_QDEPTH) {
                oled->ovl_count++;
                oled->queue_count++;
            }
        } else {
            if (oled->main_count < MAIN_QDEPTH) {
                oled->main_count++;
                oled->queue_count++;
            }
        }
        req->page = page;
        req->column_start = first_col;
        req->length = length;
        req->use_overlay = use_overlay;
        req->transfer_state = PAGE_REQ_WAITING;
    } else if (req->transfer_state == PAGE_REQ_WAITING) {
        // *** NUEVO: Fusionar con página ya encolada ***
        uint8_t new_start = (req->column_start < first_col) ? req->column_start : first_col;
        uint8_t old_end = req->column_start + req->length - 1;
        uint8_t new_end = first_col + length - 1;
        uint8_t final_end = (old_end > new_end) ? old_end : new_end;

        req->column_start = new_start;
        req->length = final_end - new_start + 1;
        // Ya está en WAITING, no cambiar estado ni contadores
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
    if (!oled || !bitmap) return;

    const uint8_t bytes_per_row = (w + 7) >> 3;          // ceil(w / 8)

    for (uint8_t row = 0; row < h; ++row) {
        const uint8_t *row_ptr = bitmap + row * bytes_per_row;

        for (uint8_t col = 0; col < w; ++col) {
            uint8_t byte = row_ptr[col >> 3];            // col / 8
            if (byte & (1 << (col & 7)))                 // col % 8
                OLED_DrawPixel(oled, x + col, y + row, use_overlay);
        }
    }
}


// --- Texto ----------------------------------------------------------------------------------

/*  fonts_min.h aporta:
 *  - FontMap[128]          (uint8_t)
 *  - FONT_GLYPH_COUNT      (=70)
 *  - Font5x10_Min …        (uint16_t[])
 *  - FontDef { width, height, data }
 *  - FONT_OFFSET_0 / _A / _a
 */

/* -------------------------------------------------------------------- */
void OLED_DrawStr(OLED_HandleTypeDef *oled,
                  const char          *str,
                  bool                use_overlay)
{
    if (!oled || !str || !oled->font) return;

    const FontDef *f  = oled->font;
    uint8_t        cx = oled->cursor_x;
    uint8_t        cy = oled->cursor_y;

    while (*str) {
        /* ---- 1. Traducir código ASCII a índice de glifo ---------------- */
        uint8_t code = (uint8_t)*str++;
        uint8_t idx  = FontMap[code];          // 0‥69  ó  0xFF

        if (idx == 0xFF) {                     // carácter no soportado
            idx = FontMap[(uint8_t)'?'];       //  → usa '?'
            if (idx == 0xFF) continue;         // (seguro, pero por si acaso)
        }

        const uint16_t *glyph = &f->data[idx * f->FontHeight];

        /* ---- 2. Pintar el glifo ---------------------------------------- */
        for (uint8_t row = 0; row < f->FontHeight; ++row) {
            uint16_t bits = glyph[row];
            uint16_t mask = 0x8000;            // empezamos por el MSB

            for (uint8_t col = 0; col < f->FontWidth; ++col, mask >>= 1) {
                if (bits & mask)
                    OLED_DrawPixel(oled, cx + col, cy + row, use_overlay);
            }
        }

        /* ---- 3. Avanzar cursor ---------------------------------------- */
        cx += f->FontWidth;
        if (cx + f->FontWidth > OLED_WIDTH) break;
    }

    oled->cursor_x = cx;
}
/* -------------------------------------------------------------------- */


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

    // 1) Ajustar límites
    uint8_t x_end = (x + w > OLED_WIDTH) ? OLED_WIDTH : (x + w);
    uint8_t y_end = (y + h > OLED_HEIGHT) ? OLED_HEIGHT : (y + h);

    // 2) Borrar bits en el framebuffer
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

    // 3) Determinar páginas y columnas afectadas
    uint8_t page_start =  y      >> 3;
    uint8_t page_last  = (y_end - 1) >> 3;
    uint8_t first_col  = x;
    uint8_t length     = x_end - x;

    // 4) Encolar y actualizar contadores
    uint8_t baseIdx = use_overlay ? MAIN_QDEPTH : 0;
    for (uint8_t p = page_start; p <= page_last; p++) {
        OLED_Page_t *req = &oled->page_queue[baseIdx + p];

        if (req->transfer_state == PAGE_REQ_IDLE) {
            // Nuevo encolado
            req->page           = p;
            req->column_start   = first_col;
            req->length         = length;
            req->use_overlay    = use_overlay;
            req->transfer_state = PAGE_REQ_WAITING;

            if (use_overlay) {
                if (oled->ovl_count < OVERLAY_QDEPTH) {
                    oled->ovl_count++;
                    oled->queue_count++;
                }
            } else {
                if (oled->main_count < MAIN_QDEPTH) {
                    oled->main_count++;
                    oled->queue_count++;
                }
            }

        } else if (req->transfer_state == PAGE_REQ_WAITING) {
            // Fusión con página ya encolada
            uint8_t old_start = req->column_start;
            uint8_t old_end   = old_start + req->length - 1;
            uint8_t new_start = (old_start < first_col) ? old_start : first_col;
            uint8_t new_end   = ((old_end   > (first_col + length - 1))
                                  ? old_end
                                  : (first_col + length - 1));
            req->column_start = new_start;
            req->length       = new_end - new_start + 1;
        }
    }

    // 5) Arrancar envío si estaba idle
    if (!oled->is_sending && oled->queue_count > 0) {
        OLED_Update(oled);
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
}



/* ============================================================================ */
