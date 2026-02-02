/*
 * uner_frame.h
 *
 *  Created on: Aug 1, 2025
 *      Author: kobac
 */

#ifndef INC_UNER_FRAME_H_
#define INC_UNER_FRAME_H_

#include "uner_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

UNER_Status UNER_BuildFrame(uint8_t *out_buf,
                            uint16_t out_max,
                            uint8_t src,
                            uint8_t dst,
                            uint8_t cmd,
                            const uint8_t *payload,
                            uint8_t payload_len,
                            uint16_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_FRAME_H_ */
