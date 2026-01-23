/*
 * i2c_manager.h
 *
 *  Created on: Jun 7, 2025
 *  Updated on: Jan 12, 2026
 *      Author: kobac (updated w/ arbitration lease)
 *
 *  I2C Manager = bus arbiter + callback router
 *  - Devices register with callbacks.
 *  - A device requests the bus; if granted, manager calls on_granted(ctx).
 *  - Device keeps ownership until it calls ReleaseBus().
 *  - When DMA TX/RX completes, HAL callbacks must forward to I2C_Manager_OnTxCplt/OnRxCplt,
 *    which route the event to the current owner device.
 */

#ifndef INC_I2C_MANAGER_H_
#define INC_I2C_MANAGER_H_

#include "stm32f1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_MANAGER_MAX_DEVICES        2
#define I2C_MANAGER_DEVICE_NONE        0xFFu

// ---------- Device IDs (internal logical IDs) ----------
typedef uint8_t I2C_DeviceID;
#define DEVICE_ID_OLED   ((I2C_DeviceID)0x01u)
#define DEVICE_ID_MPU    ((I2C_DeviceID)0x02u)

// ---------- Errors ----------
typedef enum {
    I2C_MAN_ERR_NONE = 0,
    I2C_MAN_ERR_NOT_REGISTERED,
    I2C_MAN_ERR_NOT_OWNER,
    I2C_MAN_ERR_TIMEOUT,
    I2C_MAN_ERR_INTERNAL
} I2C_ManagerError;

// ---------- Callbacks ----------
typedef void (*I2C_OnGrantedCb)(void *ctx);
typedef void (*I2C_OnTxDoneCb)(void *ctx);
typedef void (*I2C_OnRxDoneCb)(void *ctx);
typedef void (*I2C_OnErrorCb)(void *ctx, I2C_ManagerError err);

// ---------- Request priority ----------
typedef enum {
    I2C_REQ_PRIO_LOW = 0,
    I2C_REQ_PRIO_REGULAR = 1,
    I2C_REQ_PRIO_HIGH = 2
} I2C_RequestPrio;

// ---------- Device entry ----------
typedef struct {
    I2C_DeviceID id;           // internal ID
    uint16_t     addr_7bit;    // 7-bit address (0x3C, 0x68, etc.)
    uint8_t      priority;     // 0..15 (optional use)
    uint8_t      enabled;      // 0/1
    uint8_t      pending;      // 0/1 (wants bus)
    I2C_RequestPrio pending_prio; // accumulated pending prio (max)

    void        *ctx;          // user context (OLED handle*, MPU handle*, etc.)
    I2C_OnGrantedCb on_granted;
    I2C_OnTxDoneCb  on_tx_done;
    I2C_OnRxDoneCb  on_rx_done;
    I2C_OnErrorCb   on_error;
} I2C_DeviceEntry;

// ---------- Manager handle ----------
typedef struct {
    I2C_HandleTypeDef *hi2c;

    I2C_DeviceEntry devices[I2C_MANAGER_MAX_DEVICES];

    I2C_DeviceID owner_id;       // current bus owner (or NONE)
    I2C_DeviceID last_owner_id;  // last owner for fairness
    uint8_t      owner_index;    // cached index of owner in table (or 0xFF)

    uint32_t lease_start_tick;   // for timeout watchdog
    uint32_t lease_timeout_ms;   // 0 disables timeout
    void (*recover_cb)(I2C_HandleTypeDef *hi2c); // optional recovery hook

    uint32_t spurious_tx;
    uint32_t spurious_rx;
    uint32_t spurious_err;

} I2C_ManagerHandle;

// ---------- API ----------
HAL_StatusTypeDef I2C_Manager_Init(I2C_ManagerHandle *hmgr,
                                  I2C_HandleTypeDef *hi2c,
                                  uint32_t lease_timeout_ms);

HAL_StatusTypeDef I2C_Manager_RegisterDevice(I2C_ManagerHandle *hmgr,
                                            I2C_DeviceID id,
                                            uint16_t addr_7bit,
                                            uint8_t priority,
                                            void *ctx,
                                            I2C_OnGrantedCb on_granted,
                                            I2C_OnTxDoneCb on_tx_done,
                                            I2C_OnRxDoneCb on_rx_done,
                                            I2C_OnErrorCb on_error);

HAL_StatusTypeDef I2C_Manager_RequestBusPrio(I2C_ManagerHandle *hmgr,
                                             I2C_DeviceID id,
                                             I2C_RequestPrio prio);
HAL_StatusTypeDef I2C_Manager_RequestBus(I2C_ManagerHandle *hmgr, I2C_DeviceID id);
HAL_StatusTypeDef I2C_Manager_ReleaseBus(I2C_ManagerHandle *hmgr, I2C_DeviceID id);

I2C_DeviceID I2C_Manager_GetOwner(I2C_ManagerHandle *hmgr);
uint16_t I2C_Manager_GetAddress7(I2C_ManagerHandle *hmgr, I2C_DeviceID id);

HAL_StatusTypeDef I2C_Manager_IsAddressReady(I2C_ManagerHandle *hmgr, uint16_t addr_7bit);

// Route HAL callbacks to current owner:
void I2C_Manager_OnTxCplt(I2C_ManagerHandle *hmgr, I2C_HandleTypeDef *hi2c);
void I2C_Manager_OnRxCplt(I2C_ManagerHandle *hmgr, I2C_HandleTypeDef *hi2c);
void I2C_Manager_OnError(I2C_ManagerHandle *hmgr, I2C_HandleTypeDef *hi2c);

// Optional watchdog tick (call from 10ms task, etc.)
void I2C_Manager_Tick(I2C_ManagerHandle *hmgr);

#ifdef __cplusplus
}
#endif

#endif /* INC_I2C_MANAGER_H_ */
