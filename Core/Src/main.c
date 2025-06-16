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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "globals.h"
#include "utils.h"
#include "utils/macros_utils.h"
#include "types/usart_buffer_type.h"
#include "usart_dma_buffer.h"
#include "motor_control.h"
#include "i2c_manager.h"
#include "oled_ssd1306_dma.h"
#include "fonts.h"
#include "menusystem.h"
#include "oled_utils.h"
#include "menuSystem_utils.h"
#include "oled_screens.h"

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

/* USER CODE BEGIN PV */

volatile bool procesar_flag = false;
volatile bool lanzar_ADC_trigger_flag = false;
volatile LedStatus_t ledStatus;
volatile Byte_Flag_Struct systemFlags;
volatile CarMode_t carMode;
volatile uint16_t sensor_raw_data[ TCRT5000_NUM_SENSORS ];
TCRT_LightConfig_t tcrtLight;
volatile uint8_t cnt_adc_trigger = 0;
volatile uint16_t cnt_10ms = 0;
volatile uint32_t cnt_10us = 0;
volatile uint32_t tcrt_calib_cnt_phase = 0;
//Modificcar para hacer una sola struct de velocidades modo y direcciones
static uint8_t motorBothSpeed = 50;
static int8_t motorBothDirection = MOTOR_DIR_FORWARD;
// Bandera para I2C_Manager (bus ocupado)
volatile uint8_t i2c_tx_busy_flag = 0;
// Bandera para la librería OLED (DMA ocupado)
volatile uint8_t oled_dma_busy_flag = 0;
volatile uint8_t inside_menu_flag;
volatile uint16_t encoderValue;

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

/* --- Handlers de librerias --- */
USART_Buffer_t usart1Buf;
TCRTHandlerTask tcrtTask;
MotorControl_Handle motorTask;
OLED_HandleTypeDef oledTask;
ButtonState_t btnUser;
ENC_Handle_t encoder;

void USART1_PrintString(const char *msg) {
    USART1_PushTxString(&usart1Buf, msg);
}

extern void    clearScreenWrapper(void);
extern void    drawItemWrapper(const char *name, int y, bool selected);
extern void    renderFnWrapper(void);
void submenu1_Open(void) {
	submenuFn(&menuSystem, &submenu1);
	USART1_PrintString("Submenu1");
}
void submenu2_Open(void) {
	submenuFn(&menuSystem, &submenu2);
	USART1_PrintString("Submenu2");
}
void submenu3_Open(void) {
	submenuFn(&menuSystem, &submenu3);
	USART1_PrintString("Submenu3");
}
void setMode_IDLE(void){
	carMode = IDLE_MODE;
	USART1_PrintString("IDLE");
}
void setMode_FOLLOW(void){
	carMode = FOLLOW_MODE;
	USART1_PrintString("FOLLOW");
}
void setMode_TEST(void){
	carMode = TEST_MODE;
	USART1_PrintString("TEST");
}
/* --- Variables globales --- */

// Sistema de menú
MenuSystem menuSystem = {
    .currentMenu      = &mainMenu,
    .clearScreen      = clearScreenWrapper,
    .drawItem         = drawItemWrapper,
    .renderFn         = NULL,
    .insideMenuFlag   = &inside_menu_flag,
    .renderFlag       = false
};

// Ítems del menú principal
MenuItem mainMenuItems[] = {
    {"Modo",      submenu1_Open,      NULL, NULL, displayMenuCustom},
    {"Pantallas", submenu2_Open,      NULL, NULL, displayMenuCustom},
    {"Config.",   submenu3_Open,      NULL, NULL, displayMenuCustom},
    {"VOLVER",    navigateBackInMenu, NULL, NULL, renderDashboard_Wrapper}
};

