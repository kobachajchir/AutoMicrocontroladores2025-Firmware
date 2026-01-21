/*
 * encoder.h
 *
 *  Created on: Jun 13, 2025
 *      Author: kobac
 */

#ifndef INC_ENCODER_H_
#define INC_ENCODER_H_

#include "stm32f1xx_hal.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

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
} ENC_Byte_Flag_Struct_t;

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

// ---------------- Máscaras para banderas de usuario (nibble bajo) ----------------
// Estas máscaras usan bits 0..3 de Byte_Flag_Struct.byte:
#define ENC_BTN_PREVSTATE     BIT0_MASK  // bit 0 → almacena el estado previo (0 o 1)
#define ENC_BTN_SHORT_PRESS   BIT1_MASK  // bit 1 → pulsación corta detectada
#define ENC_BTN_LONG_PRESS    BIT2_MASK  // bit 2 → pulsación larga detectada
#define ENC_FLAG_UPDATED    BIT3_MASK  // Lo usaremos mas adelante para algo

// ---------------- Valores máximos para overflow count (nibble alto) ----------------
#define ENC_BTN_OVF_MAX       9U   // si nibbleH > 9, descartamos (equivale a 10 s presionado)

typedef struct {
	ENC_Byte_Flag_Struct_t flags;   ///< nibble bajo = flags; nibble alto = overflow counter
    uint8_t          counter; ///< cuenta en pasos de 10 ms (va 0..100)
    GPIO_TypeDef     *port;
    uint16_t 		pin;
} ENC_ButtonState_t;

// Dirección de rotación
typedef enum {
	ENC_DIR_NONE = 0,
	ENC_DIR_CW   = 1,
	ENC_DIR_CCW  = -1
} ENC_Direction_t;

// Datos de encoder: velocidad y aceleración empaquetados
typedef union {
	struct {
		uint16_t vel; // Pulsos por intervalo (10 ms unitario)
		uint16_t acc; // Delta de pulsos respecto intervalo previo
	} pair;
	uint32_t allData; // acc<<16 | vel
} ENC_Data;

// Handle del encoder
typedef struct {
	TIM_HandleTypeDef *htim;       // Timer en Encoder Interface Mode (CH1/CH2)
	ENC_ButtonState_t  btnState;   // Estado interno del botón usuario
	uint8_t           sampleTenMs;// Período de muestreo en unidades de 10 ms
	ENC_Direction_t    dir;        // Dirección actual
	ENC_Data           data;       // Datos actuales (vel/acc)
	ENC_Data           prevData;   // Datos previos para cálculo
	ENC_Byte_Flag_Struct_t            flags;        // ← usamos bit0 = ENC_FLAG_UPDATED
	uint16_t prevCount;
	int16_t prevStepDelta;
	int16_t lastStepDelta;
	int16_t lastStepAcc;
	uint8_t           calibrateCountPerStep; ///< ¿Cuántos pulsos raw = 1 “paso”?
	int8_t            accumCount;            ///< Acumula pulsos raw entre pasos
	bool 			allowEncoderInput;
} ENC_Handle_t;

/**
 * @brief Inicializa el encoder
 * @param h           Puntero a ENC_Handle_t
 * @param htim        Timer configurado en Encoder Mode
 * @param sampleTenMs Período de muestreo en unidades de 10 ms
 */
void ENC_Init(ENC_Handle_t *h, TIM_HandleTypeDef *htim, uint32_t sampleTenMs, uint8_t calibrateCountPerStep, GPIO_TypeDef *port, uint16_t pin);
/**
 * @brief Arranca el encoder y resetea contadores
 */
void ENC_Start(ENC_Handle_t *h);

/**
 * @brief Se invoca cada 10 ms
 *        Lee PIN y actualiza flags, counter y overflow count (nibble alto).
 * @param btn Puntero a la estructura ButtonState_t
 */
void ENC_Button_Task_10ms(ENC_Handle_t *h);

/**
 * @brief Tarea de muestreo, llamar cada sampleTenMs*10 ms
 *        Realiza debounce de botón e actualiza vel/acc/dir
 */
void ENC_Task_N10ms(ENC_Handle_t *h);

/**
 * @brief Obtiene la dirección de giro
 */
ENC_Direction_t ENC_GetDirection(ENC_Handle_t *h);

/**
 * @brief Obtiene la velocidad (pulsos / intervalo)
 */
uint16_t ENC_GetVelocity(ENC_Handle_t *h);

/**
 * @brief Obtiene la aceleración (delta pulsos / intervalo)
 */
uint16_t ENC_GetAcceleration(ENC_Handle_t *h);

int16_t ENC_GetStepDelta(ENC_Handle_t *h);
int16_t ENC_GetSignedVelocity(ENC_Handle_t *h);
int16_t ENC_GetSignedAcceleration(ENC_Handle_t *h);
int16_t ENC_GetScaledSteps(ENC_Handle_t *h);

#ifdef __cplusplus
}
#endif

#endif /* INC_ENCODER_H_ */
