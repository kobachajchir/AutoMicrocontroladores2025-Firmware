/* mpu6050.h
 * Created on: Jun 18, 2025
 *     Author: kobac
 */

#ifndef INC_MPU6050_H_
#define INC_MPU6050_H_

#include "stm32f1xx_hal.h"
#include "i2c_manager.h"
#include <stdint.h>
#include <stdbool.h>

/** Callback que se ejecuta cuando los datos del MPU6050 están listos. */
typedef void (*MPU6050_Callback)(void);

/** Datos procesados en enteros. */
typedef struct {
    int16_t accel_x_mg;
    int16_t accel_y_mg;
    int16_t accel_z_mg;
    int16_t temperature_celsius_x100;
    int16_t gyro_x_mdps;
    int16_t gyro_y_mdps;
    int16_t gyro_z_mdps;
} MPU6050_IntData_t;

/** Handle único que el usuario externaliza. */
typedef struct {
    I2C_HandleTypeDef *hi2c;            ///< I2C hardware handle
    I2C_DeviceID       dev_id;          ///< ID lógico en I2C_Manager
    uint8_t            i2c_address;     ///< Dirección 7-bit (0x68)

    volatile uint8_t  *tx_busy_flag;    ///< TX DMA ocupado (externo)
    volatile uint8_t  *rx_busy_flag;    ///< RX DMA ocupado (externo)

    uint8_t            tx_buffer[1];    ///< {0x3B}
    uint8_t            rx_buffer[14];   ///< Raw data
    uint8_t            expected_rx_len; ///< 14

    volatile uint8_t   data_ready;      ///< 1=datos nuevos disponibles
    MPU6050_IntData_t  data;            ///< Datos convertidos

    /** Usuario callback */
    MPU6050_Callback   on_data_ready_cb;

    /** Callbacks para el arbitraje TX */
    I2C_Callback       request_tx_cb;   ///< Wrapper: I2C_Manager_RequestAccess
    I2C_Callback       release_tx_cb;   ///< Wrapper: I2C_Manager_ReleaseBus

    /** Callbacks para el arbitraje RX */
    I2C_Callback       request_rx_cb;   ///< Wrapper: I2C_Manager_RequestAccessRX
    I2C_Callback       release_rx_cb;   ///< Wrapper: I2C_Manager_ReleaseBusRX

    volatile bool     *trigger;         ///< Señal externa de disparo
} MPU6050_Handle_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el handle del MPU6050 sin tocar el bus.
 */
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
    volatile bool    *trigger    // <-- debe estar aquí
);


/**
 * @brief Configura el MPU: WHO_AM_I, wakeup, 1 kHz, DLPF=3, ±250 °/s, ±2g
 */
HAL_StatusTypeDef MPU6050_Configure(MPU6050_Handle_t *hmpu);

/**
 * @brief Revisa la señal @p trigger; si está activa, la limpia y pide TX bus.
 */
void MPU6050_CheckTrigger(MPU6050_Handle_t *hmpu);

/**
 * @brief Devuelve el puntero a los datos convertidos (mg, mdps, °C×100).
 */
const MPU6050_IntData_t* MPU6050_GetData(MPU6050_Handle_t *hmpu);

/**
 * @brief Convierte el buffer raw en valores enteros.
 */
void MPU6050_Convert(MPU6050_Handle_t *hmpu);

/**
 * @brief Lee WHO_AM_I (0x75) y devuelve su valor.
 */
HAL_StatusTypeDef MPU6050_ReadWhoAmI(
    MPU6050_Handle_t *hmpu,
    uint8_t          *whoami
);

#ifdef __cplusplus
}
#endif

#endif /* INC_MPU6050_H_ */

