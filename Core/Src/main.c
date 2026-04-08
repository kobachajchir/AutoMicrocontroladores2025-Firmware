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
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb.h"
#include "gpio.h"

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
#include "menu_definitions.h"
#include "uner_app.h"
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

/* USER CODE BEGIN PV */

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
        menuSystem.renderFn = OledUtils_RenderStartupNotification_Wrapper;
        //menuSystem.renderFn = OledUtils_RenderTestScreen_Wrapper;
        menuSystem.renderFlag = true;
        oled_first_draw = true;
        SET_FLAG(systemFlags, OLED_READY);
        __NOP();
    }
}

/* --- Variables globales --- */



/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
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
void MPU_Data_Ready(void) {
    SET_FLAG(systemFlags, MPU_GET_DATA); //Setea bandera de data para volver a pedir
}

void USART1_DMA_CheckRx(void)
{
	uint16_t curr_pos = UNER_App_Uart1RxGetWritePos();

    if (curr_pos != usart1_rx_prev_pos) {
        usart1_rx_prev_pos = curr_pos;
        usart1_feed_pending = 1;
        UNER_App_NotifyUart1Rx();
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
	 UNER_App_Init();
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
	 (void)UNER_App_SendCommand(UNER_CMD_ID_REBOOT_ESP, NULL, 0u);

  //Solo llamo initCarMode() una vez, antes del while
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
		UNER_App_Poll();
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
	// SET_FLAG(systemFlags2, AP_ACTIVE);
	// SET_FLAG(systemFlags2, USB_ACTIVE);
	// SET_FLAG(systemFlags2, RF_ACTIVE);
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

            if (!inside_menu_flag && encoder_fast_scroll_enabled) {
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
        if (ssd1306_UpdateScreenCompleted()) {
            menuSystem.renderFlag = false;
            __NOP(); // BREAKPOINT: render de OLED solicitado
            menuSystem.renderFn();
            did_render = true;
        }
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
