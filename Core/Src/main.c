/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "globals.h"
#include "utils.h"
#include "utils/macros_utils.h"
#include "types/bitmap_type.h"
#include "motor_control.h"
#include "i2c_manager.h"
#include "fonts.h"
#include "menusystem.h"
#include "oled_utils.h"
#include "encoder.h"             // donde se define ENC_Handle_t
#include "user_button.h"
#include "ui_event_router.h"
#include "ssd1306.h"
#include "mpu6050.h"             // donde se define MPU6050_Handle_t
#include "screenWrappers.h"
#include "eventManagers.h"
//#include "oled_screens.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;
DMA_HandleTypeDef hdma_i2c1_rx;
DMA_HandleTypeDef hdma_i2c1_tx;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart1_rx;

PCD_HandleTypeDef hpcd_USB_FS;

/* USER CODE BEGIN PV */

volatile bool procesar_flag = false;
volatile bool lanzar_ADC_trigger_flag = false;
volatile bool mpu_trigger = false;
volatile LedStatus_t ledStatus;
volatile Byte_Flag_Struct systemFlags;
volatile Byte_Flag_Struct systemFlags2;
volatile Byte_Flag_Struct systemFlags3;
volatile Byte_Flag_Struct carModeFlags;
volatile uint16_t sensor_raw_data[ TCRT5000_NUM_SENSORS ];
TCRT_LightConfig_t tcrtLight;
volatile uint8_t cnt_adc_trigger = 0;
volatile uint16_t cnt_250us_MPU = 0;
volatile uint16_t cnt_10ms = 0;
volatile uint32_t cnt_10us = 0;
volatile uint32_t tcrt_calib_cnt_phase = 0;
//Modificcar para hacer una sola struct de velocidades modo y direcciones
volatile uint8_t inside_menu_flag;
volatile uint16_t encoderValue;
volatile uint8_t oled10msCounter = 0;
volatile uint8_t motorSelected = 0; // 0: izquierdo, 1: derecho, 2: ambos
volatile uint8_t motorSpeed    = 100;
volatile uint8_t motorDir      = 0; // 0: adelante, 1: atrás

// Bandera para I2C_Manager (bus ocupado)

bool pull_cfg[ TCRT5000_NUM_SENSORS ] = {
    TCRT_PULL_UP,    // canal 0: línea central  (no invertir)
    TCRT_PULL_UP,    // canal 1: línea lateral izq
    TCRT_PULL_UP,    // canal 2: línea lateral der
    TCRT_PULL_DOWN,  // canal 3: obstáculo diag izq (invertir)
    TCRT_PULL_DOWN,  // canal 4: obstáculo frente izq
    TCRT_PULL_DOWN,  // canal 5: obstáculo centro
    TCRT_PULL_DOWN,  // canal 6: obstáculo frente der
    TCRT_PULL_DOWN   // canal 7: obstáculo diag der
};


// Este es el buffer real que usará el DMA
uint8_t usart1_rx_dma_buf[USART1_RX_DMA_BUF_LEN];
// Posición previa usada para comparar nuevos datos
volatile uint16_t usart1_rx_prev_pos = 0;
volatile uint8_t usart1_feed_pending;
volatile uint8_t usart1_tx_busy;

/* --- Handlers de librerias --- */
USART_Buffer_t usart1Buf;
TCRTHandlerTask tcrtTask;
MotorControl_Handle motorTask;
MPU6050_Handle_t mpuTask;
UserButton_Handle_t btnUser;
ENC_Handle_t encoder;
I2C_ManagerHandle i2cManager;
CarMode_t auxCarMode;
volatile bool oled_first_draw = false;

