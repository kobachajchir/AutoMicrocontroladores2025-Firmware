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
#define MPU_DT_MS 8
#define MERGE(i) ((int16_t)(hmpu->rx_buffer[i] << 8 | hmpu->rx_buffer[i+1]))

/* -------------------------------------------------------------------------
 * API pública:
 * -------------------------------------------------------------------------*/

HAL_StatusTypeDef MPU6050_Init(
    MPU6050_Handle_t *hmpu,
    uint8_t      id,
    uint8_t           address,
    I2C_HandleTypeDef *hi2c,
    volatile uint8_t *bus_busy_flag,
    MPU6050_Callback  on_data_ready,
	I2C_Request_Bus_Use      req_cb,
	I2C_Release_Bus_Use     rel_cb,
    volatile bool    *trigger    // <-- debe estar aquí
) {
    if (!hmpu || !hi2c || !bus_busy_flag || !trigger) {
        return HAL_ERROR;
    }
    /* Rellenar handle */
    hmpu->hi2c             = hi2c;
    hmpu->dev_id           = id;
    hmpu->i2c_address      = address;
    hmpu->busy_flag     	= bus_busy_flag;
    hmpu->tx_buffer[0]     = MPU6050_BURST_REG;
    hmpu->expected_rx_len  = sizeof(hmpu->rx_buffer);
    memset(&hmpu->data, 0, sizeof(hmpu->data));

    hmpu->flags.byte = 0;
    hmpu->on_data_ready_cb = on_data_ready;

    hmpu->request_cb    = req_cb;
    hmpu->release_cb    = rel_cb;
    hmpu->trigger          = trigger;
    hmpu->dt_div = 125;

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
    if (tmp != hmpu->i2c_address) {
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
        if(hmpu->request_cb){
        	hmpu->request_cb(I2C_REQ_TYPE_TX_RX);
        }
    }
}

const MPU6050_IntData_t* MPU6050_GetData(MPU6050_Handle_t *hmpu) {
    return &hmpu->data;
}

void MPU6050_Convert(MPU6050_Handle_t *hmpu) {
    // 1) Extraer lecturas raw
    int16_t ax = MERGE(0), ay = MERGE(2), az = MERGE(4);
    int16_t t  = MERGE(6), gx = MERGE(8), gy = MERGE(10), gz = MERGE(12);

    // 2) Convertir a unidades humanas
    hmpu->data.accel_x_mg             = ax * 1000 / 16384;
    hmpu->data.accel_y_mg             = ay * 1000 / 16384;
    hmpu->data.accel_z_mg             = az * 1000 / 16384;
    hmpu->data.temperature_celsius_x100 = t  * 100  / 340 + 3653;
    hmpu->data.gyro_x_mdps            = gx * 1000 / 131;
    hmpu->data.gyro_y_mdps            = gy * 1000 / 131;
    hmpu->data.gyro_z_mdps            = gz * 1000 / 131;

    // 3) Integrar para obtener ángulo (mdeg), sin perder precision:
    //    v = velocidad*dt(ms) + resto_prev
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

/** TX DMA complete (canal 6) – llamado desde el wrapper en main */
void MPU_DMA_CompleteCallback(MPU6050_Handle_t *hmpu, uint8_t is_tx) {
    //(void)unused;
    __NOP(); //TX DMA completado
    /* Liberamos bus TX */
    if(is_tx){
    	//Termino envio de request de datos
    	HAL_I2C_Master_Receive_DMA(
			hmpu->hi2c,
			(hmpu->i2c_address << 1),
			hmpu->rx_buffer,
			hmpu->expected_rx_len
		);
    	CLEAR_FLAG(hmpu->flags, TX_SENT);
    }else{
    	//Termino recepcion
    	if(hmpu->release_cb){
    		hmpu->release_cb();
    	}
    	SET_FLAG(hmpu->flags, DATA_READY);
    	__NOP();
    }
}

void MPU_GrantAccessCallback(MPU6050_Handle_t *hmpu) {
    __NOP();  // <— BREAKPOINT #3: TX bus granted
    HAL_I2C_Master_Transmit_DMA(
        hmpu->hi2c,
        (hmpu->i2c_address << 1),
        &hmpu->tx_buffer[0],
        1
    );
    SET_FLAG(hmpu->flags, TX_SENT);
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
    *(h->trigger) = true;
    __NOP();
}



