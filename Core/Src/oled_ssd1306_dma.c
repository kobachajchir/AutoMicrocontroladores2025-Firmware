#include "oled_ssd1306_dma.h"
#include <string.h>

static OLED_HandleTypeDef *oled_handle_global = NULL;

/**
 * @brief Inicia transferencia DMA de la siguiente página en cola
 */
static void OLED_StartNextTransfer(void) {
    if (!oled_handle_global) return;
    /* Verificar DMA libre */
    if (*(oled_handleTypeDef->dma_busy_flag)) return;
    /* Verificar cola */
    if (oled_handle_global->queue_count == 0) return;

    /* Tomar request */
    OLED_Page_t req = oled_handle_global->page_queue[oled_handle_global->queue_head];
    uint8_t *buffer = req.use_overlay ?
                        oled_handle_global->frame_buffer_overlay :
                        oled_handle_global->frame_buffer_main;
    uint16_t offset = req.page * OLED_WIDTH + req.column_start;

    /* Iniciar DMA I2C del fragmento */
    if (HAL_I2C_Master_Transmit_DMA(
            oled_handle_global->hi2c,
            /* Dirección con control byte */ 0x78,
            &buffer[offset],
            req.length) == HAL_OK) {
        *(oled_handle_global->dma_busy_flag) = 1;
    }
}

static void OLED_DequeuePage(void) {
    if (oled_handle_global->queue_count == 0) return;
    oled_handle_global->queue_head = (oled_handle_global->queue_head + 1) % OLED_QUEUE_DEPTH;
    oled_handle_global->queue_count--;
}

void OLED_DMA_CompleteCallback(OLED_HandleTypeDef *oled) {
    /* Limpiar busy flag */
    *(oled_handleTypeDef->dma_busy_flag) = 0;
    /* Desencolar la página enviada */
    OLED_DequeuePage();
    /* Intentar la siguiente transferencia */
    OLED_StartNextTransfer();
}

HAL_StatusTypeDef OLED_Init(OLED_HandleTypeDef *oled,
                            I2C_HandleTypeDef *hi2c,
                            volatile uint8_t  *dma_busy_flag) {
    /* Asignar referencias */
    oled_handle_global = oled;
    oled->hi2c = hi2c;
    oled->dma_busy_flag = dma_busy_flag;

    /* Inicializar flags y cola */
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        oled->page_dirty[p] = true;
    }
    oled->queue_head = oled->queue_tail = oled->queue_count = 0;
    oled->overlay_active = false;
    *(oled_handleTypeDef->dma_busy_flag) = 0;

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
    /* TODO: rasterizar texto y actualizar buffer */
    /* Ejemplo: */
    uint8_t page = y >> 3;
    oled->page_dirty[page] = true;
    (void)oled; (void)x; (void)y; (void)str;
}

void OLED_DrawBitmap(OLED_HandleTypeDef *oled, uint8_t x, uint8_t y,
                     const uint8_t *bmp, uint16_t size) {
    /* TODO: copiar bmp y actualizar dirty flags */
    uint8_t page = y >> 3;
    oled->page_dirty[page] = true;
    (void)oled; (void)x; (void)y; (void)bmp; (void)size;
}

HAL_StatusTypeDef OLED_SendBuffer(OLED_HandleTypeDef *oled) {
    bool any = false;
    for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
        if (oled->page_dirty[p]) {
            if (oled->queue_count >= OLED_QUEUE_DEPTH) return HAL_BUSY;
            /* Crear request para columna completa o parte, aquí ejemplo full */
            oled->page_queue[oled->queue_tail] = (OLED_Page_t){
                .page = p,
                .column_start = 0,
                .length = OLED_WIDTH,
                .use_overlay = oled->overlay_active
            };
            oled->queue_tail = (oled->queue_tail + 1) % OLED_QUEUE_DEPTH;
            oled->queue_count++;
            oled->page_dirty[p] = false;
            any = true;
        }
    }
    if (any) {
        OLED_StartNextTransfer();
        return HAL_OK;
    }
    return HAL_OK;
}

void OLED_MainTask(OLED_HandleTypeDef *oled) {
    static uint32_t last = 0;
    uint32_t now = HAL_GetTick();
    if (now - last < 10) return;
    last = now;

    /* Gestionar overlay timeout */
    if (oled->overlay_active) {
        if (oled->overlay_timer_ms >= 10)
            oled->overlay_timer_ms -= 10;
        else {
            oled->overlay_active = false;
            for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
                oled->page_dirty[p] = true;
            }
            OLED_SendBuffer(oled);
        }
    }
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
    OLED_StartNextTransfer();
}
