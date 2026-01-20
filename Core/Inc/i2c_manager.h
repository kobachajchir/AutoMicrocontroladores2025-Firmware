/*
 * i2c_manager.h
 *
 *  Created on: Jun 7, 2025
 *      Author: kobac
 */

#ifndef INC_I2C_MANAGER_H_
#define INC_I2C_MANAGER_H_

#include "stm32f1xx_hal.h"

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
} I2C_Man_Byte_Flag_Struct_t;

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

#define I2C_MANAGER_MAX_DEVICES 2
#define I2C_MANAGER_NOINIT_DEVICEID 0xFF

//Defines para el i2cDeviceType
#define I2C_DEV_ENABLED BIT0_MASK
/*#define UNUSED  BIT1_MASK
#define UNUSED  BIT2_MASK
#define UNUSED  BIT3_MASK*/
//Otra parte (Alta) guarda la prioridad

//Defines para el I2C_DeviceEntry
#define I2C_REQ_PENDING BIT0_MASK
#define I2C_REQ_IS_TX  BIT1_MASK
#define I2C_REQ_IS_RX  BIT2_MASK
/*#define UNUSED  BIT3_MASK*/

#define MEMADD_SIZE_8BIT I2C_MEMADD_SIZE_8BIT

typedef void (*I2C_RequestApprovedCb)(void);
typedef void (*I2C_TransferCompleteCb)(uint8_t is_tx);

typedef uint8_t I2C_ReqType;
#define I2C_REQ_TYPE_TX   0
#define I2C_REQ_TYPE_RX   1
#define I2C_REQ_TYPE_TX_RX 2

typedef enum {
    I2C_STATE_IDLE = 0,
    I2C_STATE_BUSY
} I2C_BusState;

typedef enum {
    DEVICE_ID_OLED = 0x01,
    DEVICE_ID_MPU  = 0x02,
} I2C_DeviceID;

// Estructura del tipo de dispositivo
typedef struct {
    I2C_DeviceID id;
    uint8_t i2c_address;
    I2C_Man_Byte_Flag_Struct_t flags;
} i2cDeviceType;

// Estructura de la tabla de dispositivos
typedef struct {
    i2cDeviceType type;
    I2C_RequestApprovedCb request_approved_cb;
    I2C_TransferCompleteCb transfer_complete_cb;
    I2C_Man_Byte_Flag_Struct_t flags; //Mascaras en Nibble L, I2C_ReqType en Nibble H
} I2C_DeviceEntry;

// Funciones públicas
void I2C_Manager_Init(I2C_HandleTypeDef *hi2c, volatile uint8_t *busy_flag);
HAL_StatusTypeDef I2C_Manager_RegisterDevice(I2C_DeviceID id, uint8_t address,
											I2C_TransferCompleteCb transfer_complete_cb,
											 I2C_RequestApprovedCb request_approved_cb,
                                             uint8_t priority);
HAL_StatusTypeDef I2C_Manager_RequestAccess(I2C_DeviceID id, uint8_t req_is_tx);
void I2C_Manager_OnDMAComplete(uint8_t is_tx);
HAL_StatusTypeDef I2C_Manager_IsAddressReady(uint8_t i2c_address);
uint8_t I2C_Manager_GetAddress(I2C_DeviceID id);
uint8_t I2C_Manager_ReleaseBus(I2C_DeviceID id);
void I2C_Manager_ScanBus(void);
void I2C_Manager_Update(void);


#endif /* INC_I2C_MANAGER_H_ */
