// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "user_button.h"
#include "encoder.h"
#include "types/led_status.h"
#include "types/carmode_type.h"
#include "types/usart_buffer_type.h"
#include "utils/macros_utils.h"
#include "tcrt5000.h"
#include "motor_control.h"
#include "i2c_manager.h"
#include "menusystem.h"
#include "oled_handle.h"
#include "mpu6050.h"
#include "types/IPStruct_t.h"

// =============================================
// LED de Estado (conectado a PC13 a traves de un BJT NPN)
// =============================================

//Defines de tiempos
//#define COUNT_IRDATA_TENMS 13
#define TICKS_250US_IN_3S      12000U
#define TICKS_250US_IN_5S      20000U

//Variables flag globales
#define INIT_CAR BIT0_MASK
#define PROCESS_IR_DATA BIT1_MASK
#define TCRT_CALIB_BLACK_AND_WHITE_COMPLETE BIT2_MASK
#define TCRT_CALIB_OBSTACLES_COMPLETE BIT3_MASK
#define MPU_GET_DATA BIT4_MASK
#define OLED_READY BIT5_MASK
#define OLED_TENMS_PASSED BIT6_MASK
#define OLED_REFRESH BIT7_MASK
//SYS FLAG 2
#define OLED_ACTIVE BIT0_MASK
#define WIFI_ACTIVE BIT1_MASK
#define AP_ACTIVE BIT2_MASK
#define USB_ACTIVE BIT3_MASK
#define RF_ACTIVE BIT4_MASK
#define MODIFYING_CARMODE BIT5_MASK
#define ESP_PRESENT BIT6_MASK
#define SHOWSECONDSCREEN BIT7_MASK
//SYS FLAG 3
#define CHK_ESP_CONN BIT0_MASK
#define WIFI_SEARCHING BIT1_MASK

/* Estados de pantalla de bloqueo/PIN */
#define LOCK_STATE_LOCKED           0u
#define LOCK_STATE_PIN_INCORRECT    1u
#define LOCK_STATE_PIN_MODIFIED     2u
#define LOCK_STATE_ENTER_PIN        3u

//Definicion de tamanios
#define USART1_BUFFER_SIZE 64
/* Debe cubrir frames UNER largos: payload 255 + overhead 10, con margen para rafagas. */
#define USART1_RX_DMA_BUF_LEN 512

#define I2C_ADDR_OLED  0x3C
#define I2C_ADDR_MPU   0x68  // 104 decimal

#define INSIDE_MENU (inside_menu_flag)

//Defines para el I2C_DeviceEntry
#define I2C_REQ_PENDING BIT0_MASK
#define I2C_REQ_IS_TX   BIT1_MASK
#define I2C_REQ_IS_RX   BIT2_MASK

#define OLED_ACTIVE_TIME 500

#define OLED_BAR_COUNT 8
#define BAR_WIDTH 12
#define Y_OFFSET    5    // margen superior
#define Y_SPACING   22   // separación vertical entre ítems
#define ITEM_HEIGHT 21   // altura del recuadro de selección

#define MENU_VISIBLE_ITEMS    3
#define MENU_ITEM_Y0          4
#define MENU_ITEM_SPACING     21
#define CURSOR_X              0
#define CURSOR_WIDTH          8
#define CURSOR_HEIGHT         16

// Ajusta estos valores si tus iconos son de otro tamaño
#define ICON_X                8
#define ICON_WIDTH            16
#define ICON_HEIGHT           16
// Offset vertical para centrar el icono en la "línea" del item
#define ICON_Y_OFFSET         -13

#define WIFIDEFAULTSEARCHTIMEOUT 1000
#define WIFI_SCAN_MAX_NETWORKS 8
#define WIFI_SSID_MAX_LEN 32
#define WIFI_RESULTS_MENU_MAX_ITEMS (WIFI_SCAN_MAX_NETWORKS + 2)

#define UNER_SCREEN_PAGE_DIR_UP             0x00u
#define UNER_SCREEN_PAGE_DIR_DOWN           0x01u

#define UNER_PRESS_KIND_SHORT               0x00u
#define UNER_PRESS_KIND_LONG                0x01u

typedef struct {
    IPStruct_t staIp;
    IPStruct_t apIp;
    char staSsid[WIFI_SSID_MAX_LEN + 1u];
    uint8_t staIpValid;
    uint8_t apIpValid;
} ESPWiFiConnectionInfo_t;


