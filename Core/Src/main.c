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
#include "types/bitmap_type.h"
#include "types/button_state.h"
#include "utils/macros_utils.h"
#include "types/usart_buffer_type.h"
#include "usart_dma_buffer.h"
#include "motor_control.h"
#include "i2c_manager.h"
#include "oled_ssd1306_dma.h"
#include "fonts.h"

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

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart1_rx;

/* USER CODE BEGIN PV */
volatile bool procesar_flag = false;
volatile bool lanzar_ADC_trigger_flag = false;
volatile ButtonState_t btnUser;
volatile LedStatus_t ledStatus;
volatile Byte_Flag_Struct systemFlags;
volatile CarMode_t testMode;
volatile uint16_t sensor_raw_data[ TCRT5000_NUM_SENSORS ];
TCRT_LightConfig_t tcrtLight;
volatile uint8_t cnt_adc_trigger = 0;
volatile uint16_t cnt_10ms = 0;
volatile uint32_t cnt_10us = 0;
volatile uint32_t tcrt_calib_cnt_phase = 0;
//Modificcar para hacer una sola struct de velocidades modo y direcciones
static uint8_t motorBothSpeed = 100;
static int8_t motorBothDirection = MOTOR_DIR_FORWARD;
// Bandera para I2C_Manager (bus ocupado)
volatile uint8_t i2c_tx_busy_flag = 0;
// Bandera para la librería OLED (DMA ocupado)
volatile uint8_t oled_dma_busy_flag = 0;

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

/* --- Variables globales --- */

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
/* USER CODE BEGIN PFP */
void initCarMode();
void UserBtn_MainTask();
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

void USART1_PrintString(const char *msg) {
    USART1_PushTxString(&usart1Buf, msg);
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
  //Solo llamo initCarMode() una vez, antes del while
  if (!IS_FLAG_SET(systemFlags, INIT_CAR)) {
	  initCarMode();
  }
  initTCRTLib();
  InitMotorTask();
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)sensor_raw_data, TCRT5000_NUM_SENSORS);
  HAL_TIM_Base_Start_IT(&htim3);
  I2C_Manager_Init(&hi2c1, &i2c_tx_busy_flag);
  if (I2C_Manager_IsAddressReady(I2C_ADDR_OLED) == HAL_OK) {
	  HAL_StatusTypeDef result = I2C_Manager_RegisterDevice(
		  DEVICE_ID_OLED,
		  I2C_ADDR_OLED,
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
		  OLED_SetFont(&oledTask, &Font_11x18);
		  OLED_ClearBuffer(&oledTask, false);
		  OLED_DrawStr(&oledTask, 0, 0, "Hola mundo");
		  OLED_SendBuffer(&oledTask);
	  } else {
		  // Falló al registrar (posiblemente sin espacio en la tabla)
		  USART1_PushTxString(&usart1Buf, "OLED NO Registrado");
	  }
  } else {
	  // El OLED no respondió en el bus
	  USART1_PushTxString(&usart1Buf, "OLED no respondio");
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		USART1_Update();
		UserBtn_MainTask();
		TCRT_MainTask();
		Motor_MainTask();
		i2cManager_MainTask();
		OLED_MainTask();
		MPU_MainTask();
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
  htim2.Init.Prescaler = 11;
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

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : User_BTN_Pin */
  GPIO_InitStruct.Pin = User_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(User_BTN_GPIO_Port, &GPIO_InitStruct);

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
	testMode = GET_CAR_MODE();
	ledStatus.gpio_port = LED_PORT;
	ledStatus.gpio_pin = LED_PORT_PIN;
	ledStatus.flags.byte = 0;
	ledStatus.counter = 0;
	ledStatus.onTime = LED_IDLE_ONTIME;
	ledStatus.offTime = LED_IDLE_OFFTIME;
	SET_FLAG(ledStatus.flags, LED_FLAG_ACTIVE_LOW);  // Si el LED es activo en bajo
	SET_FLAG(systemFlags, INIT_CAR);
	USART1_PrintString("Bienvenido. UART1 + DMA listo.\r\n");
}

void UserBtn_MainTask(){
	if(IS_FLAG_SET(systemFlags, INIT_CAR)){ //Inicializado completamente
		if (IS_FLAG_SET(btnUser.flags, BTN_USER_SHORT_PRESS)) { // Acción short

			CLEAR_FLAG(btnUser.flags, BTN_USER_SHORT_PRESS);
		}
		if (IS_FLAG_SET(btnUser.flags, BTN_USER_LONG_PRESS)) { // Acción long
			Handle_ModeChange_ByButton(&btnUser, &ledStatus);
			motorBothSpeed+=20; //TEST ONLY!!
			CLEAR_FLAG(btnUser.flags, BTN_USER_LONG_PRESS);
		}
	}
}

void initTCRTLib(void)
{
    // 1) Configuro el LED (opcional). Si no tienes LED, pasa NULL en TCRT5000_Create.
    tcrtLight.port  = TCRT_LED_PORT;
    tcrtLight.pin   = TCRT_LED_PORT_PIN;
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

	    // 3) Una vez consumidos, limpiar la bandera para empezar a contar
	    //    otras 4000 muestras.
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
    const uint32_t desired_resolution = 256; // 8 bits

    if (target_freq_hz == 0 || timer_clk_hz == 0 || !prescaler_out || !period_out) {
        *prescaler_out = 0;
        *period_out = 0;
        return;
    }

    // ← Asegurate de pasar timer_clk_hz = HAL_RCC_GetPCLK1Freq() * 2

    uint32_t prescaler = timer_clk_hz / (target_freq_hz * desired_resolution);
    if (prescaler == 0) prescaler = 1;

    *prescaler_out = (uint16_t)(prescaler - 1);
    *period_out    = (uint16_t)(desired_resolution - 1);
}


void InitMotorTask(void)
{
    motorTask.htim = &htim2;
    motorTask.desired_pwm_freq = 22700;
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
    /* Gestionar overlay timeout */
    if (IS_FLAG_SET(systemFlags, OLED_TENMS_PASSED) && oledTask.overlay_active) {
        if (oledTask.overlay_timer_ms >= 10)
        	oledTask.overlay_timer_ms -= 10;
        else {
        	oledTask.overlay_active = false;
            for (uint8_t p = 0; p < OLED_MAX_PAGES; p++) {
            	oledTask.page_dirty[p] = true;
            }
            OLED_SendBuffer(&oledTask);
        }
    }
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
