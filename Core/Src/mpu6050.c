/* mpu6050.c */

#include "mpu6050.h"
#include <string.h>

/* MPU6050 registers */
#define MPU6050_REG_WHO_AM_I   0x75u
#define MPU6050_REG_PWR_MGMT_1 0x6Bu
#define MPU6050_REG_SMPLRT_DIV 0x19u
#define MPU6050_REG_CONFIG     0x1Au
#define MPU6050_REG_GYRO_CFG   0x1Bu
#define MPU6050_REG_ACCEL_CFG  0x1Cu
#define MPU6050_REG_TEMP_OUT   0x41u
#define MPU6050_REG_GYRO_OUT   0x43u
#define MPU6050_BURST_REG      0x3Bu

#define MPU_DEFAULT_DT_US              8000u
#define MPU_CALIB_DEFAULT_WARMUP_MS     400u
#define MPU_CALIB_MIN_WINDOW_MS        1500u
#define MPU_CALIB_MAX_WINDOW_MS        3000u
#define MPU_CALIB_MIN_SAMPLES            32u
#define MPU_STATIONARY_GYRO_MAX_MDPS   5000
#define MPU_STATIONARY_ACCEL_TOL_MG     150
#define MPU_STATIONARY_ACCEL_DELTA_MG   120
#define MPU_COMP_ALPHA_PERMILLE         980
#define MPU_GRAVITY_LPF_SHIFT             4

#define MERGE(i) ((int16_t)((uint16_t)hmpu->rx_buffer[(i)] << 8 | hmpu->rx_buffer[(i) + 1u]))

static void MPU_I2C_RequestApproved(void *ctx);
static void MPU_I2C_TxComplete(void *ctx);
static void MPU_I2C_RxComplete(void *ctx);
static void MPU_I2C_Error(void *ctx, I2C_ManagerError err);

__weak uint32_t MPU6050_GetTimeUs(void)
{
    return HAL_GetTick() * 1000u;
}

static int32_t MPU_Abs32(int32_t v)
{
    return (v < 0) ? -v : v;
}

static int16_t MPU_ClampI16(int32_t v)
{
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return (int16_t)v;
}

static uint32_t MPU_Isqrt32(uint32_t x)
{
    uint32_t op = x;
    uint32_t res = 0;
    uint32_t one = 1UL << 30;

    while (one > op) {
        one >>= 2;
    }

    while (one != 0u) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }

    return res;
}

/* Compact atan2 approximation in millidegrees. Max error is acceptable for a
 * BluePill UI/control estimate and avoids libm atan2f/sqrtf flash cost. */
static int32_t MPU_Atan2Md(int32_t y, int32_t x)
{
    int32_t abs_y;
    int32_t angle;
    int32_t r;
    int32_t den;

    if (x == 0 && y == 0) {
        return 0;
    }

    abs_y = MPU_Abs32(y) + 1;
    if (x >= 0) {
        den = x + abs_y;
        r = (den != 0) ? (((x - abs_y) * 1000) / den) : 0;
        angle = 45000 - ((45000 * r) / 1000);
    } else {
        den = abs_y - x;
        r = (den != 0) ? (((x + abs_y) * 1000) / den) : 0;
        angle = 135000 - ((45000 * r) / 1000);
    }

    return (y < 0) ? -angle : angle;
}

static int32_t MPU_AccelRollMd(int16_t ay, int16_t az)
{
    return MPU_Atan2Md((int32_t)ay, (int32_t)az);
}

static int32_t MPU_AccelPitchMd(int16_t ax, int16_t ay, int16_t az)
{
    uint32_t yz = (uint32_t)((int32_t)ay * ay + (int32_t)az * az);
    return MPU_Atan2Md(-(int32_t)ax, (int32_t)MPU_Isqrt32(yz));
}

