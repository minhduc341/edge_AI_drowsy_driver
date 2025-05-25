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
#include "dcmi.h"
#include "dma.h"
#include "i2c.h"
#include "memorymap.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../Drivers/OVCamera/Inc/camera.h"
#include "../../Drivers/ST7735LCD/Inc/lcd.h"
#include "network.h"
#include "network_data.h"
#include "arm_math.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FrameWidth 128
#define FrameHeight 128
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint32_t DCMI_FrameIsReady;
uint32_t Camera_FPS=0;

uint8_t dcmi_buffer[FrameWidth * FrameHeight * 2];  // Buffer cho dữ liệu YUV422
__attribute__((section(".RAM_D2")))
uint16_t grayscale_buf[FrameWidth][FrameHeight];    // Buffer lưu dữ liệu grayscale
uint16_t framebuffer[FrameWidth][FrameHeight];     // Framebuffer cho LCD

static ai_handle network = AI_HANDLE_NULL;
AI_ALIGNED(32) static ai_u8 activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];
__attribute__((section(".RAM_D2")))
AI_ALIGNED(32) static ai_float input_data[AI_NETWORK_IN_1_SIZE];
AI_ALIGNED(32) static ai_float out_data[AI_NETWORK_OUT_1_SIZE];
static ai_buffer *ai_input;
static ai_buffer *ai_output;
static ai_error Err_Report;
uint8_t pred_class[16][16];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void LED_Blink(uint32_t Hdelay, uint32_t Ldelay)
{
  HAL_GPIO_WritePin(KEY_GPIO_Port, LED_Pin, GPIO_PIN_SET);
  HAL_Delay(Hdelay - 1);
  HAL_GPIO_WritePin(KEY_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
  HAL_Delay(Ldelay - 1);
}

void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
	static uint32_t count = 0,tick = 0;

	if(HAL_GetTick() - tick >= 1000)
	{
		tick = HAL_GetTick();
		Camera_FPS = count;
		count = 0;
	}
	count ++;

	DCMI_FrameIsReady = 1;
}
void convert_yuv422_to_grayscale_and_ai_input(uint8_t *yuv422_buf,
                                             uint16_t grayscale_buf[][128],
                                             ai_float *ai_input_buf) {
    //Chuyển YUV sang RGB565 và lưu Y vào ai_input_buf
    for (uint32_t y = 0; y < FrameHeight; y++) {
        for (uint32_t x = 0; x < FrameWidth; x += 2) {
            uint32_t idx = (y * FrameWidth + x) * 2;
            uint8_t Y0 = yuv422_buf[idx];
            uint8_t Y1 = yuv422_buf[idx + 2];

            // Chuyển Y sang RGB565 (grayscale: R=G=B=Y)
            uint8_t R0 = Y0 >> 3;  // 8 bit -> 5 bit
            uint8_t G0 = Y0 >> 2;  // 8 bit -> 6 bit
            uint8_t B0 = Y0 >> 3;  // 8 bit -> 5 bit
            uint16_t pixel0 = ((R0 & 0x1F) << 11) | ((G0 & 0x3F) << 5) | (B0 & 0x1F);

            uint8_t R1 = Y1 >> 3;
            uint8_t G1 = Y1 >> 2;
            uint8_t B1 = Y1 >> 3;
            uint16_t pixel1 = ((R1 & 0x1F) << 11) | ((G1 & 0x3F) << 5) | (B1 & 0x1F);

            // Swap byte cho LCD TFT
            grayscale_buf[y][x] = ((pixel0 & 0xFF) << 8) | ((pixel0 >> 8) & 0xFF);
            grayscale_buf[y][x + 1] = ((pixel1 & 0xFF) << 8) | ((pixel1 >> 8) & 0xFF);

            // Lưu giá trị Y trực tiếp vào ai_input_buf
            ai_input_buf[y * FrameWidth + x] = (ai_float)Y0;
            ai_input_buf[y * FrameWidth + x + 1] = (ai_float)Y1;
        }
    }

    //Chuẩn hóa dùng CMSIS-DSP
    arm_scale_f32(ai_input_buf, 1.0f / 127.5f, ai_input_buf, FrameWidth * FrameHeight); // [0, 255] -> [0, 2]
    arm_offset_f32(ai_input_buf, -1.0f, ai_input_buf, FrameWidth * FrameHeight);       // [0, 2] -> [-1, 1]
}
void AI_Init(void)
{
	ai_error err;
	/* Create a local array with the addresses of the activations buffers */
	const ai_handle acts[] = {activations};
	/* Create an instance of the model */
	err = ai_network_create_and_init(&network, acts, NULL);
	if (err.type != AI_ERROR_NONE)
	{
		Err_Report = err;
	}
	ai_input = ai_network_inputs_get(network, NULL);
	ai_output = ai_network_outputs_get(network, NULL);
}
int AI_Run(const void *in_data, const void *out_data)
{
	ai_i32 n_batch;
	//ai_error err;

	/*Update the data*/
	ai_input[0].data = AI_HANDLE_PTR(in_data);
	ai_output[0].data = AI_HANDLE_PTR(out_data);
	/*Perform the interference*/
	n_batch = ai_network_run(network, &ai_input[0],&ai_output[0]);
	if (n_batch != 1)
	{
		Err_Report = ai_network_get_error(network);
		printf("AI ai_network_run error - type=%d code=%d\r\n", Err_Report.type, Err_Report.code);
		//Error_Handler();
	}
	return n_batch;
}
bool analyze_prediction_from_output(const ai_float* output_ai) {
    int count_1 = 0; // Drowsy
    int count_2 = 0; // non-Drowsy

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            const ai_float* ptr = output_ai + (y * 16 + x);
            ai_float val0 = ptr[0];
            ai_float val1 = ptr[16 * 16];
            ai_float val2 = ptr[2 * 16 * 16];
            uint8_t max_idx = 0;

            if (val1 > val0) { val0 = val1; max_idx = 1; }
            if (val2 > val0) { max_idx = 2; }

            if (max_idx == 1) count_1++;
            else if (max_idx == 2) count_2++;
        }
    }
    if ((count_1 + count_2) > 40)
    	{return (count_1 > 0.8*count_2);}
    return false;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  //MPU_Config();
  //CPU_CACHE_Enable();
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

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
  MX_DCMI_Init();
  MX_I2C1_Init();
  MX_SPI4_Init();
  MX_TIM1_Init();
  MX_TIM16_Init();
  /* USER CODE BEGIN 2 */
  uint8_t text[20];

  LCD_Test();
  AI_Init();

  sprintf((char *)&text, "Camera Not Found");
  LCD_ShowString(0, 58, ST7735Ctx.Width, 16, 16, text);

  Camera_Init_Device(&hi2c1, FRAMESIZE_128X128); //128x128

  ST7735_LCD_Driver.FillRect(&st7735_pObj, 0, 58, ST7735Ctx.Width, 16, BLACK);

  while (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET)
  {

    sprintf((char *)&text, "Camera id:0x%x   ", hcamera.device_id);
    LCD_ShowString(0, 58, ST7735Ctx.Width, 16, 12, text);

    LED_Blink(5, 500);

    sprintf((char *)&text, "LongPress K1 to Run");
    LCD_ShowString(0, 58, ST7735Ctx.Width, 16, 12, text);

    LED_Blink(5, 500);
  }

  HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, (uint32_t)&dcmi_buffer, FrameWidth * FrameHeight * 2 / 4);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  if (DCMI_FrameIsReady)
	  {
	  	DCMI_FrameIsReady = 0; //Note that frame is processed

	  	convert_yuv422_to_grayscale_and_ai_input(dcmi_buffer, framebuffer, input_data);

	  	ST7735_FillRGBRect(&st7735_pObj, 0, 0, (uint8_t *)&framebuffer[0], FrameWidth, 80);

	  	AI_Run(&input_data, &out_data);

	  	bool result = analyze_prediction_from_output(out_data);
	  	if (result)
	  	{
	  		sprintf((char *)&text,"Drowsy");
	  	}
	  	else
	  	{
	  		sprintf((char *)&text,"OK");
	  	}
	  	//display camera FPS
	  	//sprintf((char *)&text,"%dFPS",Camera_FPS);
	  	LCD_ShowString(5,5,60,16,12,text);

	  	LED_Blink(5, 100);
	  }
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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI48, RCC_MCODIV_4);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
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
