/*
 * i2c_manager.c
 *
 *  Created on: Jun 7, 2025
 *  Updated on: Jan 12, 2026
 *
 *  Arbitration model:
 *  - A device requests the bus.
 *  - If free, manager assigns owner and calls device.on_granted(ctx).
 *  - Owner keeps lease until ReleaseBus().
 *  - On release, manager grants next pending device (fairness).
 *  - HAL DMA complete callbacks must call I2C_Manager_OnTxCplt/OnRxCplt.
 */

#include "i2c_manager.h"

#define IDX_INVALID 0xFFu

static uint8_t find_index(I2C_ManagerHandle *hmgr, I2C_DeviceID id)
{
    if (!hmgr) return IDX_INVALID;
    for (uint8_t i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (hmgr->devices[i].enabled && hmgr->devices[i].id == id) {
            return i;
        }
    }
    return IDX_INVALID;
}

static void clear_device_entry(I2C_DeviceEntry *e)
{
    if (!e) return;
    e->id = (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE;
    e->addr_7bit = 0;
    e->priority = 0;
    e->enabled = 0;
    e->pending = 0;
    e->pending_prio = I2C_REQ_PRIO_LOW;
    e->ctx = NULL;
    e->on_granted = NULL;
    e->on_tx_done = NULL;
    e->on_rx_done = NULL;
    e->on_error = NULL;
}

static uint8_t pick_next_pending_index(I2C_ManagerHandle *hmgr)
{
    if (!hmgr) return IDX_INVALID;

    uint8_t best_idx = IDX_INVALID;
    I2C_RequestPrio best_prio = I2C_REQ_PRIO_LOW;
    uint8_t best_fair = 0;

    for (uint8_t i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (!hmgr->devices[i].enabled || !hmgr->devices[i].pending) {
            continue;
        }

        I2C_RequestPrio prio = hmgr->devices[i].pending_prio;
        uint8_t fair = (hmgr->devices[i].id != hmgr->last_owner_id);

        if (best_idx == IDX_INVALID ||
            prio > best_prio ||
            (prio == best_prio && fair > best_fair) ||
            (prio == best_prio && fair == best_fair && i < best_idx)) {
            best_idx = i;
            best_prio = prio;
            best_fair = fair;
        }
    }

    return best_idx;
}

static void grant_to_index(I2C_ManagerHandle *hmgr, uint8_t idx)
{
    if (!hmgr || idx == IDX_INVALID) return;

    if (HAL_I2C_GetState(hmgr->hi2c) != HAL_I2C_STATE_READY) {
        hmgr->devices[idx].pending = 1;
        return;
    }

    hmgr->devices[idx].pending = 0;
    hmgr->devices[idx].pending_prio = I2C_REQ_PRIO_LOW;

    hmgr->owner_id = hmgr->devices[idx].id;
    hmgr->owner_index = idx;
    hmgr->lease_start_tick = HAL_GetTick();

    __NOP(); // BREAKPOINT: bus I2C otorgado a un dispositivo
    if (hmgr->devices[idx].on_granted) {
        hmgr->devices[idx].on_granted(hmgr->devices[idx].ctx);
    }
}

HAL_StatusTypeDef I2C_Manager_Init(I2C_ManagerHandle *hmgr,
                                  I2C_HandleTypeDef *hi2c,
                                  uint32_t lease_timeout_ms)
{
    if (!hmgr || !hi2c) return HAL_ERROR;

    hmgr->hi2c = hi2c;
    hmgr->owner_id = (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE;
    hmgr->last_owner_id = (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE;
    hmgr->owner_index = IDX_INVALID;
    hmgr->lease_start_tick = 0;
    hmgr->lease_timeout_ms = lease_timeout_ms;
    hmgr->recover_cb = NULL;
    hmgr->spurious_tx = 0;
    hmgr->spurious_rx = 0;
    hmgr->spurious_err = 0;

    for (uint8_t i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        clear_device_entry(&hmgr->devices[i]);
    }

    return HAL_OK;
}

HAL_StatusTypeDef I2C_Manager_RegisterDevice(I2C_ManagerHandle *hmgr,
                                            I2C_DeviceID id,
                                            uint16_t addr_7bit,
                                            uint8_t priority,
                                            void *ctx,
                                            I2C_OnGrantedCb on_granted,
                                            I2C_OnTxDoneCb on_tx_done,
                                            I2C_OnRxDoneCb on_rx_done,
                                            I2C_OnErrorCb on_error)
{
    if (!hmgr) return HAL_ERROR;

    // prevent duplicates
    if (find_index(hmgr, id) != IDX_INVALID) {
        return HAL_ERROR;
    }

    for (uint8_t i = 0; i < I2C_MANAGER_MAX_DEVICES; i++) {
        if (!hmgr->devices[i].enabled) {
            hmgr->devices[i].id = id;
            hmgr->devices[i].addr_7bit = addr_7bit;
            hmgr->devices[i].priority = priority;
            hmgr->devices[i].enabled = 1;
            hmgr->devices[i].pending = 0;
            hmgr->devices[i].ctx = ctx;
            hmgr->devices[i].on_granted = on_granted;
            hmgr->devices[i].on_tx_done = on_tx_done;
            hmgr->devices[i].on_rx_done = on_rx_done;
            hmgr->devices[i].on_error = on_error;
            return HAL_OK;
        }
    }

    return HAL_ERROR;
}

HAL_StatusTypeDef I2C_Manager_RequestBus(I2C_ManagerHandle *hmgr, I2C_DeviceID id)
{
    return I2C_Manager_RequestBusPrio(hmgr, id, I2C_REQ_PRIO_REGULAR);
}

HAL_StatusTypeDef I2C_Manager_RequestBusPrio(I2C_ManagerHandle *hmgr,
                                             I2C_DeviceID id,
                                             I2C_RequestPrio prio)
{
    if (!hmgr) return HAL_ERROR;

    uint8_t idx = find_index(hmgr, id);
    if (idx == IDX_INVALID) return HAL_ERROR;

    // If already owner, nothing to do (device can continue its multi-transfer sequence)
    if (hmgr->owner_id == id) {
        return HAL_OK;
    }

    // Mark pending
    hmgr->devices[idx].pending = 1;
    if (hmgr->devices[idx].pending_prio < prio) {
        hmgr->devices[idx].pending_prio = prio;
    }

    // If bus free -> grant immediately
    if (hmgr->owner_id == (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE) {
        grant_to_index(hmgr, idx);
        return HAL_OK;
    }

    return HAL_BUSY;
}

HAL_StatusTypeDef I2C_Manager_ReleaseBus(I2C_ManagerHandle *hmgr, I2C_DeviceID id)
{
    if (!hmgr) return HAL_ERROR;

    if (hmgr->owner_id != id) {
        return HAL_ERROR; // not owner
    }

    // Release
    __NOP();
    hmgr->last_owner_id = hmgr->owner_id;
    hmgr->owner_id = (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE;
    hmgr->owner_index = IDX_INVALID;
    hmgr->lease_start_tick = 0;

    // Grant next pending (if any)
    uint8_t next = pick_next_pending_index(hmgr);
    if (next != IDX_INVALID) {
        grant_to_index(hmgr, next);
    }

    return HAL_OK;
}

I2C_DeviceID I2C_Manager_GetOwner(I2C_ManagerHandle *hmgr)
{
    if (!hmgr) return (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE;
    return hmgr->owner_id;
}

uint16_t I2C_Manager_GetAddress7(I2C_ManagerHandle *hmgr, I2C_DeviceID id)
{
    if (!hmgr) return 0xFFFFu;
    uint8_t idx = find_index(hmgr, id);
    if (idx == IDX_INVALID) return 0xFFFFu;
    return hmgr->devices[idx].addr_7bit;
}

HAL_StatusTypeDef I2C_Manager_IsAddressReady(I2C_ManagerHandle *hmgr, uint16_t addr_7bit)
{
    if (!hmgr || !hmgr->hi2c) return HAL_ERROR;
    return HAL_I2C_IsDeviceReady(hmgr->hi2c, (uint16_t)(addr_7bit << 1), 2, 10);
}

// ---------- Callback routing ----------
void I2C_Manager_OnTxCplt(I2C_ManagerHandle *hmgr, I2C_HandleTypeDef *hi2c)
{
    if (!hmgr || !hi2c) return;
    if (hmgr->hi2c->Instance != hi2c->Instance) return;

    if (hmgr->owner_index == IDX_INVALID ||
        hmgr->owner_id == (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE) {
        hmgr->spurious_tx++;
        return;
    }

    I2C_DeviceEntry *dev = &hmgr->devices[hmgr->owner_index];
    if (dev->enabled && dev->on_tx_done) {
        dev->on_tx_done(dev->ctx);
    }
}

void I2C_Manager_OnRxCplt(I2C_ManagerHandle *hmgr, I2C_HandleTypeDef *hi2c)
{
    if (!hmgr || !hi2c) return;
    if (hmgr->hi2c->Instance != hi2c->Instance) return;

    if (hmgr->owner_index == IDX_INVALID ||
        hmgr->owner_id == (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE) {
        hmgr->spurious_rx++;
        return;
    }

    I2C_DeviceEntry *dev = &hmgr->devices[hmgr->owner_index];
    if (dev->enabled && dev->on_rx_done) {
        dev->on_rx_done(dev->ctx);
    }
}

void I2C_Manager_OnError(I2C_ManagerHandle *hmgr, I2C_HandleTypeDef *hi2c)
{
    if (!hmgr || !hi2c) return;
    if (hmgr->hi2c->Instance != hi2c->Instance) return;

    if (hmgr->owner_index == IDX_INVALID ||
        hmgr->owner_id == (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE) {
        hmgr->spurious_err++;
        return;
    }

    I2C_DeviceEntry *dev = &hmgr->devices[hmgr->owner_index];
    if (dev->enabled && dev->on_error) {
        dev->on_error(dev->ctx, I2C_MAN_ERR_INTERNAL);
    }

    // Force release to avoid deadlock
    hmgr->last_owner_id = hmgr->owner_id;
    hmgr->owner_id = (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE;
    hmgr->owner_index = IDX_INVALID;
    hmgr->lease_start_tick = 0;

    if (hmgr->recover_cb) {
        hmgr->recover_cb(hi2c);
    }

    // Grant next pending
    uint8_t next = pick_next_pending_index(hmgr);
    if (next != IDX_INVALID) {
        grant_to_index(hmgr, next);
    }
}

// ---------- Watchdog tick ----------
void I2C_Manager_Tick(I2C_ManagerHandle *hmgr)
{
    if (!hmgr) return;
    if (hmgr->lease_timeout_ms == 0) return;

    if (hmgr->owner_id == (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE) return;

    uint32_t now = HAL_GetTick();
    if ((now - hmgr->lease_start_tick) > hmgr->lease_timeout_ms) {

        // Notify owner
        if (hmgr->owner_index != IDX_INVALID) {
            I2C_DeviceEntry *dev = &hmgr->devices[hmgr->owner_index];
            if (dev->enabled && dev->on_error) {
                dev->on_error(dev->ctx, I2C_MAN_ERR_TIMEOUT);
            }
        }

        // Force release
        hmgr->last_owner_id = hmgr->owner_id;
        hmgr->owner_id = (I2C_DeviceID)I2C_MANAGER_DEVICE_NONE;
        hmgr->owner_index = IDX_INVALID;
        hmgr->lease_start_tick = 0;

        if (hmgr->recover_cb) {
            hmgr->recover_cb(hmgr->hi2c);
        }

        // Grant next pending
        uint8_t next = pick_next_pending_index(hmgr);
        if (next != IDX_INVALID) {
            grant_to_index(hmgr, next);
        }
    }
}