static uint8_t MPU_IsStationary(MPU6050_Handle_t *hmpu)
{
    const int16_t ax = hmpu->data.accel_x_mg;
    const int16_t ay = hmpu->data.accel_y_mg;
    const int16_t az = hmpu->data.accel_z_mg;
    uint32_t accel_norm;
    uint8_t stable_delta = 1u;
    uint8_t stationary;

    accel_norm = MPU_Isqrt32((uint32_t)((int32_t)ax * ax +
                                        (int32_t)ay * ay +
                                        (int32_t)az * az));

    if (hmpu->prev_accel_valid) {
        stable_delta =
            (MPU_Abs32((int32_t)ax - hmpu->prev_accel_x_mg) <= MPU_STATIONARY_ACCEL_DELTA_MG) &&
            (MPU_Abs32((int32_t)ay - hmpu->prev_accel_y_mg) <= MPU_STATIONARY_ACCEL_DELTA_MG) &&
            (MPU_Abs32((int32_t)az - hmpu->prev_accel_z_mg) <= MPU_STATIONARY_ACCEL_DELTA_MG);
    }

    stationary =
        (MPU_Abs32(hmpu->data.gyro_x_mdps) <= MPU_STATIONARY_GYRO_MAX_MDPS) &&
        (MPU_Abs32(hmpu->data.gyro_y_mdps) <= MPU_STATIONARY_GYRO_MAX_MDPS) &&
        (MPU_Abs32(hmpu->data.gyro_z_mdps) <= MPU_STATIONARY_GYRO_MAX_MDPS) &&
        (MPU_Abs32((int32_t)accel_norm - 1000) <= MPU_STATIONARY_ACCEL_TOL_MG) &&
        stable_delta;

    hmpu->prev_accel_x_mg = ax;
    hmpu->prev_accel_y_mg = ay;
    hmpu->prev_accel_z_mg = az;
    hmpu->prev_accel_valid = 1u;

    return stationary;
}

static uint16_t MPU_ClampDtUs(uint32_t dt_us)
{
    if (dt_us == 0u) {
        return MPU_DEFAULT_DT_US;
    }
    if (dt_us > 65535u) {
        return 65535u;
    }
    return (uint16_t)dt_us;
}

static void MPU_UpdateSampleTiming(MPU6050_Handle_t *hmpu)
{
    uint32_t now = MPU6050_GetTimeUs();
    uint32_t dt = (hmpu->last_sample_time_us == 0u)
        ? MPU_DEFAULT_DT_US
        : (now - hmpu->last_sample_time_us);

    hmpu->last_sample_time_us = now;
    hmpu->latest.sample_dt_us = MPU_ClampDtUs(dt);
}

static void MPU_SetError(MPU6050_Handle_t *hmpu)
{
    SET_FLAG(hmpu->flags, MPU_ERROR);
    CLEAR_FLAG(hmpu->flags, MPU_VALID | MPU_NEW_READING);
    hmpu->latest.flags = hmpu->flags.byte;
}

static void MPU_ResetCalibrationAccum(MPU6050_Handle_t *hmpu)
{
    hmpu->calib_sum_x = 0;
    hmpu->calib_sum_y = 0;
    hmpu->calib_sum_z = 0;
    hmpu->calib_accel_sum_x = 0;
    hmpu->calib_accel_sum_y = 0;
    hmpu->calib_accel_sum_z = 0;
    hmpu->calib_count = 0;
}

static void MPU_FinishCalibration(MPU6050_Handle_t *hmpu)
{
    int16_t gx;
    int16_t gy;
    int16_t gz;

    if (hmpu->calib_count == 0u) {
        MPU_SetError(hmpu);
        return;
    }

    hmpu->gyro_bias_x = hmpu->calib_sum_x / hmpu->calib_count;
    hmpu->gyro_bias_y = hmpu->calib_sum_y / hmpu->calib_count;
    hmpu->gyro_bias_z = hmpu->calib_sum_z / hmpu->calib_count;

    gx = MPU_ClampI16(hmpu->calib_accel_sum_x / hmpu->calib_count);
    gy = MPU_ClampI16(hmpu->calib_accel_sum_y / hmpu->calib_count);
    gz = MPU_ClampI16(hmpu->calib_accel_sum_z / hmpu->calib_count);

    hmpu->gravity_x_mg = gx;
    hmpu->gravity_y_mg = gy;
    hmpu->gravity_z_mg = gz;
    hmpu->roll_ref_md = MPU_AccelRollMd(gy, gz);
    hmpu->pitch_ref_md = MPU_AccelPitchMd(gx, gy, gz);

    hmpu->angle_x_md = 0;
    hmpu->angle_y_md = 0;
    hmpu->angle_z_md = 0;
    hmpu->rem_x = 0;
    hmpu->rem_y = 0;
    hmpu->rem_z = 0;

    CLEAR_FLAG(hmpu->flags, MPU_CALIBRATING | MPU_ERROR);
    SET_FLAG(hmpu->flags, CALIBRATED | MPU_STATIONARY);
}

