/*
 * i2c_manager.h
 *
 *  Created on: Jun 7, 2025
 *      Author: kobac
 */

#ifndef INC_I2C_MANAGER_H_
#define INC_I2C_MANAGER_H_

#include "stm32f1xx_hal.h"

#define I2C_MANAGER_MAX_DEVICES 4
#define I2C_MANAGER_INVALID_SLOT 0xFF

typedef void (*I2C_Callback)(void);
typedef void (*I2C_RequestApprovedCallback)(void);

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
    uint8_t priority;
    uint8_t enabled;
} i2cDeviceType;

// Estructura de la tabla de dispositivos
typedef struct {
    i2cDeviceType type;
    I2C_Callback transfer_complete_cb;
    I2C_Callback request_approved_cb;
    uint8_t request_pending;
} I2C_DeviceEntry;

static I2C_HandleTypeDef *i2c_handle;
static I2C_DeviceEntry device_table[I2C_MANAGER_MAX_DEVICES];
static I2C_BusState bus_state;
static int8_t last_active_index;
static volatile uint8_t *external_tx_busy;

// Funciones públicas
void I2C_Manager_Init(I2C_HandleTypeDef *hi2c, volatile uint8_t *tx_busy_flag);
HAL_StatusTypeDef I2C_Manager_RegisterDevice(
    I2C_DeviceID id,
    uint8_t address,
    I2C_Callback dma_complete_cb,
    I2C_RequestApprovedCallback request_granted_cb,
    uint8_t priority
);
HAL_StatusTypeDef I2C_Manager_SetRequestPending(I2C_DeviceID id, uint8_t pending);
HAL_StatusTypeDef I2C_Manager_RequestAccess(I2C_DeviceID id);
void I2C_Manager_OnDMAComplete(void);
HAL_StatusTypeDef I2C_Manager_IsAddressReady(uint8_t i2c_address);
uint8_t I2C_Manager_GetAddress(I2C_DeviceID id);
void I2C_Manager_ScanBus(void);
void I2C_Manager_Update(void);

static inline void I2C_Manager_ReleaseBus(void) {
    bus_state = I2C_STATE_IDLE;
    *external_tx_busy = 0;
}


#endif /* INC_I2C_MANAGER_H_ */
