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
#include <math.h>
#include "fonts.h"

/** Dimensiones del display */
#define OLED_WIDTH            128
#define OLED_HEIGHT           64
#define OLED_BUFFER_SIZE      (OLED_WIDTH * OLED_HEIGHT / 8)
#define OLED_MAX_PAGES        (OLED_HEIGHT / 8)    // 8 páginas
#define OLED_QUEUE_DEPTH      8

#define MEMADD_SIZE_8BIT I2C_MEMADD_SIZE_8BIT

// Ajusta estos valores al layout de tu pantalla
#define Y_OFFSET   12
#define Y_SPACING  12

typedef void (*I2C_Request_Bus_Use)(void);

/** Descriptor de página para cola de actualizaciones */
typedef struct {
    uint8_t page;           /**< Página 0..7 */
    uint8_t column_start;   /**< Columna inicial 0..127 */
    uint8_t length;         /**< Número de bytes a enviar */
    bool    use_overlay;    /**< true=overlay, false=main */
} OLED_Page_t;

/** Handle del driver OLED */
typedef struct {
	const uint8_t *init_cmds;   // puntero a la tabla de comandos
	uint8_t        init_len;    // longitud de la tabla
	uint8_t        init_idx;    // índice actual en la init
	bool           init_done;   // marca cuando la init terminó
    uint8_t frame_buffer_main[OLED_BUFFER_SIZE];    /**< Contenido principal */
    uint8_t frame_buffer_overlay[OLED_BUFFER_SIZE]; /**< Contenido temporal */
    bool    page_dirty_main[OLED_MAX_PAGES];             /**< Flags de páginas modificadas */
    bool    page_dirty_overlay[OLED_MAX_PAGES];             /**< Flags de páginas modificadas */
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
    uint16_t oled_dev_address;
    I2C_Request_Bus_Use requestBusCb;
    // Fuentes y cursor
	FontDef           *font;
	uint8_t            cursor_x;
	uint8_t            cursor_y;
	bool        bitmap_opaque;    /**< true=bitmap cubre fondo; false=bitmap transparente */
	bool        font_normal;      /**< true=texto normal; false=texto invertido */
} OLED_HandleTypeDef;

static const uint8_t ssd1306_init_seq[] = {
    0xAE,0xD5,0xF0,0xA8,0x3F,0xD3,0x00,0x40,0x8D,0x14,
    0x20,0x00,0xA1,0xC8,0xDA,0x12,0x81,0xCF,0xD9,0xF1,
    0xDB,0x40,0xA4,0xA6,0x2E,0xAF
};

#define SSD1306_INIT_LEN  (sizeof(ssd1306_init_seq))

/**
 * @brief Inicializa el driver OLED
 * @param oled             puntero al handle
 * @param hi2c             handle de I2C para DMA
 * @param dma_busy_flag    puntero a uint8_t externo (0 libre, 1 ocupado)
 * @retval HAL_OK siempre
 */
HAL_StatusTypeDef OLED_Init(OLED_HandleTypeDef *oled,
                            I2C_HandleTypeDef *hi2c,
							uint16_t oled_dev_address,
                            volatile uint8_t  *dma_busy_flag, I2C_Request_Bus_Use requestBusCbFn);

/** Borra buffer principal u overlay y marca páginas dirty */
void OLED_ClearBuffer(OLED_HandleTypeDef *oled, bool clear_overlay);

void OLED_DrawPixel(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y,
                    bool use_overlay);

void OLED_DrawHLine(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y, uint8_t w,
                    bool use_overlay);

void OLED_DrawVLine(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y, uint8_t h,
                    bool use_overlay);

void OLED_DrawLineXY(OLED_HandleTypeDef *oled,
                     uint8_t x0, uint8_t y0,
                     uint8_t x1, uint8_t y1,
                     bool use_overlay);

void OLED_DrawBox(OLED_HandleTypeDef *oled,
                  uint8_t x, uint8_t y,
                  uint8_t w, uint8_t h,
                  bool use_overlay);

void OLED_DrawFrame(OLED_HandleTypeDef *oled,
                    uint8_t x, uint8_t y,
                    uint8_t w, uint8_t h,
                    bool use_overlay);

void OLED_DrawCircle(OLED_HandleTypeDef *oled,
                     uint8_t x0, uint8_t y0,
                     uint8_t rad,
                     bool use_overlay);

void OLED_DrawDisc(OLED_HandleTypeDef *oled,
                   uint8_t x0, uint8_t y0,
                   uint8_t rad,
                   bool use_overlay);

void OLED_DrawArc(OLED_HandleTypeDef *oled,
                  uint8_t x0, uint8_t y0,
                  uint8_t rad,
                  uint8_t start, uint8_t end,
                  bool use_overlay);

void OLED_DrawEllipse(OLED_HandleTypeDef *oled,
                      uint8_t x0, uint8_t y0,
                      uint8_t rx, uint8_t ry,
                      bool use_overlay);

void OLED_DrawFilledEllipse(OLED_HandleTypeDef *oled,
                            uint8_t x0, uint8_t y0,
                            uint8_t rx, uint8_t ry,
                            bool use_overlay);

void OLED_DrawTriangle(OLED_HandleTypeDef *oled,
                       int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2,
                       bool use_overlay);

void OLED_DrawBitmap(OLED_HandleTypeDef *oled,
                     uint8_t x, uint8_t y,
                     uint8_t cnt, uint8_t h,
                     const uint8_t *bitmap,
                     bool use_overlay);

void OLED_DrawXBM(OLED_HandleTypeDef *oled,
                  uint8_t x, uint8_t y,
                  uint8_t w, uint8_t h,
                  const uint8_t *bitmap,
                  bool use_overlay);

void OLED_DrawStr(OLED_HandleTypeDef *oled,
                  const char *str,
                  bool use_overlay);

/**
 * @brief Encola páginas dirty con columnas específicas y limpia flags
 * @retval HAL_OK si se encoló o no había cambios
 *         HAL_BUSY si la cola está llena
 */
HAL_StatusTypeDef OLED_SendBuffer(OLED_HandleTypeDef *oled);

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


/**
 * @brief Callback a registrar en I2C_Manager como "grant"
 *        Llama a la función interna que inicia la transferencia
 * @param oled  puntero al handle
 */
void OLED_GrantAccessCallback(OLED_HandleTypeDef *oled);

void OLED_SetCursor(OLED_HandleTypeDef *oled, uint8_t x, uint8_t y);
void OLED_SetFont(OLED_HandleTypeDef *oled, FontDef *font);
/** @brief Define si los bitmaps borran el fondo o no */
void OLED_SetBitmapMode(OLED_HandleTypeDef *oled, bool opaque);
/** @brief Define si el texto se dibuja normal o invertido */
void OLED_SetFontMode  (OLED_HandleTypeDef *oled, bool normal);


#endif /* INC_OLED_SSD1306_DMA_H_ */
