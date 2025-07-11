#include "stdint.h"


#ifndef Fonts
#define Fonts

//
//	Structure om font te definieren
//
typedef struct {
	const uint8_t FontWidth;    /*!< Font width in pixels */
	uint8_t FontHeight;   /*!< Font height in pixels */
	const uint16_t *data; /*!< Pointer to data font data array */
} FontDef;

#define Font5x10_WIDTH  5
#define Font5x10_HEIGHT 10
#define Font6x12_WIDTH  6
#define Font6x12_HEIGHT 12
#define Font14x17_WIDTH  14
#define Font14x17_HEIGHT 17
#define Font16x29_WIDTH  16
#define Font16x29_HEIGHT 29

#define LOCK_ICON_X   56
#define LOCK_ICON_Y    8
#define LOCK_ICON_W    13
#define LOCK_ICON_H    16

extern const uint8_t FontMap[128];
extern const FontDef Font_5x10_Min;
extern const FontDef Font_6x12_Min;
extern const FontDef Font_14x17_Min;
extern const FontDef Font_16x29_Min;
extern const uint8_t Icon_UserBtn_bits[];
extern const uint8_t Icon_Wifi_bits[];
extern const uint8_t Icon_Sensors_bits[];
extern const uint8_t Icon_Cursor_bits[];
extern const uint8_t Icon_Config_bits[];
extern const uint8_t Icon_Encoder_bits[];
extern const uint8_t Icon_Lock_bits[];
extern const uint8_t Icon_Volver_bits[];
extern const uint8_t Icon_APWifi_bits[];
extern const uint8_t Icon_USB_bits[];
extern const uint8_t Icon_RF_bits[];
extern const uint8_t Icon_Car_bits[];

#endif

