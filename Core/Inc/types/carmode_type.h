/*
 * carmode_type.h
 *
 *  Created on: Jun 3, 2025
 *      Author: kobac
 */

#ifndef INC_TYPES_CARMODE_TYPE_H_
#define INC_TYPES_CARMODE_TYPE_H_

typedef enum{
    IDLE_MODE = 0,
    FOLLOW_MODE = 1,
    TEST_MODE = 2
} CarMode_t;

#define CAR_MODE_MAX 3

#endif /* INC_TYPES_CARMODE_TYPE_H_ */
