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

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
    struct {
        uint8_t bit0: 1;  ///< Bit 0  (parte baja)
        uint8_t bit1: 1;  ///< Bit 1  (parte baja)
        uint8_t bit2: 1;  ///< Bit 2  (parte baja)
        uint8_t bit3: 1;  ///< Bit 3  (parte baja)
        uint8_t bit4: 1;  ///< Bit 4  (parte alta)
        uint8_t bit5: 1;  ///< Bit 5  (parte alta)
        uint8_t bit6: 1;  ///< Bit 6  (parte alta)
        uint8_t bit7: 1;  ///< Bit 7  (parte alta)
    } bitmap;            ///< Acceso individual a cada bit
    struct {
        uint8_t bitPairL: 2; ///< Bit pair bajo  (bits 0–1)
        uint8_t bitPairCL: 2; ///< Bit pair centro bajo (bits 2–3)
        uint8_t bitPairCH: 2; ///< Bit pair centro alto  (bits 4–5)
        uint8_t bitPairH: 2; ///< Bit pair alto  (bits 6–7)
    } bitPair;
    struct {
        uint8_t bitL: 4; ///< Nibble bajo  (bits 0–3)
        uint8_t bitH: 4; ///< Nibble alto  (bits 4–7)
    } nibbles;           ///< Acceso a cada nibble
    uint8_t byte;        ///< Acceso completo a los 8 bits (0–255)
} MPU_Byte_Flag_Struct_t;

#define BIT0_MASK   0x01  // 0000 0001
#define BIT1_MASK   0x02  // 0000 0010
#define BIT2_MASK   0x04  // 0000 0100
#define BIT3_MASK   0x08  // 0000 1000
#define BIT4_MASK   0x10  // 0001 0000
#define BIT5_MASK   0x20  // 0010 0000
#define BIT6_MASK   0x40  // 0100 0000
#define BIT7_MASK   0x80  // 1000 0000

// --- Pares de bits (si alguna vez los necesitas) ---
#define BITS01_MASK 0x03  // 0000 0011
#define BITS23_MASK 0x0C  // 0000 1100
#define BITS45_MASK 0x30  // 0011 0000
#define BITS67_MASK 0xC0  // 1100 0000

// --- Nibbles ---
#define NIBBLE_L_MASK 0x0F  // 0000 1111 (bits 0..3)
#define NIBBLE_H_MASK 0xF0  // 1111 0000 (bits 4..7)

#define SET_FLAG(flag_struct, BIT_MASK)    ((flag_struct).byte |=  (uint8_t)(BIT_MASK))
#define CLEAR_FLAG(flag_struct, BIT_MASK)  ((flag_struct).byte &= (uint8_t)(~(BIT_MASK)))
#define TOGGLE_FLAG(flag_struct, BIT_MASK) ((flag_struct).byte ^=  (uint8_t)(BIT_MASK))
#define IS_FLAG_SET(flag_struct, BIT_MASK) (((flag_struct).byte & (BIT_MASK)) != 0U)
#define NIBBLEL_SET_STATE(object, state)  \
    do { \
        (object).byte = (uint8_t)(((object).byte & NIBBLE_H_MASK) | ((uint8_t)((state) & NIBBLE_L_MASK))); \
    } while (0)
#define NIBBLEH_SET_STATE(object, state)  \
    do { \
        (object).byte = (uint8_t)(((object).byte & NIBBLE_L_MASK) | ((uint8_t)(((state) & NIBBLE_L_MASK) << 4))); \
    } while (0)
#define NIBBLEH_GET_STATE(object)  (uint8_t)(((object).byte & NIBBLE_H_MASK) >> 4)
#define NIBBLEL_GET_STATE(object)  (uint8_t)((object).byte & NIBBLE_L_MASK)

//Defines para el i2cDeviceType
#define DATA_READY BIT0_MASK
#define TX_SENT  BIT1_MASK
#define CALIBRATED  BIT2_MASK
//#define UNUSED  BIT3_MASK*/
//Otra parte (Alta) guarda la prioridad

/** Callback que se ejecuta cuando los datos del MPU6050 están listos. */
typedef void (*MPU6050_Callback)(void);
typedef void (*I2C_Request_Bus_Use)(uint8_t req_is_tx);
typedef void (*I2C_Release_Bus_Use)(void);
typedef void (*I2C_RequestApprovedCb)(void);
typedef void (*I2C_TransferCompleteCb)(uint8_t is_tx);

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
    uint8_t       dev_id;          ///< ID lógico en I2C_Manager
    uint8_t            i2c_address;     ///< Dirección 7-bit (0x68)

    volatile uint8_t  *busy_flag;

    uint8_t            tx_buffer[1];    ///< {0x3B}
    uint8_t            rx_buffer[14];   ///< Raw data
    uint8_t            expected_rx_len; ///< 14

    MPU_Byte_Flag_Struct_t flags;
    MPU6050_IntData_t  data;            ///< Datos convertidos

    /** Usuario callback */
    MPU6050_Callback   on_data_ready_cb;

    /** Callbacks para el arbitraje I2C */
    I2C_Request_Bus_Use      request_cb;   ///< Wrapper: I2C_Manager_RequestAccessRX
    I2C_Release_Bus_Use      release_cb;   ///< Wrapper: I2C_Manager_ReleaseBusRX

    uint16_t dt_div;

    volatile bool     *trigger;         ///< Señal externa de disparo
    int32_t gyro_bias_x, gyro_bias_y, gyro_bias_z;
	int32_t calib_sum_x,  calib_sum_y,  calib_sum_z;
    int32_t angle_x_md,  angle_y_md,  angle_z_md;
    int32_t rem_x, rem_y, rem_z;
	uint16_t calib_count,   calib_target;
} MPU6050_Handle_t;

/**
 * @brief Inicializa el handle del MPU6050 sin tocar el bus.
 */
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
);


/**
 * @brief Configura el MPU: WHO_AM_I, wakeup, 1 kHz, DLPF=3, ±250 °/s, ±2g
 */
HAL_StatusTypeDef MPU6050_Configure(MPU6050_Handle_t *hmpu);

void MPU6050_CalibrateGyro(MPU6050_Handle_t *h, uint16_t samples);

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

void MPU_DMA_CompleteCallback(MPU6050_Handle_t *hmpu, uint8_t is_tx);

void MPU_GrantAccessCallback(MPU6050_Handle_t *hmpu);

#ifdef __cplusplus
}
#endif

#endif /* INC_MPU6050_H_ */

