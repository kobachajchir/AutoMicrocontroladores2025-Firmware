/*
 * usart_buffer_type.h
 *
 *  Created on: Jun 3, 2025
 *      Author: kobac
 */

#ifndef INC_TYPES_USART_BUFFER_TYPE_H_
#define INC_TYPES_USART_BUFFER_TYPE_H_

#include "types/bitmap_type.h"

#define USART1_BUFFER_SIZE 64

typedef struct {
	volatile uint8_t headTx;             ///< Índice 0..63 para la próxima posición a escribir en txBuffer
	volatile uint8_t tailTx;             ///< Índice 0..63 para la próxima posición a leer de txBuffer
	volatile uint8_t headRx;             ///< Índice 0..63 para la próxima posición a escribir en rxBuffer
	volatile uint8_t tailRx;             ///< Índice 0..63 para la próxima posición a leer de rxBuffer
	volatile uint8_t txCount;   ///< número de bytes actualmente encolados en TX
	volatile Byte_Flag_Struct flags;     ///< Byte de flags (8 bits) para estado/error, etc.
    uint8_t txBuffer[USART1_BUFFER_SIZE];    // Buffer circular para TX
    uint8_t rxBuffer[USART1_BUFFER_SIZE];    // Buffer circular para RX
} USART_Buffer_t;

#endif /* INC_TYPES_USART_BUFFER_TYPE_H_ */
