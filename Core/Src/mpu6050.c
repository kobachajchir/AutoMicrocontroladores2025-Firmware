/* mpu6050.c */

#include "mpu6050.h"
#include <string.h>

/* Registros MPU6050 */
#define MPU6050_REG_WHO_AM_I   0x75
#define MPU6050_REG_PWR_MGMT_1 0x6B
#define MPU6050_REG_SMPLRT_DIV 0x19
#define MPU6050_REG_CONFIG     0x1A
#define MPU6050_REG_GYRO_CFG   0x1B
#define MPU6050_REG_ACCEL_CFG  0x1C
#define MPU6050_REG_TEMP_OUT   0x41
#define MPU6050_REG_GYRO_OUT   0x43
#define MPU6050_BURST_REG      0x3B
#define MPU_DT_MS 8
#define MERGE(i) ((int16_t)(hmpu->rx_buffer[i] << 8 | hmpu->rx_buffer[i+1]))

static void MPU_I2C_RequestApproved(void *ctx);
static void MPU_I2C_TxComplete(void *ctx);
static void MPU_I2C_RxComplete(void *ctx);
static void MPU_I2C_Error(void *ctx, I2C_ManagerError err);
static HAL_StatusTypeDef MPU6050_RequestTransfer(MPU6050_Handle_t *hmpu,
                                                 MPU6050_Operation op,
                                                 uint8_t tx_len,
                                                 uint8_t rx_len);
static HAL_StatusTypeDef MPU6050_RequestWrite(MPU6050_Handle_t *hmpu,
                                              uint8_t reg,
                                              uint8_t value);
static HAL_StatusTypeDef MPU6050_RequestConfigRead(MPU6050_Handle_t *hmpu,
                                                   uint8_t reg);
static void MPU6050_AdvanceConfig(MPU6050_Handle_t *hmpu);

static HAL_StatusTypeDef MPU6050_RequestRead(MPU6050_Handle_t *hmpu, MPU6050_ReadMode mode, uint8_t start_reg, uint8_t len)
{
    if (!hmpu || !hmpu->i2c_mgr) {
        return HAL_ERROR;
    }

    if (!hmpu->configured) {
        return HAL_BUSY;
    }

    if (hmpu->transfer_state != MPU6050_XFER_IDLE) {
        return HAL_BUSY;
    }

    hmpu->read_mode = mode;
    hmpu->tx_buffer[0] = start_reg;
    hmpu->tx_len = 1;
    return MPU6050_RequestTransfer(hmpu, MPU6050_OP_DATA, 1, len);
}

static HAL_StatusTypeDef MPU6050_RequestTransfer(MPU6050_Handle_t *hmpu,
                                                 MPU6050_Operation op,
                                                 uint8_t tx_len,
                                                 uint8_t rx_len)
{
    if (!hmpu || !hmpu->i2c_mgr) {
        return HAL_ERROR;
    }

    if (hmpu->transfer_state != MPU6050_XFER_IDLE) {
        return HAL_BUSY;
    }

    hmpu->operation = op;
    hmpu->tx_len = tx_len;
    hmpu->expected_rx_len = rx_len;
    hmpu->transfer_state = MPU6050_XFER_WAIT_GRANT;

    HAL_StatusTypeDef res = I2C_Manager_RequestBus(hmpu->i2c_mgr, hmpu->dev_id);
    if (res == HAL_ERROR) {
        hmpu->transfer_state = MPU6050_XFER_IDLE;
        hmpu->operation = MPU6050_OP_IDLE;
    }

    return res;
}

static HAL_StatusTypeDef MPU6050_RequestWrite(MPU6050_Handle_t *hmpu,
                                              uint8_t reg,
                                              uint8_t value)
{
    if (!hmpu) {
        return HAL_ERROR;
    }
    hmpu->tx_buffer[0] = reg;
    hmpu->tx_buffer[1] = value;
    return MPU6050_RequestTransfer(hmpu, MPU6050_OP_CONFIG, 2, 0);
}

static HAL_StatusTypeDef MPU6050_RequestConfigRead(MPU6050_Handle_t *hmpu,
                                                   uint8_t reg)
{
    if (!hmpu) {
        return HAL_ERROR;
    }
    hmpu->tx_buffer[0] = reg;
    return MPU6050_RequestTransfer(hmpu, MPU6050_OP_CONFIG, 1, 1);
}

