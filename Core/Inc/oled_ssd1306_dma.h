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

typedef union {
    struct {
        uint8_t bit0: 1;  ///< Bit 0  (parte baja)
        uint8_t bit1: 1;  ///< Bit 1  (parte baja)
        uint8_t bit2: 1;  ///< Bit 2  (parte baja)
        uint8_t bit3: 1;  ///< Bit 3  (parte baja)
        uint8_t bit4: 1;  ///< Bit 4  (parte alta)
        uint8_t bit5: 1;  ///< Bit 5  (parte alta)
        uint8_t bit6: 1;  ///< Bit 6  (parte alta)
        uint8_t bit7: 1;  ///< Bit 7  (parte alta)
    } bitmap;            ///< Acceso individual a cada bit
    struct {
        uint8_t bitPairL: 2; ///< Bit pair bajo  (bits 0–1)
        uint8_t bitPairCL: 2; ///< Bit pair centro bajo (bits 2–3)
        uint8_t bitPairCH: 2; ///< Bit pair centro alto  (bits 4–5)
        uint8_t bitPairH: 2; ///< Bit pair alto  (bits 6–7)
    } bitPair;
    struct {
        uint8_t bitL: 4; ///< Nibble bajo  (bits 0–3)
        uint8_t bitH: 4; ///< Nibble alto  (bits 4–7)
    } nibbles;           ///< Acceso a cada nibble
    uint8_t byte;        ///< Acceso completo a los 8 bits (0–255)
} OLED_Byte_Flag_Struct_t;

#define BIT0_MASK   0x01  // 0000 0001
#define BIT1_MASK   0x02  // 0000 0010
#define BIT2_MASK   0x04  // 0000 0100
#define BIT3_MASK   0x08  // 0000 1000
#define BIT4_MASK   0x10  // 0001 0000
#define BIT5_MASK   0x20  // 0010 0000
#define BIT6_MASK   0x40  // 0100 0000
#define BIT7_MASK   0x80  // 1000 0000

// --- Pares de bits (si alguna vez los necesitas) ---
#define BITS01_MASK 0x03  // 0000 0011
#define BITS23_MASK 0x0C  // 0000 1100
#define BITS45_MASK 0x30  // 0011 0000
#define BITS67_MASK 0xC0  // 1100 0000

// --- Nibbles ---
#define NIBBLE_L_MASK 0x0F  // 0000 1111 (bits 0..3)
#define NIBBLE_H_MASK 0xF0  // 1111 0000 (bits 4..7)

#define SET_FLAG(flag_struct, BIT_MASK)    ((flag_struct).byte |=  (uint8_t)(BIT_MASK))
#define CLEAR_FLAG(flag_struct, BIT_MASK)  ((flag_struct).byte &= (uint8_t)(~(BIT_MASK)))
#define TOGGLE_FLAG(flag_struct, BIT_MASK) ((flag_struct).byte ^=  (uint8_t)(BIT_MASK))
#define IS_FLAG_SET(flag_struct, BIT_MASK) (((flag_struct).byte & (BIT_MASK)) != 0U)
#define NIBBLEL_SET_STATE(object, state)  \
    do { \
        (object).byte = (uint8_t)(((object).byte & NIBBLE_H_MASK) | ((uint8_t)((state) & NIBBLE_L_MASK))); \
    } while (0)
#define NIBBLEH_SET_STATE(object, state)  \
    do { \
        (object).byte = (uint8_t)(((object).byte & NIBBLE_L_MASK) | ((uint8_t)(((state) & NIBBLE_L_MASK) << 4))); \
    } while (0)
#define NIBBLEH_GET_STATE(object)  (uint8_t)(((object).byte & NIBBLE_H_MASK) >> 4)
#define NIBBLEL_GET_STATE(object)  (uint8_t)((object).byte & NIBBLE_L_MASK)

/** Dimensiones del display */
#define OLED_WIDTH            128
#define OLED_HEIGHT           64
#define OLED_BUFFER_SIZE      (OLED_WIDTH * OLED_HEIGHT / 8)
#define OLED_MAX_PAGES        8
#define OLED_QUEUE_DEPTH      16 // 8 páginas * 2 buffers
#define OLED_PAGES      	  8

#define MEMADD_SIZE_8BIT I2C_MEMADD_SIZE_8BIT

#define DEFAULT_OVERLAY_TIME_MS 300 //300 ciclos de 10ms = 3000ms = 3s de overlay

typedef void (*I2C_Request_Bus_Use)(uint8_t req_is_tx);
typedef void (*I2C_Release_Bus_Use)(void);

typedef enum {
    PAGE_REQ_WAITING = 0,
    PAGE_REQ_COL_PENDING,
    PAGE_REQ_PAGE_PENDING,
    PAGE_REQ_DATA_PENDING,
    PAGE_REQ_DONE
} OLED_TransferState;

