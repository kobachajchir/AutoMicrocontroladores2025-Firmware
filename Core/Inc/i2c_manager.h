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

#define MEMADD_SIZE_8BIT I2C_MEMADD_SIZE_8BIT

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
    I2C_Callback transfer_complete_cb_RX;
	I2C_Callback request_approved_cb_RX;
    uint8_t request_pending;
} I2C_DeviceEntry;

// Funciones públicas
void I2C_Manager_Init(I2C_HandleTypeDef *hi2c, volatile uint8_t *tx_busy_flag, volatile uint8_t *rx_busy_flag);
HAL_StatusTypeDef I2C_Manager_RegisterDevice(I2C_DeviceID id, uint8_t address,
                                             I2C_Callback transfer_complete_cb,
                                             I2C_Callback request_approved_cb,
											 I2C_Callback transfer_complete_cb_RX,
											 I2C_Callback request_approved_cb_RX,
                                             uint8_t priority);
HAL_StatusTypeDef I2C_Manager_SetRequestPending(I2C_DeviceID id, uint8_t pending);
HAL_StatusTypeDef I2C_Manager_RequestAccess(I2C_DeviceID id);
void I2C_Manager_OnDMAComplete(void);
void I2C_Manager_OnRXDMAComplete(void);
HAL_StatusTypeDef I2C_Manager_IsAddressReady(uint8_t i2c_address);
uint8_t I2C_Manager_GetAddress(I2C_DeviceID id);
uint8_t I2C_Manager_ReleaseBus(I2C_DeviceID id);
HAL_StatusTypeDef I2C_Manager_RequestAccessRX(I2C_DeviceID id);
uint8_t          I2C_Manager_ReleaseBusRX(I2C_DeviceID id);
void I2C_Manager_ScanBus(void);
void I2C_Manager_Update(void);


#endif /* INC_I2C_MANAGER_H_ */