// =============================
// Variables globales (extern)
// =============================
extern volatile bool  procesar_flag;
extern volatile bool  lanzar_ADC_trigger_flag;
extern volatile bool  mpu_trigger;

/* Estas dos estaban declaradas pero no aparecen en tus defs actuales.
 * Mantenidas como extern para no romper otros módulos.
 * Asegurate de definirlas en algún .c si realmente se usan.
 */
extern volatile uint16_t tim3_overflow_count;
extern volatile uint32_t contador;

extern volatile LedStatus_t ledStatus;

/* OJO: 'carMode' estaba declarado pero no lo definiste en app_globals.c.
 * Si no lo usás, eliminá este extern. Si lo usás, definilo en app_globals.c.
 */
extern volatile CarMode_t carMode;

/* Flags byte (estaban causando los "multiple definition") */
extern volatile Byte_Flag_Struct systemFlags;
extern volatile Byte_Flag_Struct systemFlags2;
extern volatile Byte_Flag_Struct systemFlags3;
extern volatile Byte_Flag_Struct carModeFlags;

extern volatile uint16_t sensor_raw_data[TCRT5000_NUM_SENSORS];

/* FIX: myLight -> tcrtLight (unificar nombre real) */
extern TCRT_LightConfig_t tcrtLight;

extern bool pull_cfg[TCRT5000_NUM_SENSORS];

extern volatile uint8_t cnt_adc_trigger;
extern volatile uint16_t cnt_250us_MPU;
extern volatile uint16_t cnt_10ms;
extern volatile uint32_t cnt_10us;
extern volatile uint32_t tcrt_calib_cnt_phase;  // contador de 10 µs para la fase actual
extern volatile uint8_t oled10msCounter;

extern volatile uint8_t inside_menu_flag;
extern volatile uint8_t encoder_fast_scroll_enabled;
extern volatile uint16_t encoderValue;

extern volatile uint8_t motorSelected; // 0: izquierdo, 1: derecho, 2: ambos
extern volatile uint8_t motorSpeed;
extern volatile uint8_t motorDir; // 0: adelante, 1: atrás

extern uint16_t wifiSearchingTimeout;
extern uint8_t networksFound;
extern char wifiNetworkSsids[WIFI_SCAN_MAX_NETWORKS][WIFI_SSID_MAX_LEN + 1];
extern volatile uint8_t wifiScanSessionActive;
extern volatile uint8_t wifiScanResultsPending;
extern volatile IPStruct_t espStaIp;
extern volatile IPStruct_t espApIp;
extern ESPWiFiConnectionInfo_t espWifiConnection;
extern char espFirmwareVersion[33];

/* --- Handlers de librerias --- */
extern USART_Buffer_t usart1Buf;
extern TCRTHandlerTask tcrtTask;
extern MotorControl_Handle motorTask;
extern I2C_ManagerHandle i2cManager;
extern volatile bool oled_first_draw;
extern MPU6050_Handle_t mpuTask;
extern UserButton_Handle_t btnUser;
extern ENC_Handle_t encoder;
extern CarMode_t auxCarMode;

/* DMA RX USART1 */
extern volatile uint8_t usart1_rx_dma_buf[USART1_RX_DMA_BUF_LEN];
extern volatile uint8_t usart1_tx_busy;
extern volatile uint8_t esp_firmware_received_flag;
extern volatile uint8_t uner_uart1_rx_hint;
extern volatile uint8_t uner_uart1_rx_last_tick;

extern volatile uint8_t espBootRebootPending;
extern volatile uint8_t espBootRebootPendingResponse;

/* Menu system */
extern MenuSystem    menuSystem;
extern SubMenu       mainMenu, submenu1, submenu2, submenu3;
extern SubMenu       wifiResultsMenu;
extern MenuItem      mainMenuItems[], submenu1Items[], submenu2Items[], submenu3Items[];
extern MenuItem      wifiResultsItems[WIFI_RESULTS_MENU_MAX_ITEMS];
extern const uint8_t* generic_icon;
extern const RenderScreenFunction mainMenuScreen;
extern const RenderScreenFunction subMenuScreen;
extern const RenderScreenFunction valoresIRScreen;
extern const RenderScreenFunction wifiDataScreen;

/* OLED global handle */
extern OledHandle    oledHandle;

extern void USART1_DMA_CheckRx(void);

#endif // GLOBALS_H

