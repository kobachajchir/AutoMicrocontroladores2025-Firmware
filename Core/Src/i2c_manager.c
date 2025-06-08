/*
 * i2c_manager.c
 *
 *  Created on: Jun 7, 2025
 *      Author: kobac
 */

#include "i2c_manager.h"

static I2C_DeviceEntry device_table[I2C_MANAGER_MAX_DEVICES];
static I2C_BusState bus_state = I2C_STATE_IDLE;
static I2C_HandleTypeDef *i2c_handle = NULL;
static int8_t current_device_index = -1;
static i2cManager_DebugPrint_t debug_print; ///< Si es NULL, no imprime nada

void I2C_Manager_Init(I2C_HandleTypeDef *hi2c) {
    i2c_handle = hi2c;
    bus_state = I2C_STATE_IDLE;
    current_device_index = -1;

    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        device_table[i].active = 0;
        device_table[i].callback = NULL;
        device_table[i].id = I2C_MANAGER_INVALID_SLOT;
        device_table[i].priority = 0;
        device_table[i].retry_count = 0;
    }
}

void Init_I2C_Manager(I2C_HandleTypeDef *hi2c, i2cManager_DebugPrint_t debug_print_fn) {
    I2C_Manager_Init(hi2c);
    debug_print = debug_print_fn;
    I2C_Manager_ScanBus();
}


HAL_StatusTypeDef I2C_Manager_RegisterDevice(I2C_DeviceID id, uint8_t address, I2C_Callback callback) {
    return I2C_Manager_RegisterDeviceWithPriority(id, address, callback, 1);
}

HAL_StatusTypeDef I2C_Manager_RegisterDeviceWithPriority(I2C_DeviceID id, uint8_t address, I2C_Callback callback, uint8_t priority) {
    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (!device_table[i].active) {
            device_table[i].id = id;
            device_table[i].i2c_address = address;
            device_table[i].callback = callback;
            device_table[i].priority = priority;
            device_table[i].retry_count = 0;
            device_table[i].active = 1;
            return HAL_OK;
        }
    }
    return HAL_ERROR;
}

I2C_RequestStatus I2C_Manager_RequestAccess(I2C_DeviceID id) {
    if (bus_state == I2C_STATE_IDLE) {
        int best_index = -1;
        uint8_t best_priority = 0;
        for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
            if (device_table[i].active && device_table[i].id == id) {
                if (device_table[i].retry_count >= FAIRNESS_THRESHOLD) {
                    best_index = i;
                    break;
                } else if (device_table[i].priority >= best_priority) {
                    best_index = i;
                    best_priority = device_table[i].priority;
                }
            }
        }
        if (best_index >= 0) {
            current_device_index = best_index;
            bus_state = I2C_STATE_BUSY;
            device_table[best_index].retry_count = 0;
            return I2C_REQ_GRANTED;
        }
    } else {
        for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
            if (device_table[i].active && device_table[i].id == id) {
                device_table[i].retry_count++;
                break;
            }
        }
    }
    return I2C_REQ_PENDING;
}

void I2C_Manager_ReleaseBus(void) {
    bus_state = I2C_STATE_IDLE;
    current_device_index = -1;
}

void I2C_Manager_OnDMAComplete(void) {
	if (current_device_index >= 0 && current_device_index < I2C_MANAGER_MAX_DEVICES) {
	    I2C_Callback cb = device_table[current_device_index].callback;
	    if (cb != NULL) cb();
	}

    I2C_Manager_ReleaseBus();
}

uint8_t I2C_Manager_GetAddress(I2C_DeviceID id) {
    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (device_table[i].active && device_table[i].id == id) {
            return device_table[i].i2c_address;
        }
    }
    return 0xFF; // Dirección inválida
}

uint8_t I2C_Manager_FindFreeSlot(void) {
    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (!device_table[i].active) {
            return i;
        }
    }
    return I2C_MANAGER_INVALID_SLOT;
}

I2C_BusState I2C_Manager_GetState(void) {
    return bus_state;
}

void I2C_Manager_ScanBus(void) {
    if (i2c_handle == NULL) return;
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (HAL_I2C_IsDeviceReady(i2c_handle, addr << 1, 2, 10) == HAL_OK) {
        	__NOP();
        }
    }
}

