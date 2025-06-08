/*
 * i2c_manager.h
 *
 *  Created on: Jun 7, 2025
 *      Author: kobac
 */

#ifndef INC_I2C_MANAGER_H_
#define INC_I2C_MANAGER_H_

#include "stm32f1xx_hal.h" // O el HAL correspondiente

// Número máximo de periféricos registrados
#define I2C_MANAGER_MAX_DEVICES 2
#define I2C_MANAGER_INVALID_SLOT 0xFF
#define FAIRNESS_THRESHOLD 5

typedef enum {
    I2C_STATE_IDLE = 0,
    I2C_STATE_BUSY,
    I2C_STATE_WAITING
} I2C_BusState;

typedef enum {
    I2C_REQ_PENDING = 0,
    I2C_REQ_GRANTED,
    I2C_REQ_DENIED
} I2C_RequestStatus;

typedef uint8_t I2C_DeviceID;

// Callback de transferencia terminada
typedef void (*I2C_Callback)(void);

typedef struct {
    I2C_DeviceID id;
    uint8_t i2c_address;
    I2C_Callback callback;
    uint8_t active;
    uint8_t priority;       // 0 (baja) a 3 (alta)
    uint8_t retry_count;    // cuenta de intentos fallidos
} I2C_DeviceEntry;

// Funciones públicas
typedef void (*i2cManager_DebugPrint_t)(const char *msg);  // <-- agregar también este tipo

void Init_I2C_Manager(I2C_HandleTypeDef *hi2c, i2cManager_DebugPrint_t debug_print_fn);

// Registra un periférico
HAL_StatusTypeDef I2C_Manager_RegisterDeviceWithPriority(I2C_DeviceID id, uint8_t address, I2C_Callback callback, uint8_t priority);

// Solicita acceso al bus
I2C_RequestStatus I2C_Manager_RequestAccess(I2C_DeviceID id);

// Libera el bus (llamada desde callback de periférico o DMA handler)
void I2C_Manager_ReleaseBus(void);

// Llamado desde el DMA complete handler
void I2C_Manager_OnDMAComplete(void);

// Obtiene dirección I2C de un ID registrado
uint8_t I2C_Manager_GetAddress(I2C_DeviceID id);

// Verifica si hay slots disponibles
uint8_t I2C_Manager_FindFreeSlot(void);

// Consulta estado del bus
I2C_BusState I2C_Manager_GetState(void);

void I2C_Manager_ScanBus(void);


#endif /* INC_I2C_MANAGER_H_ */