static void MPU_ProcessCalibration(MPU6050_Handle_t *hmpu, uint8_t stationary)
{
    uint32_t elapsed_ms = HAL_GetTick() - hmpu->calib_started_ms;

    if (elapsed_ms < hmpu->calib_warmup_ms) {
        return;
    }

    if (stationary) {
        hmpu->calib_sum_x += hmpu->data.gyro_x_mdps;
        hmpu->calib_sum_y += hmpu->data.gyro_y_mdps;
        hmpu->calib_sum_z += hmpu->data.gyro_z_mdps;
        hmpu->calib_accel_sum_x += hmpu->data.accel_x_mg;
        hmpu->calib_accel_sum_y += hmpu->data.accel_y_mg;
        hmpu->calib_accel_sum_z += hmpu->data.accel_z_mg;
        hmpu->calib_count++;
    }

    if (elapsed_ms < ((uint32_t)hmpu->calib_warmup_ms + hmpu->calib_window_ms)) {
        return;
    }

    if (hmpu->calib_count >= hmpu->calib_target) {
        MPU_FinishCalibration(hmpu);
        return;
    }

    /* If the car moved, keep trying but expose an error/quality flag. */
    MPU_SetError(hmpu);
    MPU_ResetCalibrationAccum(hmpu);
    hmpu->calib_started_ms = HAL_GetTick() - hmpu->calib_warmup_ms;
}

static void MPU_UpdateEstimator(MPU6050_Handle_t *hmpu, uint8_t stationary)
{
    const uint16_t dt_us = (hmpu->latest.sample_dt_us != 0u)
        ? hmpu->latest.sample_dt_us
        : MPU_DEFAULT_DT_US;
    int64_t vx;
    int64_t vy;
    int64_t vz;
    int32_t pred_x;
    int32_t pred_y;
    int32_t pred_z;
    int32_t roll_acc_md;
    int32_t pitch_acc_md;
    uint16_t next_seq;

    if (!IS_FLAG_SET(hmpu->flags, CALIBRATED)) {
        return;
    }

    vx = ((int64_t)(hmpu->data.gyro_x_mdps - hmpu->gyro_bias_x) * dt_us) + hmpu->rem_x;
    vy = ((int64_t)(hmpu->data.gyro_y_mdps - hmpu->gyro_bias_y) * dt_us) + hmpu->rem_y;
    vz = ((int64_t)(hmpu->data.gyro_z_mdps - hmpu->gyro_bias_z) * dt_us) + hmpu->rem_z;

    pred_x = hmpu->angle_x_md + (int32_t)(vx / 1000000);
    pred_y = hmpu->angle_y_md + (int32_t)(vy / 1000000);
    pred_z = hmpu->angle_z_md + (int32_t)(vz / 1000000);

    hmpu->rem_x = (int32_t)(vx % 1000000);
    hmpu->rem_y = (int32_t)(vy % 1000000);
    hmpu->rem_z = (int32_t)(vz % 1000000);

    roll_acc_md = MPU_AccelRollMd(hmpu->data.accel_y_mg, hmpu->data.accel_z_mg) - hmpu->roll_ref_md;
    pitch_acc_md = MPU_AccelPitchMd(hmpu->data.accel_x_mg,
                                    hmpu->data.accel_y_mg,
                                    hmpu->data.accel_z_mg) - hmpu->pitch_ref_md;

    hmpu->angle_x_md =
        (pred_x * MPU_COMP_ALPHA_PERMILLE + roll_acc_md * (1000 - MPU_COMP_ALPHA_PERMILLE)) / 1000;
    hmpu->angle_y_md =
        (pred_y * MPU_COMP_ALPHA_PERMILLE + pitch_acc_md * (1000 - MPU_COMP_ALPHA_PERMILLE)) / 1000;
    hmpu->angle_z_md = pred_z;

    hmpu->gravity_x_mg += (int16_t)((hmpu->data.accel_x_mg - hmpu->gravity_x_mg) >> MPU_GRAVITY_LPF_SHIFT);
    hmpu->gravity_y_mg += (int16_t)((hmpu->data.accel_y_mg - hmpu->gravity_y_mg) >> MPU_GRAVITY_LPF_SHIFT);
    hmpu->gravity_z_mg += (int16_t)((hmpu->data.accel_z_mg - hmpu->gravity_z_mg) >> MPU_GRAVITY_LPF_SHIFT);

    next_seq = (uint16_t)(hmpu->latest.sample_seq + 1u);
    hmpu->latest.sample_seq = (next_seq == 0u) ? 1u : next_seq;
    hmpu->latest.accel_x_mg = hmpu->data.accel_x_mg;
    hmpu->latest.accel_y_mg = hmpu->data.accel_y_mg;
    hmpu->latest.accel_z_mg = hmpu->data.accel_z_mg;
    hmpu->latest.linear_accel_x_mg = hmpu->data.accel_x_mg - hmpu->gravity_x_mg;
    hmpu->latest.linear_accel_y_mg = hmpu->data.accel_y_mg - hmpu->gravity_y_mg;
    hmpu->latest.linear_accel_z_mg = hmpu->data.accel_z_mg - hmpu->gravity_z_mg;
    hmpu->latest.gyro_x_mdps = hmpu->data.gyro_x_mdps - hmpu->gyro_bias_x;
    hmpu->latest.gyro_y_mdps = hmpu->data.gyro_y_mdps - hmpu->gyro_bias_y;
    hmpu->latest.gyro_z_mdps = hmpu->data.gyro_z_mdps - hmpu->gyro_bias_z;
    hmpu->latest.roll_md = hmpu->angle_x_md;
    hmpu->latest.pitch_md = hmpu->angle_y_md;
    hmpu->latest.yaw_md = hmpu->angle_z_md;

    CLEAR_FLAG(hmpu->flags, MPU_ERROR | MPU_STATIONARY);
    SET_FLAG(hmpu->flags, CALIBRATED | MPU_VALID | MPU_NEW_READING);
    if (stationary) {
        SET_FLAG(hmpu->flags, MPU_STATIONARY);
    }
    hmpu->latest.flags = hmpu->flags.byte;
}

