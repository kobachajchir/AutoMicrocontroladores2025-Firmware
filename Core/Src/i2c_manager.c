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
static volatile uint8_t *external_busy = NULL;

void I2C_Manager_Init(I2C_HandleTypeDef *hi2c, volatile uint8_t *busy_flag) {
    i2c_handle = hi2c;
    external_busy = busy_flag;
    bus_state = I2C_STATE_IDLE;

    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        device_table[i].type.id = I2C_MANAGER_NOINIT_DEVICEID;
        CLEAR_FLAG(device_table[i].type.flags, I2C_DEV_ENABLED);
        CLEAR_FLAG(device_table[i].flags, I2C_REQ_PENDING);
        device_table[i].transfer_complete_cb = NULL;
        device_table[i].request_approved_cb = NULL;
    }
}

HAL_StatusTypeDef I2C_Manager_RegisterDevice(I2C_DeviceID id, uint8_t address,
											I2C_TransferCompleteCb transfer_complete_cb,
											I2C_RequestApprovedCb request_approved_cb,
                                             uint8_t priority) {
    for (int i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (!IS_FLAG_SET(device_table[i].type.flags, I2C_DEV_ENABLED)) {
            device_table[i].type.id = id;
            device_table[i].type.i2c_address = address;
            NIBBLEH_SET_STATE(device_table[i].type.flags, priority);
            SET_FLAG(device_table[i].type.flags, I2C_DEV_ENABLED);
            device_table[i].transfer_complete_cb = transfer_complete_cb;
            device_table[i].request_approved_cb = request_approved_cb;
            return HAL_OK;
        }
    }
    return HAL_ERROR;
}

HAL_StatusTypeDef I2C_Manager_RequestAccess(I2C_DeviceID id, uint8_t req_type) {
    uint8_t idx = 0xFF;

    // 1) Buscar índice del dispositivo
    for (uint8_t i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (IS_FLAG_SET(device_table[i].type.flags, I2C_DEV_ENABLED)
         && device_table[i].type.id == id) {
            idx = i;
            break;
        }
    }
    if (idx == 0xFF) {
        return HAL_ERROR;  // No está registrado
    }

    // 2) Marcar petición como pendiente y tipo
    SET_FLAG(device_table[idx].flags, I2C_REQ_PENDING);
    switch (req_type) {
        case I2C_REQ_TYPE_TX:
            SET_FLAG(device_table[idx].flags, I2C_REQ_IS_TX);
            CLEAR_FLAG(device_table[idx].flags, I2C_REQ_IS_RX);
            break;
        case I2C_REQ_TYPE_RX:
            CLEAR_FLAG(device_table[idx].flags, I2C_REQ_IS_TX);
            SET_FLAG(device_table[idx].flags, I2C_REQ_IS_RX);
            break;
        case I2C_REQ_TYPE_TX_RX:
            SET_FLAG(device_table[idx].flags, I2C_REQ_IS_TX);
            SET_FLAG(device_table[idx].flags, I2C_REQ_IS_RX);
            break;
    }

    // 3) Si el bus está ocupado, devolvemos BUSY
    if (*external_busy) {
        return HAL_BUSY;
    }

    // 4) Buscar la primera petición pendiente distinta del último activo
    for (uint8_t offset = 1; offset <= I2C_MANAGER_MAX_DEVICES; offset++) {
        uint8_t i = (last_active_index + offset) % I2C_MANAGER_MAX_DEVICES;

        if (IS_FLAG_SET(device_table[i].type.flags, I2C_DEV_ENABLED) &&
            IS_FLAG_SET(device_table[i].flags, I2C_REQ_PENDING))
        {
            // 5) Conceder acceso al nuevo dispositivo
            CLEAR_FLAG(device_table[i].flags, I2C_REQ_PENDING);
            *external_busy = 1;
            last_active_index = i;

            if (device_table[i].request_approved_cb) {
                device_table[i].request_approved_cb();
            }
            return HAL_OK;
        }
    }

    // 6) Si nadie más tiene pendiente, volver a conceder al mismo (si tenía pendiente)
    if (IS_FLAG_SET(device_table[idx].flags, I2C_REQ_PENDING)) {
        CLEAR_FLAG(device_table[idx].flags, I2C_REQ_PENDING);
        *external_busy = 1;
        last_active_index = idx;

        if (device_table[idx].request_approved_cb) {
            device_table[idx].request_approved_cb();
        }
        return HAL_OK;
    }

    return HAL_BUSY;
}

void I2C_Manager_OnDMAComplete(uint8_t is_tx) {
    int idx = last_active_index;
    void (*cb)(uint8_t is_tx) = device_table[idx].transfer_complete_cb;
    __NOP();  // breakpoint opcional

    if (idx >= 0 && cb) {
        cb(is_tx);  // 1 EN TX
    }
}

uint8_t I2C_Manager_ReleaseBus(I2C_DeviceID id) {
    __NOP();

    if (last_active_index < 0) return 0;

    if (device_table[last_active_index].type.id == id) {
        bus_state = I2C_STATE_IDLE;
        *external_busy = 0;
        last_active_index = -1;
        CLEAR_FLAG(device_table[last_active_index].flags, I2C_REQ_IS_TX);
        CLEAR_FLAG(device_table[last_active_index].flags, I2C_REQ_IS_RX);
        return 1;  // Se liberó exitosamente
    }

    return 0;  // No correspondía liberar
}

uint8_t I2C_Manager_GetAddress(I2C_DeviceID id) {
    for (uint8_t i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (device_table[i].type.id == id) {
            return device_table[i].type.i2c_address;
        }
    }
    return 0xFF;
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
    if (bus_state != I2C_STATE_IDLE || *external_busy){
    	__NOP();
        return;
    }

    for (uint8_t i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (i != last_active_index &&
        	IS_FLAG_SET(device_table[i].type.flags, I2C_DEV_ENABLED) &&
			IS_FLAG_SET(device_table[i].flags, I2C_REQ_PENDING)) {

            CLEAR_FLAG(device_table[i].flags, I2C_REQ_PENDING);
            bus_state = I2C_STATE_BUSY;
            *external_busy = 1;
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