static bool Oled_WaitReady(I2C_ManagerHandle *manager, uint16_t addr_7bit, uint32_t retries, uint32_t delay_ms)
{
    if (!manager) {
        return false;
    }

    for (uint32_t attempt = 0; attempt < retries; attempt++) {
        if (I2C_Manager_IsAddressReady(manager, addr_7bit) == HAL_OK) {
            __NOP(); // BREAKPOINT: OLED respondió en I2C
            return true;
        }
        HAL_Delay(delay_ms);
    }

    return false;
}

void OledUtils_Clear_Wrapper(){
	OledUtils_Clear();
}

void OLED_Is_Ready(void) {
    if (!IS_FLAG_SET(systemFlags, OLED_READY)) {
        menuSystem.renderFn = OledUtils_RenderDashboard_Wrapper;
        //menuSystem.renderFn = OledUtils_RenderTestScreen_Wrapper;
        menuSystem.renderFlag = true;
        oled_first_draw = true;
        SET_FLAG(systemFlags, OLED_READY);
        __NOP();
    }
}

void setMode_IDLE(void){
	SET_CAR_MODE(IDLE_MODE);
}
void setMode_FOLLOW(void){
	SET_CAR_MODE(FOLLOW_MODE);
}
void setMode_TEST(void){
	SET_CAR_MODE(TEST_MODE);
}
/* --- Variables globales --- */

// Sistema de menú


// submenuESPItems
MenuItem submenuESPItems[] = {
    {"Chk Conexion", NULL, NULL, Icon_Link_bits, NULL},
    {"Firmware",    NULL, NULL, Icon_Info_bits,  NULL},
	{"Reset ESP",    NULL, NULL, Icon_Refrescar_bits,  NULL},
    {"Volver",       MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper}
};


