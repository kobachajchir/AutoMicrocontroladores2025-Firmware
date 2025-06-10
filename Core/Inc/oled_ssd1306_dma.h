/*
 * oled_ssd1306_dma.h
 *
 *  Created on: Jun 9, 2025
 *      Author: kobac
 */

#ifndef INC_OLED_SSD1306_DMA_H_
#define INC_OLED_SSD1306_DMA_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/** Dimensiones del display */
#define OLED_WIDTH            128
#define OLED_HEIGHT           64
#define OLED_BUFFER_SIZE      (OLED_WIDTH * OLED_HEIGHT / 8)
#define OLED_MAX_PAGES        (OLED_HEIGHT / 8)    // 8 páginas
#define OLED_QUEUE_DEPTH      8

/** Descriptor de página para cola de actualizaciones */
typedef struct {
    uint8_t page;           /**< Página 0..7 */
    uint8_t column_start;   /**< Columna inicial 0..127 */
    uint8_t length;         /**< Número de bytes a enviar */
    bool    use_overlay;    /**< true=overlay, false=main */
} OLED_Page_t;

/** Handle del driver OLED */
typedef struct {
    uint8_t frame_buffer_main[OLED_BUFFER_SIZE];    /**< Contenido principal */
    uint8_t frame_buffer_overlay[OLED_BUFFER_SIZE]; /**< Contenido temporal */

    bool    page_dirty[OLED_MAX_PAGES];             /**< Flags de páginas modificadas */

    bool    overlay_active;      /**< Overlay activo */
    uint16_t overlay_timer_ms;   /**< Tiempo restante (ms) */

    /* Cola circular de requests de página */
    OLED_Page_t page_queue[OLED_QUEUE_DEPTH];
    uint8_t     queue_head;
    uint8_t     queue_tail;
    uint8_t     queue_count;

    /* I2C y DMA */
    I2C_HandleTypeDef *hi2c;           /**< Handle I2C (DMA) */
    volatile uint8_t  *dma_busy_flag;  /**< Puntero a flag externo */
} OLED_HandleTypeDef;

/**
 * @brief Inicializa el driver OLED
 * @param oled             puntero al handle
 * @param hi2c             handle de I2C para DMA
 * @param dma_busy_flag    puntero a uint8_t externo (0 libre, 1 ocupado)
 * @retval HAL_OK siempre
 */
HAL_StatusTypeDef OLED_Init(OLED_HandleTypeDef *oled,
                            I2C_HandleTypeDef *hi2c,
                            volatile uint8_t  *dma_busy_flag);

/** Borra buffer principal u overlay y marca páginas dirty */
void OLED_ClearBuffer(OLED_HandleTypeDef *oled, bool clear_overlay);

/** Dibuja texto en buffer activo y marca dirty de páginas */
void OLED_DrawStr(OLED_HandleTypeDef *oled,
                  uint8_t x, uint8_t y,
                  const char *str);

/** Dibuja bitmap en buffer activo y marca dirty de páginas */
void OLED_DrawBitmap(OLED_HandleTypeDef *oled,
                     uint8_t x, uint8_t y,
                     const uint8_t *bmp,
                     uint16_t size);

/**
 * @brief Encola páginas dirty con columnas específicas y limpia flags
 * @retval HAL_OK si se encoló o no había cambios
 *         HAL_BUSY si la cola está llena
 */
HAL_StatusTypeDef OLED_SendBuffer(OLED_HandleTypeDef *oled);

/**
 * @brief Tarea a invocar continuamente desde main
 *        - Internamente envía sólo cada 10 ms
 *        - Gestiona overlay y desmonta overlay cuando expire
 *        - Inicia transferencias DMA si el bus está libre
 */
void OLED_MainTask(OLED_HandleTypeDef *oled);

/**
 * @brief Activa un overlay temporal
 * @param data         puntero a BUFFER_SIZE bytes
 * @param duration_ms  duración en milisegundos
 */
void OLED_ActivateOverlay(OLED_HandleTypeDef *oled,
                          const uint8_t *data,
                          uint16_t duration_ms);

/**
 * @brief Callback a invocar desde DMA IRQ Handler cuando completa
 * @param oled  puntero al handle
 */
void OLED_DMA_CompleteCallback(OLED_HandleTypeDef *oled);

#endif /* INC_OLED_SSD1306_DMA_H_ */
