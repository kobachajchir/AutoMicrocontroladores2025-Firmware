/*
 * motor_control.h
 *
 *  Created on: Jun 7, 2025
 *      Author: kobac
 */

#ifndef INC_TYPES_MOTOR_CONTROL_H_
#define INC_TYPES_MOTOR_CONTROL_H_

#include "stm32f1xx_hal.h"
#include "stdbool.h"

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
} Motor_Byte_Flag_Struct_t;

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
#define ENABLE_MOVEMENT     BIT0_MASK  // bit 0 → almacena el estado previo (0 o 1)
/*#define UNUSED   BIT1_MASK  // bit 1 → pulsación corta detectada
#define UNUSED    BIT2_MASK  // bit 2 → pulsación larga detectada
#define UNUSED    BIT3_MASK  // Lo usaremos mas adelante para algo*/

#define MOTORLEFT_SELECTED 0
#define MOTORRIGHT_SELECTED 1
#define MOTORNONE_SELECTED 2

// Dirección de rotación
typedef enum {
	MOTOR_DIR_FORWARD   = 0,
	MOTOR_DIR_BACKWARD  = 1
} Motor_Direction_t;

typedef struct {
	uint8_t motorSpeed;
	Motor_Direction_t motorDirection;
} Motor_Config_t;

typedef struct {
	Motor_Byte_Flag_Struct_t flags;   ///< nibble bajo = flags; nibble alto = motor seleccionado
	Motor_Config_t motorLeft;
	Motor_Config_t motorRight;
} Motor_Data_t;

typedef struct MotorControl_Handle MotorControl_Handle;

// Tipo de función para calcular prescaler y periodo
typedef void (*TimerComputeParamsFunc)(uint32_t target_freq_hz,
                                        uint32_t timer_clk_hz,
                                        uint16_t *prescaler_out,
                                        uint16_t *period_out);

	struct MotorControl_Handle {
		TIM_HandleTypeDef *htim;
		uint32_t desired_pwm_freq;
		TimerComputeParamsFunc compute_params;

		uint32_t left_forward_channel;
		uint32_t left_backward_channel;
		uint32_t right_forward_channel;
		uint32_t right_backward_channel;

		Motor_Data_t motorData;

	};

	HAL_StatusTypeDef Motor_Init(MotorControl_Handle *self);
	void Motor_SetLeftForward(MotorControl_Handle *self, uint8_t speed);
	void Motor_SetLeftBackward(MotorControl_Handle *self, uint8_t speed);
	void Motor_SetRightForward(MotorControl_Handle *self, uint8_t speed);
	void Motor_SetRightBackward(MotorControl_Handle *self, uint8_t speed);
	void Motor_StopLeft(MotorControl_Handle *self);
	void Motor_StopRight(MotorControl_Handle *self);
	void Motor_StopBoth(MotorControl_Handle *self);
	void Motor_SetBoth(MotorControl_Handle *self, uint8_t speed, Motor_Direction_t dir);

#ifdef __cplusplus
}
#endif


#endif /* INC_TYPES_MOTOR_CONTROL_H_ */