SubMenu submenuESP = {
    .name                = "Enlace ESP",
    .items               = submenuESPItems,
    .itemCount           = sizeof(submenuESPItems)/sizeof(submenuESPItems[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = &submenu1,
    .icon                = NULL
};

MenuSystem menuSystem = {
    .currentMenu      = &mainMenu,
    .clearScreen      = OledUtils_Clear_Wrapper,
    .drawItem         = OledUtils_DrawItem_Wrapper,
    .renderFn         = OledUtils_RenderDashboard_Wrapper,
    .insideMenuFlag   = &inside_menu_flag,
    .renderFlag       = false
};

// Ítems del menú principal
MenuItem mainMenuItems[] = {
    // nombre     acción          submenú        icono             pantalla render
    { "Wifi",      NULL,           &submenu1,     Icon_Wifi_bits,   MenuSys_RenderMenu_Wrapper },
    { "Sensores",  NULL,           &submenu2,     Icon_Sensors_bits, MenuSys_RenderMenu_Wrapper },
    { "Config.",   NULL,           &submenu3,     Icon_Config_bits, MenuSys_RenderMenu_Wrapper },
	// name       action    submenu       icon               screenRenderFn
	{ "Volver",   MenuSys_GoBack_Wrapper,     NULL,         Icon_Volver_bits,  OledUtils_RenderDashboard_Wrapper },
};
// Menú principal
SubMenu mainMenu = {
    .name                = "Principal",
    .items               = mainMenuItems,
    .itemCount           = sizeof(mainMenuItems)/sizeof(mainMenuItems[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = NULL,
    .icon                = NULL
};

// Ítems del submenu 1: MODO (sin pantalla asociada por ahora)
MenuItem submenu1Items[] = {
    {"Info AP",   NULL,       NULL, Icon_Info_bits, NULL},
    {"Buscar APs", NULL,     NULL, Icon_Refrescar_bits, NULL},
    {"Conexion ESP",   NULL,       &submenuESP, Icon_Link_bits, MenuSys_RenderMenu_Wrapper},
    {"Volver", MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper}
};

SubMenu submenu1 = {
    .name                = "Wifi",
    .items               = submenu1Items,
    .itemCount           = sizeof(submenu1Items)/sizeof(submenu1Items[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = &mainMenu,
    .icon                = NULL
};

// Ítems del submenu 2 (“Pantallas”)
MenuItem submenu2Items[] = {
    {"Valores IR",     NULL,               NULL, Icon_Tool_bits, OledUtils_RenderValoresIR_Wrapper},
	{"Valores MPU",     NULL,               NULL, Icon_Tool_bits, OledUtils_RenderValoresMPU_Wrapper},
	{"Test motores",     NULL,               NULL, Icon_Tool_bits, OledUtils_RenderMotorTest_Wrapper},
    {"Volver",         MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper}
};

SubMenu submenu2 = {
    .name                = "Sensores",
    .items               = submenu2Items,
    .itemCount           = sizeof(submenu2Items)/sizeof(submenu2Items[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = &mainMenu,
    .icon                = NULL
};

// submenu3Items
MenuItem submenu3Items[] = {
    {"Preferencias", NULL, NULL, Icon_Prefs_bits, NULL},
    {"Acerca de",    NULL, NULL, Icon_Info_bits,  OledUtils_About_Wrapper},
    {"Volver",       MenuSys_GoBack_Wrapper, &mainMenu, Icon_Volver_bits, MenuSys_RenderMenu_Wrapper}
};


SubMenu submenu3 = {
    .name                = "Config.",
    .items               = submenu3Items,
    .itemCount           = sizeof(submenu3Items)/sizeof(submenu3Items[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = &mainMenu,
    .icon                = NULL
};


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM3_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM4_Init(void);
static void MX_USB_PCD_Init(void);
/* USER CODE BEGIN PFP */
void initCarMode();
void UserBtn_MainTask(UserButton_Handle_t *h);
void initTCRTLib();
void TCRT_MainTask();
void InitMotorTask(void);
void My_TimerCalcFunc(uint32_t target_freq_hz,
                      uint32_t timer_clk_hz,
                      uint16_t *prescaler_out,
                      uint16_t *period_out);
void Motor_MainTask(void);
void i2cManager_MainTask(void);
void MPU_MainTask(void);
void OLED_MainTask(void);
void initUart1DmaRx(void);
/**
 * @brief Tarea principal del encoder (main loop)
 *        Maneja eventos de botón corto/largo
 */
void Encoder_MainTask(ENC_Handle_t *h);
void initMenuSystemTask(void);
void clearScreenWrapper(void);
void renderFnWrapper(void);
void drawItemWrapper(const char *name, int y, bool selected);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void clearScreenWrapper(void) {
    // Simula limpieza de pantalla

}

void drawItemWrapper(const char *name, int y, bool selected) {
    // Simula el renderizado de un ítem
    if (selected){

    }else{

    }
}

void renderFnWrapper(void) {
    // Simula el envío al display

}

void MPU_Data_Ready(void) {
    SET_FLAG(systemFlags, MPU_GET_DATA); //Setea bandera de data para volver a pedir
}

void USART1_DMA_CheckRx(void)
{
    uint16_t curr_pos = USART1_RX_DMA_BUF_LEN - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);

    if (curr_pos != usart1_rx_prev_pos) {
        usart1_rx_prev_pos = curr_pos;
        usart1_feed_pending = 1;
    }
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM3_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_SPI2_Init();
  MX_TIM4_Init();
  MX_USB_PCD_Init();
  /* USER CODE BEGIN 2 */
	 HAL_StatusTypeDef result;
	 initUart1DmaRx();
	 initTCRTLib();
	 InitMotorTask();
	 I2C_Manager_Init(&i2cManager, &hi2c1, 0);
	 __NOP(); // BREAKPOINT: I2C Manager inicializado
	 /*I2C_Manager_ScanBus();*/
	 if (I2C_Manager_IsAddressReady(&i2cManager, I2C_ADDR_MPU) == HAL_OK) {
	     if (MPU6050_Init(
	             &mpuTask,
	             I2C_ADDR_MPU,
	             &hi2c1,
	             MPU_Data_Ready,                  // callback usuario
	             &mpu_trigger
	         ) == HAL_OK) {
	         HAL_StatusTypeDef res = MPU6050_BindI2CManager(
	             &mpuTask,
	             &i2cManager,
	             DEVICE_ID_MPU,
	             1
	         );
	         if (res == HAL_OK) {
	             MPU6050_Configure(&mpuTask);
	             MPU6050_CalibrateGyro(&mpuTask, 250);
	             SET_FLAG(systemFlags, MPU_GET_DATA);
	         } else {
	             // Falló al registrar (posiblemente sin espacio en la tabla)
	         }
	     }
	 } else {
	     // El MPU no respondió en el bus
	 }
	 if (I2C_Manager_IsAddressReady(&i2cManager, I2C_ADDR_OLED) == HAL_OK) {
	     __NOP(); // BREAKPOINT: I2C detectó el OLED
	     result = ssd1306_BindI2CManager(&i2cManager, DEVICE_ID_OLED);

	     if (result == HAL_OK) {
	         __NOP(); // BREAKPOINT: OLED registrado en I2C Manager
	         if (ssd1306_Init()) {
	             __NOP(); // BREAKPOINT: OLED inicializado
	             OLED_Is_Ready();
	             __NOP(); // BREAKPOINT: OLED marcado como listo
	         }
	     } else {
	         // Falló al registrar
	     }
	 } else {
	     // El OLED no respondió
	 }
	 HAL_ADC_Start_DMA(&hadc1, (uint32_t*)sensor_raw_data, TCRT5000_NUM_SENSORS);
	 HAL_TIM_Base_Start_IT(&htim3);
	 initCarMode();
	 initMenuSystemTask();
	 //Aca debemos poner la inicializacion de la ESP
	 SET_FLAG(systemFlags2, ESP_PRESENT);

  //Solo llamo initCarMode() una vez, antes del while
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		TCRT_MainTask();
		i2cManager_MainTask();
		UserBtn_MainTask(&btnUser);
		Encoder_MainTask(&encoder);
		OLED_MainTask();
		MPU_MainTask();
		Motor_MainTask();
		// Si quieres saber cuántos sobrepasos de 1 s hubo (0..9):
		//uint8_t num_overflows = NIBBLEH_GET_STATE(btnUser.state.flags);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_USB;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */
  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */
  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 8;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_6;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_7;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = ADC_REGULAR_RANK_8;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */
  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 2812;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 255;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 249;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 9;
  if (HAL_TIM_Encoder_Init(&htim4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);
  __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USB Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_FS.Instance = USB;
  hpcd_USB_FS.Init.dev_endpoints = 8;
  hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

  /* USER CODE END USB_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
	/* Aquí podrías inicializar otras cosas antes, si hiciera falta */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LED_Pin|nRF24_CE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, nRF24_CSN_Pin|Motor_Enable_Pin|Luces_IR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ESP_ENABLE_GPIO_Port, ESP_ENABLE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED_Pin nRF24_CE_Pin */
  GPIO_InitStruct.Pin = LED_Pin|nRF24_CE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : nRF24_IRQ_Pin */
  GPIO_InitStruct.Pin = nRF24_IRQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(nRF24_IRQ_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : EncoderSW_Pin */
  GPIO_InitStruct.Pin = EncoderSW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(EncoderSW_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : User_BTN_Pin */
  GPIO_InitStruct.Pin = User_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(User_BTN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : nRF24_CSN_Pin Motor_Enable_Pin Luces_IR_Pin */
  GPIO_InitStruct.Pin = nRF24_CSN_Pin|Motor_Enable_Pin|Luces_IR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : ESP_ENABLE_Pin */
  GPIO_InitStruct.Pin = ESP_ENABLE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ESP_ENABLE_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
	GPIO_InitStruct.Pin   = GPIO_PIN_0;
	GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull  = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */


HAL_StatusTypeDef uart1_send_bytes(const uint8_t *data, uint16_t len) {
    if (!data || len == 0) {
        return HAL_ERROR;
    }

    if (usart1_tx_busy) {
        return HAL_BUSY;
    }

    usart1_tx_busy = 1;
    if (HAL_UART_Transmit_DMA(&huart1, (uint8_t *)data, len) != HAL_OK) {
        usart1_tx_busy = 0;
        return HAL_ERROR;
    }

    return HAL_OK;
}

void initUart1DmaRx(void)
{
    usart1_feed_pending = 0;
    usart1_rx_prev_pos = 0;
    HAL_UART_Receive_DMA(&huart1, usart1_rx_dma_buf, USART1_RX_DMA_BUF_LEN);
}


void initCarMode(){
	SET_CAR_MODE(IDLE_MODE);
	ledStatus.gpio_port = LED_GPIO_Port;
	ledStatus.gpio_pin = LED_Pin;
	ledStatus.flags.byte = 0;
	ledStatus.counter = 0;
	ledStatus.onTime = LED_IDLE_ONTIME;
	ledStatus.offTime = LED_IDLE_OFFTIME;
	SET_FLAG(ledStatus.flags, LED_FLAG_ACTIVE_LOW);  // Si el LED es activo en bajo
	SET_FLAG(systemFlags, INIT_CAR);
	UserButton_Init(&btnUser, User_BTN_GPIO_Port, User_BTN_Pin, USER_BUTTON_ACTIVE_HIGH);
	ENC_Init(&encoder, &htim4, 1, 4, EncoderSW_GPIO_Port, EncoderSW_Pin);
	ENC_Start(&encoder);
	SET_FLAG(systemFlags2, OLED_ACTIVE);
	SET_FLAG(systemFlags2, AP_ACTIVE);
	SET_FLAG(systemFlags2, USB_ACTIVE);
	SET_FLAG(systemFlags2, RF_ACTIVE);
}

void UserBtn_MainTask(UserButton_Handle_t *btnUser) {
    UserEvent_t local_ev = UE_NONE;

    if (IS_FLAG_SET(btnUser->state.flags, BTN_USER_SHORT_PRESS)) {
        CLEAR_FLAG(btnUser->state.flags, BTN_USER_SHORT_PRESS);
        local_ev = UE_SHORT_PRESS;
    }
    else if (IS_FLAG_SET(btnUser->state.flags, BTN_USER_LONG_PRESS)) {
        CLEAR_FLAG(btnUser->state.flags, BTN_USER_LONG_PRESS);
        local_ev = UE_LONG_PRESS;
    }

    UiEventRouter_HandleEvent(local_ev);
}

void Encoder_MainTask(ENC_Handle_t *encoder) {
    UserEvent_t local_ev = UE_NONE;
    int16_t scaled_steps = 0;

    // rotación
    if (IS_FLAG_SET(encoder->flags, ENC_FLAG_UPDATED)) {
        CLEAR_FLAG(encoder->flags, ENC_FLAG_UPDATED);
        local_ev = (encoder->dir == ENC_DIR_CW ? UE_ROTATE_CW : UE_ROTATE_CCW);
        scaled_steps = ENC_GetScaledSteps(encoder);
    }
    // click encoder
    else if (IS_FLAG_SET(encoder->btnState.flags, ENC_BTN_SHORT_PRESS)) {
        CLEAR_FLAG(encoder->btnState.flags, ENC_BTN_SHORT_PRESS);
        local_ev = UE_ENC_SHORT_PRESS;
    }
    else if (IS_FLAG_SET(encoder->btnState.flags, ENC_BTN_LONG_PRESS)) {
        CLEAR_FLAG(encoder->btnState.flags, ENC_BTN_LONG_PRESS);
        local_ev = UE_ENC_LONG_PRESS;
    }

    if (local_ev != UE_NONE && encoder->allowEncoderInput) {
        if (local_ev == UE_ROTATE_CW || local_ev == UE_ROTATE_CCW) {
            int16_t repeats = 1;

            if (!inside_menu_flag) {
                repeats = scaled_steps;
                if (repeats < 0) {
                    repeats = (int16_t)(-repeats);
                }
                if (repeats < 1) {
                    repeats = 1;
                }
            }

            for (int16_t i = 0; i < repeats; i++) {
                UiEventRouter_HandleEvent(local_ev);
            }
        } else {
            UiEventRouter_HandleEvent(local_ev);
        }
    }
}

void initTCRTLib(void)
{
    // 1) Configuro el LED (opcional). Si no tienes LED, pasa NULL en TCRT5000_Create.
    tcrtLight.port  = Luces_IR_GPIO_Port;
    tcrtLight.pin   = Luces_IR_Pin;
    tcrtLight.state = true;

    // 2) Creo la instancia del TCRT5000. Pasa USART1_PrintString (o NULL si no quieres debug).
    HAL_StatusTypeDef st = TCRT5000_Create(
        &tcrtTask,            // handler de la librería
        &procesar_flag,       // bandera que marca “8 muestras ADC listas”
        sensor_raw_data,      // arreglo uint16_t[8]
        pull_cfg,             // arreglo bool[8]
        &tcrtLight,           // LED opcional para iluminar sensores
        NULL //USART1_PrintString    // callback de debug (puede ser NULL)
    );
    TCRT5000_SetAutoModeAdvance(&tcrtTask, false);
    // 3) Informo por UART si hubo éxito o error al crear la instancia.
    if (st != HAL_OK) {

        return;  // Salimos, no tiene sentido continuar si la creación falló.
    }
    else {

    }
    // 4) Si la creación fue exitosa, inicio la calibración interna:
    if (TCRT5000_StartCalibration(&tcrtTask, 50, 50) != HAL_OK) {
        // Si falla la primera fase de calibración, lo informo:

    }
    else {
        // Si StartCalibration devolvió HAL_OK, quedó en modo CALIB_LINE_BLACK

    }
}

void TCRT_MainTask(){
	if (procesar_flag) {
		TCRT5000_Update(&tcrtTask);
		procesar_flag = false; // ya procesó la transferencia
	}
	// Si pasó a READY, entonces podemos usar IsReady + Update para filtrar continuamente:
	/* En tu bucle principal (por ejemplo, dentro de while(1)): */
	if (TCRT5000_IsReady(&tcrtTask)) {
	    // Aquí han pasado 4000 bloques de 8 muestras en modo READY.

	    // 1) Leer las banderas digitales:
	    bool too_left   = tcrtTask.results.flags.bitmap.bit0;
	    bool too_right  = tcrtTask.results.flags.bitmap.bit1;
	    bool obs_center = tcrtTask.results.flags.bitmap.bit4;
	    bool light_on   = tcrtTask.results.flags.bitmap.bit7;

	    // 2) Leer lecturas crudas si hace falta:
	    uint16_t raw_center = tcrtTask.results.raw.channels.line;
	    uint16_t raw_obs5   = tcrtTask.results.raw.channels.obs_center;

	    uint16_t values[TCRT5000_NUM_SENSORS];
		for (int i = 0; i < TCRT5000_NUM_SENSORS; i++) {
			values[i] = tcrtTask.results.raw.array[i];
		}
		SET_FLAG(systemFlags, PROCESS_IR_DATA);
	    TCRT5000_ClearReady(&tcrtTask);
	}
	if(IS_FLAG_SET(systemFlags, PROCESS_IR_DATA)){
		//Procesar aca la data de los IR
		CLEAR_FLAG(systemFlags, PROCESS_IR_DATA);

	}
}

void My_TimerCalcFunc(uint32_t target_freq_hz,
                      uint32_t timer_clk_hz,
                      uint16_t *prescaler_out,
                      uint16_t *period_out)
{
    if (target_freq_hz == 0 || timer_clk_hz == 0 || !prescaler_out || !period_out) {
        *prescaler_out = 0;
        *period_out = 0;
        return;
    }

    const uint32_t period = 255;
    uint32_t prescaler = timer_clk_hz / (target_freq_hz * (period + 1));
    if (prescaler == 0) prescaler = 1;

    *prescaler_out = (uint16_t)(prescaler - 1);
    *period_out    = (uint16_t)period;
}

/**
 * @brief  Initialize and configure motorTask handle
 */
void InitMotorTask(void)
{
    // Asignar timer y PWM default al handle
    motorTask.htim             = &htim2;
    motorTask.desired_pwm_freq = 3900;   // ~3.9 kHz
    motorTask.compute_params   = NULL;   // NULL usa valores fijos

    motorTask.left_forward_channel  = TIM_CHANNEL_1;
    motorTask.left_backward_channel = TIM_CHANNEL_2;
    motorTask.right_forward_channel = TIM_CHANNEL_3;
    motorTask.right_backward_channel= TIM_CHANNEL_4;

    // Inicializar PWM
    if (Motor_Init(&motorTask) != HAL_OK)
    {

        Error_Handler();
    }
    else
    {

    }
}

void Motor_MainTask(void)
{
    // 1) Habilitar/deshabilitar driver
    bool enabled = IS_FLAG_SET(motorTask.motorData.flags, ENABLE_MOVEMENT);
    HAL_GPIO_WritePin(Motor_Enable_GPIO_Port, Motor_Enable_Pin,
        enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);

    // Si está deshabilitado, frenamos ambos y salimos
    if (!enabled) {
        Motor_StopBoth(&motorTask);
        return;
    }

    // 2) Leemos selección y config del motor desde motorData
    uint8_t sel = NIBBLEH_GET_STATE(motorTask.motorData.flags);
    Motor_Config_t *cfg = NULL;
    switch (sel) {
        case MOTORLEFT_SELECTED:  cfg = &motorTask.motorData.motorLeft;  break;
        case MOTORRIGHT_SELECTED: cfg = &motorTask.motorData.motorRight; break;
        case MOTORNONE_SELECTED:  // ambos
        default:                  cfg = NULL;                           break;
    }

    // 3) Ejecutar según selección
    if (sel == MOTORNONE_SELECTED) {
        // Ambos: usan la misma velocidad/dirección guardada en cada config
        Motor_SetBoth(&motorTask,
                      motorTask.motorData.motorLeft.motorSpeed,
                      motorTask.motorData.motorLeft.motorDirection);
    }
    else if (cfg) {
        // Solo uno: lo arrancamos y detenemos el otro
        if (cfg->motorDirection == MOTOR_DIR_FORWARD) {
            if (sel == MOTORLEFT_SELECTED)
                Motor_SetLeftForward(&motorTask, cfg->motorSpeed);
            else
                Motor_SetRightForward(&motorTask, cfg->motorSpeed);
        } else {
            if (sel == MOTORLEFT_SELECTED)
                Motor_SetLeftBackward(&motorTask, cfg->motorSpeed);
            else
                Motor_SetRightBackward(&motorTask, cfg->motorSpeed);
        }
        // Frenar el opuesto
        if (sel == MOTORLEFT_SELECTED)       Motor_StopRight(&motorTask);
        else /* derecho */                   Motor_StopLeft(&motorTask);
    }
    else {
        // Selección inválida: frenar todo
        Motor_StopBoth(&motorTask);
    }
}


void MPU_MainTask(void) {
    // ——————————————————————————————————————————
    // 1) Trigger periódico
    // ——————————————————————————————————————————
    if (mpu_trigger) {
        __NOP();              // BREAKPOINT #1
        MPU6050_CheckTrigger(&mpuTask);
        mpu_trigger = false;  // consumimos
    }

    // ——————————————————————————————————————————
    // 2) DATA_READY (RX DMA complete)
    // ——————————————————————————————————————————
    if (IS_FLAG_SET(mpuTask.flags, DATA_READY)) {
        CLEAR_FLAG(mpuTask.flags, DATA_READY);
        __NOP();              // BREAKPOINT #2

        // ————————————————— Calibración —————————————————
        if (!IS_FLAG_SET(mpuTask.flags, CALIBRATED)) {
            // acumulamos muestras
            mpuTask.calib_sum_x += mpuTask.data.gyro_x_mdps;
            mpuTask.calib_sum_y += mpuTask.data.gyro_y_mdps;
            mpuTask.calib_sum_z += mpuTask.data.gyro_z_mdps;
            mpuTask.calib_count++;

            if (mpuTask.calib_count >= mpuTask.calib_target) {
                // calculamos bias y marcamos calibrado
                mpuTask.gyro_bias_x = mpuTask.calib_sum_x / mpuTask.calib_target;
                mpuTask.gyro_bias_y = mpuTask.calib_sum_y / mpuTask.calib_target;
                mpuTask.gyro_bias_z = mpuTask.calib_sum_z / mpuTask.calib_target;
                SET_FLAG(mpuTask.flags, CALIBRATED);
                __NOP();  // BREAKPOINT #3

                // **rearmamos un nuevo ciclo de lectura**
                mpu_trigger = true;
                MPU6050_CheckTrigger(&mpuTask);
                return;
            } else {
                // pedimos siguiente muestra de calibración
                mpu_trigger = true;
                MPU6050_CheckTrigger(&mpuTask);
                __NOP();  // breakpoint opcional
                return;
            }
        }

        // ————————————————— Fase normal ————————————————
        MPU6050_Convert(&mpuTask);
        __NOP();              // BREAKPOINT #4: conversión + ángulo listos

        // ————————————————— Callback usuario ————————————
        if (mpuTask.on_data_ready_cb) {
            __NOP();          // BREAKPOINT #5
            mpuTask.on_data_ready_cb();
        }
    }
}

void i2cManager_MainTask(){
	I2C_Manager_Tick(&i2cManager);
}


void OLED_MainTask(void) {
    // 0) Esperamos a que el SSD esté listo
    if (!IS_FLAG_SET(systemFlags, OLED_READY)) {
    	__NOP();
        return;
    }

    // 2) Refresco periódico si está permitido
    if (IS_FLAG_SET(systemFlags, OLED_REFRESH) && menuSystem.allowPeriodicRefresh) {
    	__NOP();
        if (menuSystem.renderFn) {
            menuSystem.renderFlag = true;
        }
        CLEAR_FLAG(systemFlags, OLED_REFRESH);
    }

    // 3) Ejecutar la rutina de render si alguien la pidió
    bool did_render = false;
    if (menuSystem.renderFlag && menuSystem.renderFn) {
        menuSystem.renderFlag = false;
        __NOP(); // BREAKPOINT: render de OLED solicitado
        menuSystem.renderFn();
        did_render = true;
    }

    // 4) Enviar el buffer al display (no bloqueante con DMA/I2C manager)
    if (did_render) {
        __NOP(); // BREAKPOINT: inicio de ssd1306_UpdateScreen
        ssd1306_UpdateScreen();
    }
}

void initMenuSystemTask(void) {
    RenderFunction initialRender = menuSystem.renderFn ? menuSystem.renderFn
                                                      : OledUtils_RenderDashboard_Wrapper;
    MenuSys_Init(&menuSystem);
    MenuSys_SetCallbacks(&menuSystem,
        OledUtils_Clear_Wrapper,
        OledUtils_DrawItem_Wrapper,
        initialRender,
        &inside_menu_flag,
		OledUtils_RenderDashboard_Wrapper);
    menuSystem.renderFlag = true;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
