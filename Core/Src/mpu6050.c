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
#define MPU6050_BURST_REG      0x3B

/* -------------------------------------------------------------------------
 * API pública:
 * -------------------------------------------------------------------------*/

HAL_StatusTypeDef MPU6050_Init(
    MPU6050_Handle_t *hmpu,
    I2C_DeviceID      id,
    uint8_t           address,
    I2C_HandleTypeDef *hi2c,
    volatile uint8_t *tx_busy,
    volatile uint8_t *rx_busy,
    MPU6050_Callback  on_data_ready,
    I2C_Callback      req_tx_cb,
    I2C_Callback      rel_tx_cb,
    I2C_Callback      req_rx_cb,
    I2C_Callback      rel_rx_cb,
    volatile bool    *trigger
) {
    if (!hmpu || !hi2c || !tx_busy || !rx_busy || !trigger) {
        return HAL_ERROR;
    }
    /* Rellenar handle */
    hmpu->hi2c             = hi2c;
    hmpu->dev_id           = id;
    hmpu->i2c_address      = address;
    hmpu->tx_busy_flag     = tx_busy;
    hmpu->rx_busy_flag     = rx_busy;
    hmpu->tx_buffer[0]     = MPU6050_BURST_REG;
    hmpu->expected_rx_len  = sizeof(hmpu->rx_buffer);
    hmpu->data_ready       = 0;
    memset(&hmpu->data, 0, sizeof(hmpu->data));

    hmpu->on_data_ready_cb = on_data_ready;

    hmpu->request_tx_cb    = req_tx_cb;
    hmpu->release_tx_cb    = rel_tx_cb;
    hmpu->request_rx_cb    = req_rx_cb;
    hmpu->release_rx_cb    = rel_rx_cb;
    hmpu->trigger          = trigger;

    /* Registramos internamente nuestros callbacks en I2C Manager */
    /* (esto ya lo hizo el usuario externamente al llamar I2C_Manager_RegisterDevice,
       aquí solo indicamos al manager quién es el dispositivo activo) */

    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_Configure(MPU6050_Handle_t *hmpu) {
    uint8_t tmp;
    /* WHO_AM_I */
    if (HAL_I2C_Mem_Read(hmpu->hi2c,
                        (hmpu->i2c_address << 1),
                        MPU6050_REG_WHO_AM_I, 1,
                        &tmp, 1, 1000) != HAL_OK) {
        return HAL_ERROR;
    }
    if (tmp != 0x68) {
        return HAL_ERROR;
    }
    /* Saca de sleep, sample rate, filtros, fullscale gyro/accel */
    tmp = 0;
    HAL_I2C_Mem_Write(hmpu->hi2c, (hmpu->i2c_address << 1),
                      MPU6050_REG_PWR_MGMT_1, 1, &tmp, 1, 1000);
    tmp = 7;
    HAL_I2C_Mem_Write(hmpu->hi2c, (hmpu->i2c_address << 1),
                      MPU6050_REG_SMPLRT_DIV, 1, &tmp, 1, 1000);
    tmp = 3;
    HAL_I2C_Mem_Write(hmpu->hi2c, (hmpu->i2c_address << 1),
                      MPU6050_REG_CONFIG, 1, &tmp, 1, 1000);
    tmp = 0;
    HAL_I2C_Mem_Write(hmpu->hi2c, (hmpu->i2c_address << 1),
                      MPU6050_REG_GYRO_CFG, 1, &tmp, 1, 1000);
    HAL_I2C_Mem_Write(hmpu->hi2c, (hmpu->i2c_address << 1),
                      MPU6050_REG_ACCEL_CFG, 1, &tmp, 1, 1000);
    return HAL_OK;
}

void MPU6050_CheckTrigger(MPU6050_Handle_t *hmpu) {
    if (*(hmpu->trigger)) {
        /* Limpio flag */
        *(hmpu->trigger) = false;
        /* Pido TX bus */
        if(hmpu->request_tx_cb){
        	__NOP();
        	hmpu->request_tx_cb();
        }
    }
}

const MPU6050_IntData_t* MPU6050_GetData(MPU6050_Handle_t *hmpu) {
    return &hmpu->data;
}

void MPU6050_Convert(MPU6050_Handle_t *hmpu) {
    #define MERGE(i) ((int16_t)(hmpu->rx_buffer[i] << 8 | hmpu->rx_buffer[i+1]))
    int16_t ax = MERGE(0), ay = MERGE(2), az = MERGE(4);
    int16_t t  = MERGE(6), gx = MERGE(8), gy = MERGE(10), gz = MERGE(12);
    hmpu->data.accel_x_mg             = ax * 1000 / 16384;
    hmpu->data.accel_y_mg             = ay * 1000 / 16384;
    hmpu->data.accel_z_mg             = az * 1000 / 16384;
    hmpu->data.temperature_celsius_x100 = t  * 100 / 340 + 3653;
    hmpu->data.gyro_x_mdps            = gx * 1000 / 131;
    hmpu->data.gyro_y_mdps            = gy * 1000 / 131;
    hmpu->data.gyro_z_mdps            = gz * 1000 / 131;
}

/** TX DMA complete (canal 6) – llamado desde el wrapper en main */
void MPU_DMA_CompleteCallback(MPU6050_Handle_t *hmpu) {
    //(void)unused;
    __NOP(); //TX DMA completado
    /* Liberamos bus TX */
    hmpu->release_tx_cb();
}

/** RX DMA complete (canal 7) – llamado desde el wrapper en main */
void MPU_DMARX_CompleteCallback(MPU6050_Handle_t *hmpu) {
    //(void)unused;
    /* 4.1) Marcar RX libre */
    __NOP(); //RX DMA completado
	//hmpu->rx_busy_flag = 0;
    hmpu->release_rx_cb();
    /* 4.2) Bajar trigger si quieres reactivar tras lectura */
    /* 4.3) Datos listos */
	hmpu->data_ready = 1;
    /* 4.4) Convertir */
    MPU6050_Convert(hmpu);
    /* 4.5) Callback usuario */
    if (hmpu->on_data_ready_cb) {
    	hmpu->on_data_ready_cb();
    }
    /* 4.6) Liberar bus RX */
}
// 3) En el wrapper de “grant TX” (cuando el manager concede el bus):
void MPU_GrantAccessCallback(MPU6050_Handle_t *hmpu) {
    __NOP();  // <— BREAKPOINT #3: TX bus granted
    /*HAL_I2C_Mem_Write_DMA(
        hmpu->hi2c,
        (hmpu->i2c_address << 1),    // dirección I²C del dispositivo
        hmpu->tx_buffer[0],          // registro de memoria (0x3B)
        I2C_MEMADD_SIZE_8BIT,        // tamaño del puntero de registro (8-bit)
        NULL,                        // puntero a datos a escribir (ninguno)
        0                            // longitud de datos (0)
    );*/
    HAL_I2C_Master_Transmit_DMA(
        hmpu->hi2c,
        (hmpu->i2c_address << 1),
        &hmpu->tx_buffer[0],
        1
    );
    /* Pedimos bus RX */
    hmpu->request_rx_cb();
}

void MPU_GrantAccessRXCallback(MPU6050_Handle_t *hmpu) {
    __NOP();  // <— BREAKPOINT #3: RX bus granted
    //Nada aun
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


