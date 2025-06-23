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
#define OLED_PAGES      	  8
#define MAIN_QDEPTH    8
#define OVERLAY_QDEPTH 8
#define OLED_QUEUE_DEPTH 16

#define MEMADD_SIZE_8BIT I2C_MEMADD_SIZE_8BIT

#define DEFAULT_OVERLAY_TIME_MS 300 //300 ciclos de 10ms = 3000ms = 3s de overlay

typedef void (*I2C_Request_Bus_Use)(uint8_t req_is_tx);
typedef void (*I2C_Release_Bus_Use)(void);
typedef void (*On_OLED_Ready)(void);

typedef enum {
    PAGE_REQ_IDLE,        // slot libre, nada pendiente
    PAGE_REQ_WAITING,     // datos nuevos en RAM, hay que enviar
    PAGE_REQ_PAGE_PENDING,
    PAGE_REQ_COL_PENDING,
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

/** Handle del driver OLED, refactorizado para sub-colas MAIN/OVERLAY */
typedef struct {
    /* Secuencia de inicialización */
    const uint8_t *init_cmds;   /**< Puntero a la tabla de comandos de init */
    uint8_t        init_len;    /**< Longitud de la tabla de init */
    uint8_t        init_idx;    /**< Índice actual en la init */
    bool           init_done;   /**< true cuando la init finalizó */
    bool           just_finished_init;
    bool           first_Fn_Draw;

    /* Frame buffers en RAM */
    uint8_t frame_buffer_main[OLED_BUFFER_SIZE];    /**< Contenido principal */
    uint8_t frame_buffer_overlay[OLED_BUFFER_SIZE]; /**< Contenido overlay */

    /* Control de overlay */
    bool    allow_overlay_transfer;
    bool    overlay_active;
    bool    overlay_timeout_active;
    bool    overlay_hide_now;
    uint16_t overlay_timer_ms;
    uint16_t overlay_time_in_ms;

    /* Cola circular de solicitudes (0–7 MAIN, 8–15 OVERLAY) */
    OLED_Page_t page_queue[OLED_QUEUE_DEPTH];
    uint8_t     queue_head;    /**< Índice de extracción global */
    uint8_t     queue_count;   /**< Número total de requests pendientes */
    uint8_t     main_head;     /**< Head de sub-cola MAIN (0..7) */
    uint8_t     main_count;    /**< Count de sub-cola MAIN */
    uint8_t     ovl_head;      /**< Head de sub-cola OVERLAY (8..15) */
    uint8_t     ovl_count;     /**< Count de sub-cola OVERLAY */

    uint8_t     current_page_index; /**< Última entrada procesada */

    /* I2C y DMA */
    I2C_HandleTypeDef    *hi2c;
    volatile uint8_t     *dma_busy_flag;
    uint16_t              oled_dev_address;
    I2C_Request_Bus_Use   requestBusCb;
    I2C_Release_Bus_Use   releaseBusCb;
    On_OLED_Ready oled_just_init_fn;

    /* Fuentes y cursor */
    FontDef *font;
    uint8_t  cursor_x;
    uint8_t  cursor_y;
    bool     bitmap_opaque;
    bool     font_normal;
    bool is_sending;
} OLED_HandleTypeDef;


// Display commands
#define CHARGEPUMP            0x8D
#define COLUMNADDR            0x21
#define PAGEADDR              0x22
#define MEMORYMODE            0x20
#define SEGREMAP_NORMAL       0xA0  // column address 0 → SEG0 (left-to-right)
#define SEGREMAP_INVERT       0xA1  // column address 127 → SEG0 (right-to-left)
#define COMSCANINC            0xC0  // COM scan direction: row 0 at top
#define COMSCANDEC            0xC8  // COM scan direction: row 0 at bottom (vertical mirror)
#define SETCOMPINS            0xDA
#define SETCONTRAST           0x81
#define SETDISPLAYCLOCKDIV    0xD5
#define SETDISPLAYOFFSET      0xD3
#define SETSTARTLINE          0x40
#define CHARGEPUMP            0x8D
#define SETMULTIPLEX          0xA8
#define SETPRECHARGE          0xD9
#define SETVCOMDETECT         0xDB
#define DISPLAYALLON_RESUME   0xA4
#define NORMALDISPLAY         0xA6
#define INVERTDISPLAY         0xA7
#define DISPLAYOFF            0xAE
#define DISPLAYON             0xAF
#define DEACTIVATE_SCROLL     0x2E

static const uint8_t ssd1306_init_seq_128x64[] = {
    DISPLAYOFF,                // 0xAE: apagar pantalla
    SETDISPLAYCLOCKDIV, 0xF0,  // 0xD5,0xF0: reloj al máximo (~96 Hz)
    SETMULTIPLEX,       0x3F,  // 0xA8,0x3F: multiplexado = 64-1
    SETDISPLAYOFFSET,   0x00,  // 0xD3,0x00: offset = 0
    SETSTARTLINE,              // 0x40: start line = 0
    CHARGEPUMP,         0x14,  // 0x8D,0x14: enable charge pump
    MEMORYMODE,         0x02,  // 0x20,0x02: addressing mode = page addressing
    SEGREMAP_INVERT,           // 0xA0: column 0→SEG0 (normal, izquierda→derecha)
    COMSCANDEC,                // 0xC0: COM scan dir = increment (fila 0 arriba)
    SETCOMPINS,         0x12,  // 0xDA,0x12: config pins para 128×64
    SETCONTRAST,        0xCF,  // 0x81,0xCF: contraste para 128×64
    SETPRECHARGE,       0xF1,  // 0xD9,0xF1: fase de pre-carga
    SETVCOMDETECT,      0x40,  // 0xDB,0x40: nivel VCOM
    DISPLAYALLON_RESUME,       // 0xA4: resume RAM display
    NORMALDISPLAY,             // 0xA6: display normal
    DEACTIVATE_SCROLL,         // 0x2E: detener scroll
    DISPLAYON                  // 0xAF: encender pantalla
};

// Asegúrate de definir SSD1306_HEIGHT = 64 antes de incluir este array
#define SSD1306_INIT_LEN  (sizeof(ssd1306_init_seq_128x64))

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
                            volatile uint8_t  *dma_busy_flag, I2C_Request_Bus_Use requestBusCbFn,
							I2C_Release_Bus_Use releaseBusUseFn,
							On_OLED_Ready on_ready_fn);

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

/**
 * @brief  Pone un píxel en el búfer (main o overlay) y encola inmediatamente
 *         la página con el rango mínimo de columnas que contiene datos.
 * @param  oled         Puntero al handle con buffers y colas.
 * @param  x            Coordenada X (0..OLED_WIDTH-1).
 * @param  y            Coordenada Y (0..OLED_HEIGHT-1).
 * @param  use_overlay  true = usar frame_buffer_overlay; false = frame_buffer_main.
 *
 * Esta versión:
 *  - Quita el uso de flags dirty y encola directamente aquí.
 *  - Si la página ya estaba en cola (transfer_state == PAGE_REQ_WAITING),
 *    actualiza column_start y length según el nuevo rango.
 *  - Si no, añade la página al final de la cola.
 */
void _setPixel(OLED_HandleTypeDef *oled,
                      uint8_t x, uint8_t y,
                      bool use_overlay);

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

void OLED_DequeuePage(OLED_HandleTypeDef *oled);

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
