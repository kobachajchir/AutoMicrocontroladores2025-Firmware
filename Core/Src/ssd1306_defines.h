#ifndef SSD1306_DEFINES_H_
#define SSD1306_DEFINES_H_

#define SSD1306_I2C_PORT      hi2c1
#define SSD1306_ADDRESS       0x3C

#define SSD1306_128X64
#define SSD1306_USE_DMA       1

// CON I2C MANAGER: NO USAR REFRESH CONTINUO
#define SSD1306_CONTUPDATE    0   // <- antes 1

// Habilita integración con I2C Manager
#define SSD1306_USE_I2C_MANAGER  1

#endif /* SSD1306_DEFINES_H_ */