static HAL_StatusTypeDef MPU6050_RequestRead(MPU6050_Handle_t *hmpu,
                                             MPU6050_ReadMode mode,
                                             uint8_t start_reg,
                                             uint8_t len)
{
    HAL_StatusTypeDef res;

    if (!hmpu || !hmpu->i2c_mgr) {
        return HAL_ERROR;
    }

    if (hmpu->transfer_state != MPU6050_XFER_IDLE) {
        return HAL_BUSY;
    }

    hmpu->read_mode = mode;
    hmpu->tx_buffer[0] = start_reg;
    hmpu->expected_rx_len = len;
    hmpu->transfer_state = MPU6050_XFER_WAIT_GRANT;

    res = I2C_Manager_RequestBusPrio(hmpu->i2c_mgr, hmpu->dev_id, I2C_REQ_PRIO_HIGH);
    if (res == HAL_ERROR) {
        hmpu->transfer_state = MPU6050_XFER_IDLE;
        MPU_SetError(hmpu);
    }

    return res;
}

HAL_StatusTypeDef MPU6050_Init(MPU6050_Handle_t *hmpu,
                               uint8_t address,
                               I2C_HandleTypeDef *hi2c,
                               MPU6050_Callback on_data_ready,
                               volatile bool *trigger)
{
    if (!hmpu || !hi2c || !trigger) {
        return HAL_ERROR;
    }

    memset(hmpu, 0, sizeof(*hmpu));
    hmpu->hi2c = hi2c;
    hmpu->i2c_mgr = NULL;
    hmpu->dev_id = (uint8_t)I2C_MANAGER_DEVICE_NONE;
    hmpu->i2c_address = address;
    hmpu->tx_buffer[0] = MPU6050_BURST_REG;
    hmpu->expected_rx_len = sizeof(hmpu->rx_buffer);
    hmpu->read_mode = MPU6050_READ_ALL;
    hmpu->last_read_mode = MPU6050_READ_ALL;
    hmpu->transfer_state = MPU6050_XFER_IDLE;
    hmpu->on_data_ready_cb = on_data_ready;
    hmpu->trigger = trigger;
    hmpu->dt_div = 125;

    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_BindI2CManager(MPU6050_Handle_t *hmpu,
                                         I2C_ManagerHandle *hmgr,
                                         I2C_DeviceID dev_id,
                                         uint8_t priority)
{
    if (!hmpu || !hmgr) {
        return HAL_ERROR;
    }

    hmpu->i2c_mgr = hmgr;
    hmpu->dev_id = dev_id;

    return I2C_Manager_RegisterDevice(hmgr,
                                      dev_id,
                                      hmpu->i2c_address,
                                      priority,
                                      hmpu,
                                      MPU_I2C_RequestApproved,
                                      MPU_I2C_TxComplete,
                                      MPU_I2C_RxComplete,
                                      MPU_I2C_Error);
}

HAL_StatusTypeDef MPU6050_Configure(MPU6050_Handle_t *hmpu)
{
    uint8_t tmp;

    if (!hmpu || !hmpu->hi2c) {
        return HAL_ERROR;
    }

    if (HAL_I2C_Mem_Read(hmpu->hi2c,
                         (uint16_t)(hmpu->i2c_address << 1),
                         MPU6050_REG_WHO_AM_I,
                         I2C_MEMADD_SIZE_8BIT,
                         &tmp,
                         1,
                         100) != HAL_OK) {
        return HAL_ERROR;
    }

    if (tmp != hmpu->i2c_address) {
        return HAL_ERROR;
    }

    tmp = 0u;
    if (HAL_I2C_Mem_Write(hmpu->hi2c, (uint16_t)(hmpu->i2c_address << 1),
                          MPU6050_REG_PWR_MGMT_1, I2C_MEMADD_SIZE_8BIT, &tmp, 1, 100) != HAL_OK) {
        return HAL_ERROR;
    }
    tmp = 7u;
    if (HAL_I2C_Mem_Write(hmpu->hi2c, (uint16_t)(hmpu->i2c_address << 1),
                          MPU6050_REG_SMPLRT_DIV, I2C_MEMADD_SIZE_8BIT, &tmp, 1, 100) != HAL_OK) {
        return HAL_ERROR;
    }
    tmp = 3u;
    if (HAL_I2C_Mem_Write(hmpu->hi2c, (uint16_t)(hmpu->i2c_address << 1),
                          MPU6050_REG_CONFIG, I2C_MEMADD_SIZE_8BIT, &tmp, 1, 100) != HAL_OK) {
        return HAL_ERROR;
    }
    tmp = 0u;
    if (HAL_I2C_Mem_Write(hmpu->hi2c, (uint16_t)(hmpu->i2c_address << 1),
                          MPU6050_REG_GYRO_CFG, I2C_MEMADD_SIZE_8BIT, &tmp, 1, 100) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_I2C_Mem_Write(hmpu->hi2c, (uint16_t)(hmpu->i2c_address << 1),
                          MPU6050_REG_ACCEL_CFG, I2C_MEMADD_SIZE_8BIT, &tmp, 1, 100) != HAL_OK) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

void MPU6050_CheckTrigger(MPU6050_Handle_t *hmpu)
{
    if (!hmpu || !hmpu->trigger) {
        return;
    }

    if (*(hmpu->trigger)) {
        *(hmpu->trigger) = false;
        (void)MPU6050_RequestAllData(hmpu);
    }
}

void MPU6050_Process(MPU6050_Handle_t *hmpu)
{
    uint8_t stationary = 0u;

    if (!hmpu) {
        return;
    }

    MPU6050_CheckTrigger(hmpu);

    if (!IS_FLAG_SET(hmpu->flags, DATA_READY)) {
        return;
    }

    CLEAR_FLAG(hmpu->flags, DATA_READY);
    MPU6050_Convert(hmpu);

    if (hmpu->last_read_mode == MPU6050_READ_ALL) {
        stationary = MPU_IsStationary(hmpu);
        if (stationary) {
            SET_FLAG(hmpu->flags, MPU_STATIONARY);
        } else {
            CLEAR_FLAG(hmpu->flags, MPU_STATIONARY);
        }

        if (IS_FLAG_SET(hmpu->flags, MPU_CALIBRATING) ||
            !IS_FLAG_SET(hmpu->flags, CALIBRATED)) {
            MPU_ProcessCalibration(hmpu, stationary);
        } else {
            MPU_UpdateEstimator(hmpu, stationary);
        }
    }

    if (hmpu->on_data_ready_cb) {
        hmpu->on_data_ready_cb();
    }
}

HAL_StatusTypeDef MPU6050_RequestAllData(MPU6050_Handle_t *hmpu)
{
    return MPU6050_RequestRead(hmpu, MPU6050_READ_ALL, MPU6050_BURST_REG, 14u);
}

HAL_StatusTypeDef MPU6050_RequestAccel(MPU6050_Handle_t *hmpu)
{
    return MPU6050_RequestRead(hmpu, MPU6050_READ_ACCEL, MPU6050_BURST_REG, 6u);
}

HAL_StatusTypeDef MPU6050_RequestGyro(MPU6050_Handle_t *hmpu)
{
    return MPU6050_RequestRead(hmpu, MPU6050_READ_GYRO, MPU6050_REG_GYRO_OUT, 6u);
}

HAL_StatusTypeDef MPU6050_RequestTemp(MPU6050_Handle_t *hmpu)
{
    return MPU6050_RequestRead(hmpu, MPU6050_READ_TEMP, MPU6050_REG_TEMP_OUT, 2u);
}

HAL_StatusTypeDef MPU6050_RequestOrientation(MPU6050_Handle_t *hmpu)
{
    return MPU6050_RequestAllData(hmpu);
}

const MPU6050_IntData_t *MPU6050_GetData(MPU6050_Handle_t *hmpu)
{
    return hmpu ? &hmpu->data : NULL;
}

HAL_StatusTypeDef MPU6050_GetLatest(MPU6050_Handle_t *hmpu, MPU6050_Snapshot_t *out)
{
    if (!hmpu || !out) {
        return HAL_ERROR;
    }

    *out = hmpu->latest;
    return IS_FLAG_SET(hmpu->flags, MPU_VALID) ? HAL_OK : HAL_ERROR;
}

uint8_t MPU6050_HasNewSample(const MPU6050_Handle_t *hmpu, uint16_t last_seq)
{
    return (hmpu &&
            IS_FLAG_SET(hmpu->flags, MPU_VALID) &&
            hmpu->latest.sample_seq != last_seq) ? 1u : 0u;
}

void MPU6050_ClearNewFlag(MPU6050_Handle_t *hmpu)
{
    if (!hmpu) {
        return;
    }
    CLEAR_FLAG(hmpu->flags, MPU_NEW_READING);
    hmpu->latest.flags = hmpu->flags.byte;
}

void MPU6050_SetReferenceNow(MPU6050_Handle_t *hmpu)
{
    if (!hmpu) {
        return;
    }

    hmpu->roll_ref_md = MPU_AccelRollMd(hmpu->data.accel_y_mg, hmpu->data.accel_z_mg);
    hmpu->pitch_ref_md = MPU_AccelPitchMd(hmpu->data.accel_x_mg,
                                          hmpu->data.accel_y_mg,
                                          hmpu->data.accel_z_mg);
    hmpu->gravity_x_mg = hmpu->data.accel_x_mg;
    hmpu->gravity_y_mg = hmpu->data.accel_y_mg;
    hmpu->gravity_z_mg = hmpu->data.accel_z_mg;
    hmpu->angle_x_md = 0;
    hmpu->angle_y_md = 0;
    hmpu->angle_z_md = 0;
    hmpu->rem_x = 0;
    hmpu->rem_y = 0;
    hmpu->rem_z = 0;
}

void MPU6050_GetOrientation(const MPU6050_Handle_t *hmpu, MPU6050_Orientation_t *out)
{
    if (!hmpu || !out) {
        return;
    }

    out->angle_x_md = hmpu->angle_x_md;
    out->angle_y_md = hmpu->angle_y_md;
    out->angle_z_md = hmpu->angle_z_md;
}

void MPU6050_Convert(MPU6050_Handle_t *hmpu)
{
    if (!hmpu) {
        return;
    }

    switch (hmpu->last_read_mode) {
        case MPU6050_READ_ALL: {
            int16_t ax = MERGE(0u), ay = MERGE(2u), az = MERGE(4u);
            int16_t t = MERGE(6u), gx = MERGE(8u), gy = MERGE(10u), gz = MERGE(12u);

            hmpu->data.accel_x_mg = (int16_t)((int32_t)ax * 1000 / 16384);
            hmpu->data.accel_y_mg = (int16_t)((int32_t)ay * 1000 / 16384);
            hmpu->data.accel_z_mg = (int16_t)((int32_t)az * 1000 / 16384);
            hmpu->data.temperature_celsius_x100 = (int16_t)((int32_t)t * 100 / 340 + 3653);
            hmpu->data.gyro_x_mdps = (int32_t)gx * 1000 / 131;
            hmpu->data.gyro_y_mdps = (int32_t)gy * 1000 / 131;
            hmpu->data.gyro_z_mdps = (int32_t)gz * 1000 / 131;
            break;
        }
        case MPU6050_READ_ACCEL: {
            int16_t ax = MERGE(0u), ay = MERGE(2u), az = MERGE(4u);
            hmpu->data.accel_x_mg = (int16_t)((int32_t)ax * 1000 / 16384);
            hmpu->data.accel_y_mg = (int16_t)((int32_t)ay * 1000 / 16384);
            hmpu->data.accel_z_mg = (int16_t)((int32_t)az * 1000 / 16384);
            break;
        }
        case MPU6050_READ_TEMP: {
            int16_t t = MERGE(0u);
            hmpu->data.temperature_celsius_x100 = (int16_t)((int32_t)t * 100 / 340 + 3653);
            break;
        }
        case MPU6050_READ_GYRO:
        case MPU6050_READ_ORIENTATION: {
            int16_t gx = MERGE(0u), gy = MERGE(2u), gz = MERGE(4u);
            hmpu->data.gyro_x_mdps = (int32_t)gx * 1000 / 131;
            hmpu->data.gyro_y_mdps = (int32_t)gy * 1000 / 131;
            hmpu->data.gyro_z_mdps = (int32_t)gz * 1000 / 131;
            break;
        }
        default:
            break;
    }
}

HAL_StatusTypeDef MPU6050_ReadWhoAmI(MPU6050_Handle_t *hmpu, uint8_t *whoami)
{
    if (!hmpu || !whoami) {
        return HAL_ERROR;
    }

    return HAL_I2C_Mem_Read(hmpu->hi2c,
                            (uint16_t)(hmpu->i2c_address << 1),
                            MPU6050_REG_WHO_AM_I,
                            I2C_MEMADD_SIZE_8BIT,
                            whoami,
                            1,
                            100);
}

void MPU6050_StartCalibration(MPU6050_Handle_t *hmpu, uint16_t warmup_ms, uint16_t window_ms)
{
    uint32_t expected_samples;

    if (!hmpu) {
        return;
    }

    if (warmup_ms == 0u) {
        warmup_ms = MPU_CALIB_DEFAULT_WARMUP_MS;
    }
    if (window_ms < MPU_CALIB_MIN_WINDOW_MS) {
        window_ms = MPU_CALIB_MIN_WINDOW_MS;
    }
    if (window_ms > MPU_CALIB_MAX_WINDOW_MS) {
        window_ms = MPU_CALIB_MAX_WINDOW_MS;
    }

    expected_samples = ((uint32_t)window_ms * 1000u) / MPU_DEFAULT_DT_US;
    expected_samples = (expected_samples * 3u) / 4u;
    if (expected_samples < MPU_CALIB_MIN_SAMPLES) {
        expected_samples = MPU_CALIB_MIN_SAMPLES;
    }

    MPU_ResetCalibrationAccum(hmpu);
    hmpu->calib_warmup_ms = warmup_ms;
    hmpu->calib_window_ms = window_ms;
    hmpu->calib_target = (uint16_t)expected_samples;
    hmpu->calib_started_ms = HAL_GetTick();
    hmpu->prev_accel_valid = 0u;
    hmpu->last_sample_time_us = 0u;

    CLEAR_FLAG(hmpu->flags, CALIBRATED | MPU_VALID | MPU_NEW_READING | MPU_ERROR);
    SET_FLAG(hmpu->flags, MPU_CALIBRATING);
    hmpu->latest.flags = hmpu->flags.byte;

    if (hmpu->trigger) {
        *(hmpu->trigger) = true;
    }
}

void MPU6050_CalibrateGyro(MPU6050_Handle_t *hmpu, uint16_t samples)
{
    uint32_t window_ms = (uint32_t)samples * MPU_DEFAULT_DT_US / 1000u;

    if (window_ms < MPU_CALIB_MIN_WINDOW_MS) {
        window_ms = 2000u;
    }

    MPU6050_StartCalibration(hmpu,
                             MPU_CALIB_DEFAULT_WARMUP_MS,
                             (uint16_t)window_ms);
}

static void MPU_I2C_RequestApproved(void *ctx)
{
    MPU6050_Handle_t *hmpu = (MPU6050_Handle_t *)ctx;

    if (!hmpu) {
        return;
    }

    if (hmpu->transfer_state != MPU6050_XFER_WAIT_GRANT) {
        (void)I2C_Manager_ReleaseBus(hmpu->i2c_mgr, hmpu->dev_id);
        return;
    }

    hmpu->transfer_state = MPU6050_XFER_TX_REG;
    if (HAL_I2C_Master_Transmit_DMA(hmpu->hi2c,
                                    (uint16_t)(hmpu->i2c_address << 1),
                                    &hmpu->tx_buffer[0],
                                    1) != HAL_OK) {
        hmpu->transfer_state = MPU6050_XFER_IDLE;
        MPU_SetError(hmpu);
        (void)I2C_Manager_ReleaseBus(hmpu->i2c_mgr, hmpu->dev_id);
        return;
    }

    SET_FLAG(hmpu->flags, TX_SENT);
}

static void MPU_I2C_TxComplete(void *ctx)
{
    MPU6050_Handle_t *hmpu = (MPU6050_Handle_t *)ctx;

    if (!hmpu || hmpu->transfer_state != MPU6050_XFER_TX_REG) {
        return;
    }

    hmpu->transfer_state = MPU6050_XFER_RX_DATA;
    if (HAL_I2C_Master_Receive_DMA(hmpu->hi2c,
                                   (uint16_t)(hmpu->i2c_address << 1),
                                   hmpu->rx_buffer,
                                   hmpu->expected_rx_len) != HAL_OK) {
        hmpu->transfer_state = MPU6050_XFER_IDLE;
        CLEAR_FLAG(hmpu->flags, TX_SENT);
        MPU_SetError(hmpu);
        (void)I2C_Manager_ReleaseBus(hmpu->i2c_mgr, hmpu->dev_id);
        return;
    }

    CLEAR_FLAG(hmpu->flags, TX_SENT);
}

static void MPU_I2C_RxComplete(void *ctx)
{
    MPU6050_Handle_t *hmpu = (MPU6050_Handle_t *)ctx;

    if (!hmpu || hmpu->transfer_state != MPU6050_XFER_RX_DATA) {
        return;
    }

    hmpu->transfer_state = MPU6050_XFER_IDLE;
    hmpu->last_read_mode = hmpu->read_mode;
    MPU_UpdateSampleTiming(hmpu);

    (void)I2C_Manager_ReleaseBus(hmpu->i2c_mgr, hmpu->dev_id);
    CLEAR_FLAG(hmpu->flags, MPU_ERROR);
    SET_FLAG(hmpu->flags, DATA_READY);
}

static void MPU_I2C_Error(void *ctx, I2C_ManagerError err)
{
    MPU6050_Handle_t *hmpu = (MPU6050_Handle_t *)ctx;
    (void)err;

    if (!hmpu) {
        return;
    }

    hmpu->transfer_state = MPU6050_XFER_IDLE;
    hmpu->last_read_mode = hmpu->read_mode;
    CLEAR_FLAG(hmpu->flags, TX_SENT | DATA_READY);
    memset(hmpu->rx_buffer, 0, hmpu->expected_rx_len);
    MPU_SetError(hmpu);
}