// Menú principal
SubMenu mainMenu = {
    .name                = "Main Menu",
    .items               = mainMenuItems,
    .itemCount           = sizeof(mainMenuItems)/sizeof(mainMenuItems[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = NULL,
    .icon                = NULL
};

// Ítems del submenu 1: MODO (sin pantalla asociada por ahora)
MenuItem submenu1Items[] = {
    {"IDLE",   setMode_IDLE,       NULL, NULL, NULL},
    {"FOLLOW", setMode_FOLLOW,     NULL, NULL, NULL},
    {"TEST",   setMode_TEST,       NULL, NULL, NULL},
    {"VOLVER", navigateBackInMenu, &mainMenu, NULL, displayMenuCustom}
};

SubMenu submenu1 = {
    .name                = "Modo",
    .items               = submenu1Items,
    .itemCount           = sizeof(submenu1Items)/sizeof(submenu1Items[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = &mainMenu,
    .icon                = NULL
};

// Ítems del submenu 2 (“Pantallas”)
MenuItem submenu2Items[] = {
    {"Valores IR",     NULL,               NULL, NULL, renderValoresIR_Wrapper},
    {"VOLVER",         navigateBackInMenu, &mainMenu, NULL, displayMenuCustom}
};

SubMenu submenu2 = {
    .name                = "Pantallas",
    .items               = submenu2Items,
    .itemCount           = sizeof(submenu2Items)/sizeof(submenu2Items[0]),
    .currentItemIndex    = 0,
    .firstVisibleItem    = 0,
    .parent              = &mainMenu,
    .icon                = NULL
};

// Ítems del submenu 3: Mockup sin pantallas por ahora
MenuItem submenu3Items[] = {
    {"Option 1", NULL,               NULL, NULL, NULL},
    {"Option 2", NULL,               NULL, NULL, NULL},
    {"VOLVER",   navigateBackInMenu, &mainMenu, NULL, displayMenuCustom}
};

SubMenu submenu3 = {
    .name                = "Configuración",
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
/* USER CODE BEGIN PFP */
void initCarMode();
void UserBtn_MainTask(ButtonState_t *h);
void initTCRTLib();
void TCRT_MainTask();
void initUsartBufferHandler();
void USART1_PrintString(const char *msg);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void InitMotorTask(void);
void My_TimerCalcFunc(uint32_t target_freq_hz,
                      uint32_t timer_clk_hz,
                      uint16_t *prescaler_out,
                      uint16_t *period_out);
void Motor_MainTask(void);
void i2cManager_MainTask(void);
void MPU_MainTask(void);
void OLED_MainTask(void);
void OLED_DMA_Complete_I2CManager(void);
void OLED_GrantAccess_I2CManager(void);
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
/* Wrappers para HAL UART DMA */
static HAL_StatusTypeDef HAL_UART1_TxDMA_Wrapper(uint8_t *pData, uint16_t size)
{
    return HAL_UART_Transmit_DMA(&huart1, pData, size);
}

static HAL_StatusTypeDef HAL_UART1_RxDMA_Wrapper(uint8_t *pData, uint16_t size)
{
    return HAL_UART_Receive_DMA(&huart1, pData, size);
}

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

// Wrapper para I2C_Manager cuando termina el DMA
void OLED_DMA_Complete_I2CManager(void) {
    // aquí puedes poner un breakpoint o togglear un LED para debug
    OLED_DMA_CompleteCallback(&oledTask);
}

// Wrapper para I2C_Manager cuando concede acceso al bus
void OLED_GrantAccess_I2CManager(void) {
    // breakpoint / LED toggle aquí si quieres
    OLED_GrantAccessCallback(&oledTask);
}

void OLED_RequestBusUse_I2CManager(void) {
    // Pide acceso inmediato al manager
    I2C_Manager_RequestAccess(DEVICE_ID_OLED);
}

/* Callback que procesará cada byte recibido */
static void MyRxHandler(uint8_t byte)
{
    // Ejemplo: convertir a mayúscula y reenviar
    if (byte >= 'a' && byte <= 'z') {
        byte -= ('a' - 'A');
    }
    char eco[4] = { (char)byte, '\r', '\n', '\0' };
    USART1_PushTxString(&usart1Buf, eco);
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
  MX_USB_DEVICE_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_SPI2_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  if (USART1_RegisterHandle(&huart1) != HAL_OK){
	 Error_Handler();
  }
  if (USART1_SetDMACallbacks(HAL_UART1_TxDMA_Wrapper,
							 HAL_UART1_RxDMA_Wrapper) != HAL_OK)
  {
	  Error_Handler();
  }

  if (USART1_DMA_Init(&usart1Buf) != HAL_OK) {
	  Error_Handler();
  }

  if (USART1_SetRxHandler(MyRxHandler) != HAL_OK) {
	  Error_Handler();
  }
  for (uint8_t addr7 = 1; addr7 < 0x80; addr7++) {
      // pruebo cada dirección 7-bit << 1
      if (HAL_I2C_IsDeviceReady(&hi2c1, addr7 << 1, 1, 10) == HAL_OK) {
          // aquí addr7 es, por ejemplo, 0x3C; addr7<<1 = 0x78
          char buf[32];
          snprintf(buf, sizeof(buf), "I2C device at 0x%02X\n", addr7);
          USART1_PrintString(buf);
          HAL_Delay(100);
          __NOP();
      }
  }
  initTCRTLib();
  InitMotorTask();
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)sensor_raw_data, TCRT5000_NUM_SENSORS);
  HAL_TIM_Base_Start_IT(&htim3);
  if (HAL_I2C_IsDeviceReady(&hi2c1, I2C_ADDR_OLED, 3, 100) == HAL_OK) {
	  USART1_PrintString("OLED responde HAL\r\n");
  } else {
	  USART1_PrintString("OLED NO responde HAL\r\n");
  }
  __NOP();
  I2C_Manager_Init(&hi2c1, &i2c_tx_busy_flag);
  if (I2C_Manager_IsAddressReady(I2C_ADDR_OLED) == HAL_OK) {
	  HAL_StatusTypeDef result = I2C_Manager_RegisterDevice(
		  DEVICE_ID_OLED,
		  (I2C_ADDR_OLED << 1),
		  OLED_DMA_Complete_I2CManager,
		  OLED_GrantAccess_I2CManager,
		  1
	  );
	  if (result == HAL_OK) {
		  // Éxito: El OLED está presente y fue registrado
		  USART1_PushTxString(&usart1Buf, "OLED Registrado");
		  OLED_Init(&oledTask,
				   &hi2c1,
				   I2C_ADDR_OLED,
				   &oled_dma_busy_flag,    // ← pasa la bandera DMA
				   OLED_RequestBusUse_I2CManager);
	  } else {
		  // Falló al registrar (posiblemente sin espacio en la tabla)
		  USART1_PushTxString(&usart1Buf, "OLED NO Registrado");
	  }
  } else {
	  // El OLED no respondió en el bus
	  USART1_PushTxString(&usart1Buf, "OLED no respondio");
  }
  if (!IS_FLAG_SET(systemFlags, INIT_CAR)) {
  	  initCarMode();
  	  initMenuSystemTask();
  }
  //Solo llamo initCarMode() una vez, antes del while
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		USART1_Update();
		TCRT_MainTask();
		i2cManager_MainTask();
		if(IS_FLAG_SET(systemFlags, INIT_CAR)){ //Inicializado completamente
			UserBtn_MainTask(&btnUser);
			Encoder_MainTask(&encoder);
		}
		if(IS_FLAG_SET(systemFlags, OLED_READY)){ //Inicializado completamente
			OLED_MainTask();
		}
		MPU_MainTask();
		Motor_MainTask();
		// Si quieres saber cuántos sobrepasos de 1 s hubo (0..9):
		//uint8_t num_overflows = NIBBLEH_GET_STATE(btnUser.flags);

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
  sConfig.EncoderMode = TIM_ENCODERMODE_TI2;
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
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, IR_LED_Pin|nRF24_CSN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ESP_ENABLE_GPIO_Port, ESP_ENABLE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

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

  /*Configure GPIO pins : IR_LED_Pin nRF24_CSN_Pin */
  GPIO_InitStruct.Pin = IR_LED_Pin|nRF24_CSN_Pin;
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

/**
  * @brief  Callback que llama la HAL cuando el DMA de UART1 completa una transmisión.
  *         Necesario para que la librería maneje el “avance de índices” y, si hay datos
  *         pendientes, relance otro bloque DMA.
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    USART1_DMA_TxCpltHandler(huart);
}

void initUsartBufferHandler(){
    usart1Buf.headTx = 0;
    usart1Buf.tailTx = 0;
    usart1Buf.headRx = 0;
    usart1Buf.tailRx = 0;
    usart1Buf.flags.byte = 0;
}

void initCarMode(){
	NIBBLEH_SET_STATE(systemFlags, IDLE_MODE);
	carMode = GET_CAR_MODE();
	ledStatus.gpio_port = LED_GPIO_Port;
	ledStatus.gpio_pin = LED_Pin;
	ledStatus.flags.byte = 0;
	ledStatus.counter = 0;
	ledStatus.onTime = LED_IDLE_ONTIME;
	ledStatus.offTime = LED_IDLE_OFFTIME;
	SET_FLAG(ledStatus.flags, LED_FLAG_ACTIVE_LOW);  // Si el LED es activo en bajo
	SET_FLAG(systemFlags, INIT_CAR);
	btnUser.flags.byte = 0;
	btnUser.counter = 0;
	btnUser.port = User_BTN_GPIO_Port;
	btnUser.pin = User_BTN_Pin;
	ENC_Init(&encoder, &htim4, 1, 2,EncoderSW_GPIO_Port, EncoderSW_Pin);
	ENC_Start(&encoder);
	if(OLED_Is_Ready(&oledTask)){
	  SET_FLAG(systemFlags, OLED_READY);
	}
	USART1_PrintString("Bienvenido. UART1 + DMA listo.\r\n");
}

void UserBtn_MainTask(ButtonState_t *h){
	if (IS_FLAG_SET(h->flags, BTN_USER_SHORT_PRESS)) { // Acción short

		CLEAR_FLAG(h->flags, BTN_USER_SHORT_PRESS);
	}
	if (IS_FLAG_SET(h->flags, BTN_USER_LONG_PRESS)) { // Acción long, entra y sale del menu
		//Handle_ModeChange_ByButton(h, &ledStatus);
		INSIDE_MENU = !INSIDE_MENU;
		if(INSIDE_MENU){
			USART1_PrintString("Entered menu system\r\n");
		}else{
			USART1_PrintString("Exited menu system\r\n");
		}
		CLEAR_FLAG(h->flags, BTN_USER_LONG_PRESS);
	}
}

void Encoder_MainTask(ENC_Handle_t *h)
{
    // ROTARY: si hubo giro, avanzamos o retrocedemos en el menú
    if (IS_FLAG_SET(h->flags, ENC_FLAG_UPDATED)) {
        CLEAR_FLAG(h->flags, ENC_FLAG_UPDATED);
        if (INSIDE_MENU) {
            if (h->dir == ENC_DIR_CW) {
                moveCursorDown(&menuSystem);
            }
            else if (h->dir == ENC_DIR_CCW) {
                moveCursorUp(&menuSystem);
            }
            // Imprimir el nombre
            const char *name = menuSystem.currentMenu->items[menuSystem.currentMenu->currentItemIndex].name;
            char buf[64];
            sprintf(buf, "Current item: %s\r\n", name);
            USART1_PrintString(buf);
        } else {
            // fuera del menú, podrías usar el encoder para otra cosa
        }
    }

    // SHORT PRESS: dentro del menú ejecuta acción; fuera no hace nada
    if (IS_FLAG_SET(h->btnState.flags, ENC_BTN_SHORT_PRESS)) {
        CLEAR_FLAG(h->btnState.flags, ENC_BTN_SHORT_PRESS);
        if (INSIDE_MENU) {
        	selectCurrentItem(&menuSystem);
        	//Aca iria el call al render de cada item
        }else{

        }
    }

    // LONG PRESS: dentro vuelve al main menu; fuera nada
    if (IS_FLAG_SET(h->btnState.flags, ENC_BTN_LONG_PRESS)) {
        CLEAR_FLAG(h->btnState.flags, ENC_BTN_LONG_PRESS);
        if (INSIDE_MENU) {
            navigateToMainMenu(&menuSystem);
        } else {

        }
    }
}

void initTCRTLib(void)
{
    // 1) Configuro el LED (opcional). Si no tienes LED, pasa NULL en TCRT5000_Create.
    tcrtLight.port  = IR_LED_GPIO_Port;
    tcrtLight.pin   = IR_LED_Pin;
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
        USART1_PrintString("TCRT5000_Create devolvió HAL_ERROR\r\n");
        return;  // Salimos, no tiene sentido continuar si la creación falló.
    }
    else {
        USART1_PrintString("TCRT5000_Create devolvió HAL_OK\r\n");
    }
    // 4) Si la creación fue exitosa, inicio la calibración interna:
    if (TCRT5000_StartCalibration(&tcrtTask, 50, 50) != HAL_OK) {
        // Si falla la primera fase de calibración, lo informo:
        USART1_PrintString("main - Error al iniciar calibración\r\n");
    }
    else {
        // Si StartCalibration devolvió HAL_OK, quedó en modo CALIB_LINE_BLACK
        USART1_PrintString("main - Calibración iniciada (50 muestras línea, 50 obst)\r\n");
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
	    // ... aquí procesas esos 4000 bloques como prefieras ...
		/*
		if (USART1_PushTxU16Values(&usart1Buf, values, TCRT5000_NUM_SENSORS) != HAL_OK) {
		    // manejar buffer lleno o error
		}else{

		}*/
	    // 3) Una vez consumidos, limpiar la bandera para empezar a contar
	    //    otras 4000 muestras.
		SET_FLAG(systemFlags, PROCESS_IR_DATA);
	    TCRT5000_ClearReady(&tcrtTask);
	}
	if(IS_FLAG_SET(systemFlags, PROCESS_IR_DATA)){
		//Procesar aca la data de los IR
		CLEAR_FLAG(systemFlags, PROCESS_IR_DATA);
		if (menuSystem.renderFn == renderValoresIR) {
		    // Estamos en la pantalla de Valores IR:
		    menuSystem.renderFlag = true;
		}
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



void InitMotorTask(void)
{
    motorTask.htim = &htim2;
    motorTask.desired_pwm_freq = 100;
    motorTask.compute_params = My_TimerCalcFunc;

    motorTask.left_forward_channel  = TIM_CHANNEL_1;
    motorTask.left_backward_channel = TIM_CHANNEL_2;
    motorTask.right_forward_channel = TIM_CHANNEL_3;
    motorTask.right_backward_channel = TIM_CHANNEL_4;

    // 🟡 VINCULÁS EL MÉTODO init ANTES DE USARLO
    Motor_AttachMethods(&motorTask);

    if (motorTask.init(&motorTask) != HAL_OK)
    {
        USART1_PrintString("InitMotorTask devolvió HAL_ERROR\r\n");
        Error_Handler();
    }else{
    	USART1_PrintString("Motores lib OK\r\n");
    }
}


void Motor_MainTask(void)
{
    if (motorTask.set_both != NULL) {
        motorTask.set_both(&motorTask, MOTOR_DIR_FORWARD, motorBothSpeed);
    } else {
        // Opcional: breakpoint o log para debug
    }
}

void MPU_MainTask(){
	if(IS_FLAG_SET(systemFlags, MPU_GET_DATA)){
		CLEAR_FLAG(systemFlags, MPU_GET_DATA);
	}
}

void i2cManager_MainTask(){
	I2C_Manager_Update();
}


void OLED_MainTask(void) {
    // Verificar si pasó el tiempo de 10 ms y hay overlay activo
	if (IS_FLAG_SET(systemFlags, OLED_TENMS_PASSED) && oledTask.overlay_active){
		// Decrementar el temporizador del overlay
		if (oledTask.overlay_timer_ms >= 10) {
			oledTask.overlay_timer_ms -= 10;
		} else {
			// Tiempo agotado, desactivar overlay y marcar todas las páginas como sucias
			oledTask.overlay_active = false;

			for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
			    oledTask.page_dirty_main[p] = true;
			}

			OLED_SendBuffer(&oledTask);
		}
    }
    if (menuSystem.renderFlag &&
        menuSystem.renderFn) {
        menuSystem.renderFlag = false;
        menuSystem.renderFn();
        OLED_SendBuffer(&oledTask);
    }
}


void initMenuSystemTask(void) {
    initMenuSystem(&menuSystem);
    MenuSystem_SetCallbacks(&menuSystem,
        clearScreenWrapper,
        drawItemWrapper,
        renderFnWrapper,
        &inside_menu_flag);
    menuSystem.renderFn = renderDashboard_Wrapper;
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

#ifdef  USE_FULL_ASSERT
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