static void MPU6050_AdvanceConfig(MPU6050_Handle_t *hmpu)
{
    if (!hmpu) {
        return;
    }

    switch (hmpu->config_state) {
        case MPU6050_CONFIG_WHOAMI:
            hmpu->config_state = MPU6050_CONFIG_PWR;
            (void)MPU6050_RequestWrite(hmpu, MPU6050_REG_PWR_MGMT_1, 0);
            break;
        case MPU6050_CONFIG_PWR:
            hmpu->config_state = MPU6050_CONFIG_SMPLRT;
            (void)MPU6050_RequestWrite(hmpu, MPU6050_REG_SMPLRT_DIV, 7);
            break;
        case MPU6050_CONFIG_SMPLRT:
            hmpu->config_state = MPU6050_CONFIG_DLPF;
            (void)MPU6050_RequestWrite(hmpu, MPU6050_REG_CONFIG, 3);
            break;
        case MPU6050_CONFIG_DLPF:
            hmpu->config_state = MPU6050_CONFIG_GYRO;
            (void)MPU6050_RequestWrite(hmpu, MPU6050_REG_GYRO_CFG, 0);
            break;
        case MPU6050_CONFIG_GYRO:
            hmpu->config_state = MPU6050_CONFIG_ACCEL;
            (void)MPU6050_RequestWrite(hmpu, MPU6050_REG_ACCEL_CFG, 0);
            break;
        case MPU6050_CONFIG_ACCEL:
            hmpu->config_state = MPU6050_CONFIG_DONE;
            hmpu->configured = true;
            hmpu->operation = MPU6050_OP_IDLE;
            if (hmpu->calib_pending) {
                hmpu->calib_pending = false;
                if (hmpu->trigger) {
                    *(hmpu->trigger) = true;
                }
            }
            break;
        default:
            break;
    }
}

/* -------------------------------------------------------------------------
 * API pública:
 * -------------------------------------------------------------------------*/