/** Descriptor de página para cola de actualizaciones */
typedef struct {
    uint8_t page;           /**< Página 0..7 */
    uint8_t column_start;   /**< Columna inicial 0..127 */
    uint8_t length;         /**< Número de bytes a enviar */
    bool    use_overlay;    /**< true=overlay, false=main */
    OLED_TransferState transfer_state;
} OLED_Page_t;

/** Handle del driver OLED */
typedef struct {
	const uint8_t *init_cmds;   // puntero a la tabla de comandos
	uint8_t        init_len;    // longitud de la tabla
	uint8_t        init_idx;    // índice actual en la init
	bool           init_done;   // marca cuando la init terminó
	bool           just_finished_init;
	bool 		   first_Fn_Draw;
    uint8_t frame_buffer_main[OLED_BUFFER_SIZE];    /**< Contenido principal */
    uint8_t frame_buffer_overlay[OLED_BUFFER_SIZE]; /**< Contenido temporal */
    bool    page_dirty_main[OLED_MAX_PAGES];             /**< Flags de páginas modificadas */
    bool    page_dirty_overlay[OLED_MAX_PAGES];             /**< Flags de páginas modificadas */
    bool 	allow_overlay_transfer;
    bool    overlay_active;      /**< Overlay activo */
    bool    overlay_timeout_active; //Auto clear overlay
    bool    overlay_hide_now;
    uint16_t overlay_timer_ms;   /**< Tiempo restante (ms) */
    uint16_t overlay_time_in_ms;
    /* Cola circular de requests de página */
    OLED_Page_t page_queue[OLED_QUEUE_DEPTH];
    uint8_t     queue_head;
    uint8_t     queue_tail;
    uint8_t     queue_count;
    uint8_t page_count;
	uint8_t current_page_index;
    /* I2C y DMA */
    I2C_HandleTypeDef *hi2c;           /**< Handle I2C (DMA) */
    volatile uint8_t  *dma_busy_flag;  /**< Puntero a flag externo */
    uint16_t oled_dev_address;
    I2C_Request_Bus_Use requestBusCb;
    I2C_Release_Bus_Use releaseBusCb;
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
                            volatile uint8_t  *dma_busy_flag, I2C_Request_Bus_Use requestBusCbFn, I2C_Release_Bus_Use releaseBusUseFn);

/**
 * @brief   Lanza la secuencia de init por DMA (envío de ssd1306_init_seq).
 *          Debe llamarse justo después de OLED_Init().
 * @param   oled  puntero al handle ya inicializado.
 */
void initOLEDSequence(OLED_HandleTypeDef *oled);

/**
 * @brief Función periódica no bloqueante que gestiona el envío de páginas.
 *        Debe llamarse desde OLED_MainTask().
 */
void OLED_Update(OLED_HandleTypeDef *oled);

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
 * @brief Callback a invocar desde DMA IRQ Handler cuando completa
 * @param oled  puntero al handle
 */
void OLED_DMA_CompleteCallback(OLED_HandleTypeDef *oled, uint8_t is_tx);


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

void OLED_SendCommand_SetColumn(OLED_HandleTypeDef *oled, uint8_t col_start);

void OLED_SendData(OLED_HandleTypeDef *oled, OLED_Page_t *req);

void OLED_SendCommand_SetPage(OLED_HandleTypeDef *oled, uint8_t page);

/**
 * @brief   Marca todas las páginas del buffer overlay como dirty y activa su envío.
 * @param   oled  Puntero al handle OLED.
 * @retval  HAL_StatusTypeDef
 *
 * @note Esta función permite encolar y transferir datos del frame_buffer_overlay.
 *       Una vez que se completen todas las páginas, el flag `allow_overlay_transfer`
 *       se desactiva automáticamente dentro de `OLED_Update()`.
 */
HAL_StatusTypeDef OLED_SendOverlayBuffer(OLED_HandleTypeDef *oled);

/**
 * @brief Oculta inmediatamente el overlay y reenvía el contenido del buffer main.
 * @param oled Puntero al handler del OLED
 */
void OLED_HideOverlayNow(OLED_HandleTypeDef *oled);

/**
 * @brief Activa el overlay y configura el modo de ocultamiento (manual o por timeout).
 *
 * @param oled           Puntero al handle del OLED.
 * @param duration_ms    Tiempo que permanecerá activo el overlay. Si es 0, se esconde solo manualmente.
 */
void OLED_ActivateOverlay(OLED_HandleTypeDef *oled, uint16_t duration_ms);

void OLED_ChangeOverlayTime(OLED_HandleTypeDef *oled, uint16_t duration_ms);

#endif /* INC_OLED_SSD1306_DMA_H_ */
