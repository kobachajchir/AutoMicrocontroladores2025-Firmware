/*
 * i2c_manager.c
 *
 *  Created on: Jun 7, 2025
 *      Author: kobac
 */
#include "i2c_manager.h"

static I2C_HandleTypeDef *i2c_handle = NULL;
static I2C_DeviceEntry device_table[I2C_MANAGER_MAX_DEVICES];
static I2C_BusState bus_state = I2C_STATE_IDLE;
static int8_t last_active_index = -1;
static volatile uint8_t *external_tx_busy = NULL;

void I2C_Manager_Init(I2C_HandleTypeDef *hi2c, volatile uint8_t *tx_busy_flag) {
    i2c_handle = hi2c;
    external_tx_busy = tx_busy_flag;
    bus_state = I2C_STATE_IDLE;

    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        device_table[i].type.id = I2C_MANAGER_INVALID_SLOT;
        device_table[i].type.enabled = 0;
        device_table[i].request_pending = 0;
        device_table[i].transfer_complete_cb = NULL;
        device_table[i].request_approved_cb = NULL;
    }
}

HAL_StatusTypeDef I2C_Manager_RegisterDevice(I2C_DeviceID id, uint8_t address,
                                             I2C_Callback transfer_complete_cb,
                                             I2C_Callback request_approved_cb,
                                             uint8_t priority) {
    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (!device_table[i].type.enabled) {
            device_table[i].type.id = id;
            device_table[i].type.i2c_address = address;
            device_table[i].type.priority = priority;
            device_table[i].type.enabled = 1;
            device_table[i].transfer_complete_cb = transfer_complete_cb;
            device_table[i].request_approved_cb = request_approved_cb;
            return HAL_OK;
        }
    }
    return HAL_ERROR;
}

HAL_StatusTypeDef I2C_Manager_SetRequestPending(I2C_DeviceID id, uint8_t pending) {
    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (device_table[i].type.id == id) {
            device_table[i].request_pending = pending;
            return HAL_OK;
        }
    }
    return HAL_ERROR;
}

HAL_StatusTypeDef I2C_Manager_RequestAccess(I2C_DeviceID id) {
    if (bus_state != I2C_STATE_IDLE || *external_tx_busy) {
        return HAL_BUSY;
    }

    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (device_table[i].type.id == id && device_table[i].type.enabled) {
            bus_state = I2C_STATE_BUSY;
            *external_tx_busy = 1;
            last_active_index = i;

            if (device_table[i].request_approved_cb) {
                device_table[i].request_approved_cb();
            }

            return HAL_OK;
        }
    }
    return HAL_ERROR;
}

void I2C_Manager_OnDMAComplete(void) {
    // 1) Libera el bus de I2C antes de invocar el callback
    I2C_Manager_ReleaseBus();

    // 2) Llama al callback de transferencia completa
    int idx = last_active_index;
    void (*cb)(void) = device_table[idx].transfer_complete_cb;
    __NOP();  // breakpoint opcional

    if (idx >= 0 && cb) {
        cb();  // aquí es donde el wrapper OLED_DMA_Complete_I2CManager vuelve a pedir el bus
    }
}


uint8_t I2C_Manager_GetAddress(I2C_DeviceID id) {
    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (device_table[i].type.id == id) {
            return device_table[i].type.i2c_address;
        }
    }
    return 0xFF;
}

void I2C_Manager_ReleaseBus(void) {
	 __NOP();
    bus_state = I2C_STATE_IDLE;
    *external_tx_busy = 0;
}

void I2C_Manager_ScanBus(void) {
    if (!i2c_handle) return;

    for (uint8_t addr = 1; addr < 127; addr++) {
        if (HAL_I2C_IsDeviceReady(i2c_handle, addr << 1, 2, 10) == HAL_OK) {
            // Encontró un dispositivo, colocamos un NOP para debug
            __NOP();  // → poné un breakpoint acá para ver qué dirección se detectó (en `addr`)
        }
    }
}

void I2C_Manager_Update(void) {
    if (bus_state != I2C_STATE_IDLE || !external_tx_busy || *external_tx_busy){
    	__NOP();
        return;
    }

    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (i != last_active_index &&
            device_table[i].type.enabled &&
            device_table[i].request_pending) {

            device_table[i].request_pending = 0;
            bus_state = I2C_STATE_BUSY;
            *external_tx_busy = 1;
            last_active_index = i;

			__NOP();
            if (device_table[i].request_approved_cb) {
                device_table[i].request_approved_cb();
            }

            break;
        }
    }
}

HAL_StatusTypeDef I2C_Manager_IsAddressReady(uint8_t i2c_address) {
    if (!i2c_handle) return HAL_ERROR;
    return HAL_I2C_IsDeviceReady(i2c_handle, (i2c_address << 1), 2, 10);
}