HAL_StatusTypeDef MPU6050_Init(
    MPU6050_Handle_t *hmpu,
    uint8_t           address,
    I2C_HandleTypeDef *hi2c,
    MPU6050_Callback  on_data_ready,
    volatile bool    *trigger    // <-- debe estar aquí
) {
    if (!hmpu || !hi2c || !trigger) {
        return HAL_ERROR;
    }
    /* Rellenar handle */
    hmpu->hi2c             = hi2c;
    hmpu->i2c_mgr          = NULL;
    hmpu->dev_id           = (uint8_t)I2C_MANAGER_DEVICE_NONE;
    hmpu->i2c_address      = address;
    hmpu->tx_buffer[0]     = MPU6050_BURST_REG;
    hmpu->expected_rx_len  = sizeof(hmpu->rx_buffer);
    hmpu->read_mode        = MPU6050_READ_ALL;
    hmpu->last_read_mode   = MPU6050_READ_ALL;
    hmpu->transfer_state   = MPU6050_XFER_IDLE;
    hmpu->operation        = MPU6050_OP_IDLE;
    hmpu->config_state     = MPU6050_CONFIG_NONE;
    hmpu->configured       = false;
    hmpu->calib_pending    = false;
    memset(&hmpu->data, 0, sizeof(hmpu->data));

    hmpu->flags.byte = 0;
    hmpu->on_data_ready_cb = on_data_ready;

    hmpu->trigger          = trigger;
    hmpu->dt_div = 125;

    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_BindI2CManager(
    MPU6050_Handle_t *hmpu,
    I2C_ManagerHandle *hmgr,
    I2C_DeviceID dev_id,
    uint8_t priority
) {
    if (!hmpu || !hmgr) {
        return HAL_ERROR;
    }

    hmpu->i2c_mgr = hmgr;
    hmpu->dev_id = dev_id;

    return I2C_Manager_RegisterDevice(
        hmgr,
        dev_id,
        hmpu->i2c_address,
        priority,
        hmpu,
        MPU_I2C_RequestApproved,
        MPU_I2C_TxComplete,
        MPU_I2C_RxComplete,
        MPU_I2C_Error
    );
}

HAL_StatusTypeDef MPU6050_Configure(MPU6050_Handle_t *hmpu) {
    if (!hmpu || !hmpu->i2c_mgr) {
        return HAL_ERROR;
    }

    if (hmpu->configured) {
        return HAL_OK;
    }

    if (hmpu->transfer_state != MPU6050_XFER_IDLE ||
        hmpu->config_state == MPU6050_CONFIG_WHOAMI ||
        hmpu->config_state == MPU6050_CONFIG_PWR ||
        hmpu->config_state == MPU6050_CONFIG_SMPLRT ||
        hmpu->config_state == MPU6050_CONFIG_DLPF ||
        hmpu->config_state == MPU6050_CONFIG_GYRO ||
        hmpu->config_state == MPU6050_CONFIG_ACCEL) {
        return HAL_BUSY;
    }

    hmpu->config_state = MPU6050_CONFIG_WHOAMI;
    return MPU6050_RequestConfigRead(hmpu, MPU6050_REG_WHO_AM_I);
}

void MPU6050_CheckTrigger(MPU6050_Handle_t *hmpu) {
    if (!hmpu->configured) {
        return;
    }

    if (*(hmpu->trigger)) {
        /* Limpio flag */
        *(hmpu->trigger) = false;
        (void)MPU6050_RequestAllData(hmpu);
    }
}

HAL_StatusTypeDef MPU6050_RequestAllData(MPU6050_Handle_t *hmpu) {
    return MPU6050_RequestRead(hmpu, MPU6050_READ_ALL, MPU6050_BURST_REG, 14);
}

HAL_StatusTypeDef MPU6050_RequestAccel(MPU6050_Handle_t *hmpu) {
    return MPU6050_RequestRead(hmpu, MPU6050_READ_ACCEL, MPU6050_BURST_REG, 6);
}

HAL_StatusTypeDef MPU6050_RequestGyro(MPU6050_Handle_t *hmpu) {
    return MPU6050_RequestRead(hmpu, MPU6050_READ_GYRO, MPU6050_REG_GYRO_OUT, 6);
}

HAL_StatusTypeDef MPU6050_RequestTemp(MPU6050_Handle_t *hmpu) {
    return MPU6050_RequestRead(hmpu, MPU6050_READ_TEMP, MPU6050_REG_TEMP_OUT, 2);
}

HAL_StatusTypeDef MPU6050_RequestOrientation(MPU6050_Handle_t *hmpu) {
    return MPU6050_RequestRead(hmpu, MPU6050_READ_ORIENTATION, MPU6050_REG_GYRO_OUT, 6);
}

const MPU6050_IntData_t* MPU6050_GetData(MPU6050_Handle_t *hmpu) {
    return &hmpu->data;
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

void MPU6050_Convert(MPU6050_Handle_t *hmpu) {
    switch (hmpu->last_read_mode) {
        case MPU6050_READ_ALL: {
            int16_t ax = MERGE(0), ay = MERGE(2), az = MERGE(4);
            int16_t t  = MERGE(6), gx = MERGE(8), gy = MERGE(10), gz = MERGE(12);

            hmpu->data.accel_x_mg             = ax * 1000 / 16384;
            hmpu->data.accel_y_mg             = ay * 1000 / 16384;
            hmpu->data.accel_z_mg             = az * 1000 / 16384;
            hmpu->data.temperature_celsius_x100 = t  * 100  / 340 + 3653;
            hmpu->data.gyro_x_mdps            = gx * 1000 / 131;
            hmpu->data.gyro_y_mdps            = gy * 1000 / 131;
            hmpu->data.gyro_z_mdps            = gz * 1000 / 131;
            break;
        }
        case MPU6050_READ_ACCEL: {
            int16_t ax = MERGE(0), ay = MERGE(2), az = MERGE(4);

            hmpu->data.accel_x_mg             = ax * 1000 / 16384;
            hmpu->data.accel_y_mg             = ay * 1000 / 16384;
            hmpu->data.accel_z_mg             = az * 1000 / 16384;
            return;
        }
        case MPU6050_READ_TEMP: {
            int16_t t  = MERGE(0);

            hmpu->data.temperature_celsius_x100 = t  * 100  / 340 + 3653;
            return;
        }
        case MPU6050_READ_GYRO:
        case MPU6050_READ_ORIENTATION: {
            int16_t gx = MERGE(0), gy = MERGE(2), gz = MERGE(4);

            hmpu->data.gyro_x_mdps            = gx * 1000 / 131;
            hmpu->data.gyro_y_mdps            = gy * 1000 / 131;
            hmpu->data.gyro_z_mdps            = gz * 1000 / 131;
            break;
        }
        default:
            return;
    }

    // Integrar para obtener ángulo (mdeg), sin perder precision:
    // v = velocidad*dt(ms) + resto_prev
    int32_t v_x = (hmpu->data.gyro_x_mdps - hmpu->gyro_bias_x) * MPU_DT_MS + hmpu->rem_x;
    int32_t v_y = (hmpu->data.gyro_y_mdps - hmpu->gyro_bias_y) * MPU_DT_MS + hmpu->rem_y;
    int32_t v_z = (hmpu->data.gyro_z_mdps - hmpu->gyro_bias_z) * MPU_DT_MS + hmpu->rem_z;

    // delta de ángulo (mdeg) y nuevo resto
    int32_t d_x = v_x / 1000;
    int32_t d_y = v_y / 1000;
    int32_t d_z = v_z / 1000;

    hmpu->rem_x = v_x % 1000;
    hmpu->rem_y = v_y % 1000;
    hmpu->rem_z = v_z % 1000;

    // acumular el ángulo en milidegrados
    hmpu->angle_x_md += d_x;
    hmpu->angle_y_md += d_y;
    hmpu->angle_z_md += d_z;
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
    (void)HAL_I2C_Master_Transmit_DMA(
        hmpu->hi2c,
        (hmpu->i2c_address << 1),
        &hmpu->tx_buffer[0],
        hmpu->tx_len
    );
    SET_FLAG(hmpu->flags, TX_SENT);
}

static void MPU_I2C_TxComplete(void *ctx)
{
    MPU6050_Handle_t *hmpu = (MPU6050_Handle_t *)ctx;
    if (!hmpu) {
        return;
    }

    if (hmpu->transfer_state != MPU6050_XFER_TX_REG) {
        return;
    }

    if (hmpu->expected_rx_len == 0) {
        hmpu->transfer_state = MPU6050_XFER_IDLE;
        CLEAR_FLAG(hmpu->flags, TX_SENT);
        (void)I2C_Manager_ReleaseBus(hmpu->i2c_mgr, hmpu->dev_id);
        if (hmpu->operation == MPU6050_OP_CONFIG) {
            MPU6050_AdvanceConfig(hmpu);
        }
        return;
    }

    hmpu->transfer_state = MPU6050_XFER_RX_DATA;
    (void)HAL_I2C_Master_Receive_DMA(
        hmpu->hi2c,
        (hmpu->i2c_address << 1),
        hmpu->rx_buffer,
        hmpu->expected_rx_len
    );
    CLEAR_FLAG(hmpu->flags, TX_SENT);
}

static void MPU_I2C_RxComplete(void *ctx)
{
    MPU6050_Handle_t *hmpu = (MPU6050_Handle_t *)ctx;
    if (!hmpu) {
        return;
    }

    if (hmpu->transfer_state != MPU6050_XFER_RX_DATA) {
        return;
    }

    hmpu->transfer_state = MPU6050_XFER_IDLE;

    (void)I2C_Manager_ReleaseBus(hmpu->i2c_mgr, hmpu->dev_id);

    if (hmpu->operation == MPU6050_OP_CONFIG) {
        if (hmpu->config_state == MPU6050_CONFIG_WHOAMI) {
            uint8_t whoami = hmpu->rx_buffer[0];
            if (whoami != hmpu->i2c_address) {
                hmpu->config_state = MPU6050_CONFIG_ERROR;
                hmpu->operation = MPU6050_OP_IDLE;
                return;
            }
        }
        MPU6050_AdvanceConfig(hmpu);
        return;
    }

    hmpu->last_read_mode = hmpu->read_mode;
    hmpu->operation = MPU6050_OP_IDLE;
    SET_FLAG(hmpu->flags, DATA_READY);
}

static void MPU_I2C_Error(void *ctx, I2C_ManagerError err)
{
    (void)err;
    MPU6050_Handle_t *hmpu = (MPU6050_Handle_t *)ctx;
    if (!hmpu) {
        return;
    }

    hmpu->transfer_state = MPU6050_XFER_IDLE;
    hmpu->last_read_mode = hmpu->read_mode;
    CLEAR_FLAG(hmpu->flags, TX_SENT);
    memset(hmpu->rx_buffer, 0, hmpu->expected_rx_len);
    if (hmpu->operation == MPU6050_OP_CONFIG) {
        hmpu->config_state = MPU6050_CONFIG_ERROR;
        hmpu->operation = MPU6050_OP_IDLE;
        return;
    }
    hmpu->operation = MPU6050_OP_IDLE;
    SET_FLAG(hmpu->flags, DATA_READY);
}

/** Función para leer WHO_AM_I directamente */
HAL_StatusTypeDef MPU6050_ReadWhoAmI(MPU6050_Handle_t *hmpu, uint8_t *whoami) {
    return HAL_I2C_Mem_Read(
        hmpu->hi2c,
        (hmpu->i2c_address << 1),
        MPU6050_REG_WHO_AM_I,
        I2C_MEMADD_SIZE_8BIT,
        whoami, 1,
        HAL_MAX_DELAY
    );
}

void MPU6050_CalibrateGyro(MPU6050_Handle_t *h, uint16_t samples) {
    // 1) Reiniciar acumuladores y contador
    h->calib_sum_x  = 0;
    h->calib_sum_y  = 0;
    h->calib_sum_z  = 0;
    h->calib_count  = 0;
    h->calib_target = samples;
    CLEAR_FLAG(h->flags, CALIBRATED);

    // 2) Disparar primer trigger (desde el init o aquí)
    if (!h->configured) {
        h->calib_pending = true;
        return;
    }
    *(h->trigger) = true;
    __NOP();
}
