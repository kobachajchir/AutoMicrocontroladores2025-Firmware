/*
 * uner_app.h - UNER v2 application wiring
 */
#ifndef INC_UNER_APP_H_
#define INC_UNER_APP_H_

#include <stdint.h>
#include "uner_core.h"

#ifdef __cplusplus
extern "C" {
#endif

void UNER_App_Init(void);
void UNER_App_Poll(void);
UNER_Status UNER_App_SendCommand(uint8_t cmd, const uint8_t *payload, uint8_t len);
void UNER_App_OnUart1TxComplete(void);
void UNER_App_NotifyUart1Rx(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_UNER_APP_H_ */
