/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "usbd_cdc_if.h"

#include "../HEXTIr/config.h"
#include "../HEXTIr/uart.h"	// HEXTir

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
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim10;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM10_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int hextir_main(void);

static int delay_loops;

void _delay_us(int us) {
	// here we have a microsecond delay.
	// The overhead is about 1.5us. So we make the min time 2us.
	if(us < 2)
		us = 2;
	// The timer has been configured to count at processor clock i.e. 96MHz.
	// It's a 16-bit timer, so it rolls out real quick. the reload is set to 50000.
	htim10.Instance->CNT = 0;
	uint32_t limit = 96*us - 150;	// 150 approximates the overhead.
	if(limit >= 50000)
		limit = 50000;	// Prevent eternal loops. Beware of off by one.
	delay_loops = 0;
	HAL_TIM_Base_Start(&htim10);
	// Just a busy loop to wait the time to pass.
	while(htim10.Instance->CNT < limit) {
		delay_loops++;
	}
	HAL_TIM_Base_Stop(&htim10);
}

void timer_init(void) {
	// BUGBUG MISSING
	int t=1;	// breakpoint placeholder
}

void set_busy_led(uint8_t state) {
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}


uint8_t crc7update(uint8_t crc, uint8_t data) {
//    ;; uint8_t crc7update(uint8_t crc, uint8_t data)
//    ;;
//    ;; input : r24: crc, r22: data
//    ;; output: r24: new crc
//crc7update:
//    ldi     r18, 8       ; number of bits to process
//    ldi     r19, 0x09    ; CRC7 polynomial
//    ldi     r20, 0x80    ; constant for inverting the top bit of the CRC
//
//loop:   lsl     r24          ; shift CRC
//    lsl     r22          ; shift data byte
//    brcc    0f           ; jump if top data bit was 0
//    eor     r24, r20     ; invert top bit of CRC if not
//0:      bst     r24, 7       ; read top bit of CRC
//    brtc    1f           ; skip if top bit of CRC is now clear
//    eor     r24, r19     ; apply polynomial
//1:      dec     r18          ; decrement bit counter
//    brne    loop         ; loop for next bit
//    andi    r24, 0x7f    ; clear top bit of
//    ret

	const uint8_t poly = 9;

	for(int i=0; i< 8; i++) {
		crc <<= 1;
		data <<= 1;
		if(data & 0x80)
			crc ^= 0x80;
		if(crc & 0x80)
			crc ^= poly;
	}
	return crc & 0x7F;
}


uint8_t crc7update2(uint8_t crc, uint8_t data)
{
	const uint8_t poly = 0x89;
	uint8_t v = (crc << 1) ^ data;
	v = (v & 0x80) ? v ^ poly : v;
	for (int i = 1; i < 8; i++) {
		v <<= 1;
		if (v & 0x80)
			v ^= poly;
	}
	return v;
}


volatile int rxbuf_level = 0;
uint8_t rxbuf[256];
uint8_t txbuf[256];
int txbuf_level = 0;
uint32_t cdc_sent_bytes = 0;

int erik_cdc_received_data(uint8_t *p, uint16_t len) {
	if(len < sizeof(rxbuf) - rxbuf_level) {
		memcpy(rxbuf+rxbuf_level, p, len);
		rxbuf_level += len;
		return len;
	}
	return 0;
}

/**
 * @brief get_cdc_received_data should probably block interrupts from CDC
 * before being run.
 */
int get_cdc_received_data(uint8_t *p, int len) {
	int amount = MIN(rxbuf_level, len);
	if(amount > 0) {
		memcpy(p, rxbuf, amount);
		if(rxbuf_level > amount) {
			// in efficient - copy data in the buffer to the beginning
			memcpy(rxbuf, rxbuf+amount, rxbuf_level-amount);
		}
		rxbuf_level -= amount;
		return amount;
	}
	return 0;
}

int shift_data_in_tx_buffer(int amount) {
	if(amount <= 0)
		return 0;

	if(txbuf_level >= amount) {
		// in efficient - copy data in the buffer to the beginning
		memcpy(txbuf, txbuf+amount, txbuf_level-amount);
		txbuf_level -= amount;
	}
	return amount;
}

// HEXTIr debug UART stuff, forward to USB CDC
void uart_init(void) {
	// Nothing
}

void uart_putc(uint8_t c) {
	if(txbuf_level < sizeof(txbuf)-1) {
		txbuf[txbuf_level++] = c;
	}
	uart_data_tosend();
}

uint8_t uart_data_tosend(void) {

	int count = txbuf_level;
	if(count > 0) {
	  if(CDC_Transmit_FS(txbuf, count) == USBD_OK) {
		  shift_data_in_tx_buffer(count);
		  cdc_sent_bytes += count;
	  }
	}
	return count > 0;
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
  MX_TIM10_Init();
  MX_USB_DEVICE_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  int delays[] = { 2, 8, 10, 100, -1 };
  int index = 0;

  // Write pin:
  // 	Reset: GPIOx->BSRR = (uint32_t)GPIO_Pin << 16U;
  // 	Set: GPIOx->BSRR = GPIO_Pin;
  __IO uint32_t *pBSRR = &DEBUG_GPIO_Port->BSRR;

  while (0) {
	  char s[80];
	  // HAL_GPIO_WritePin(DEBUG_GPIO_Port, DEBUG_Pin, GPIO_PIN_SET);
	  *pBSRR = DEBUG_Pin;
	  _delay_us(delays[index]);
	  // HAL_GPIO_WritePin(DEBUG_GPIO_Port, DEBUG_Pin, GPIO_PIN_RESET);
	  *pBSRR = DEBUG_Pin << 16;

	  sprintf(s, "_delay_us(%d): %d loops\r\n", delays[index], delay_loops);
	  CDC_Transmit_FS((uint8_t *)s, strlen(s));
	  index++;
	  if(delays[index] < 0)
		  index = 0;
	  HAL_Delay(1000);

	  int count = txbuf_level;
	  if(count > 0) {
		  if(CDC_Transmit_FS(txbuf, count) == USBD_OK) {
			  shift_data_in_tx_buffer(count);
			  cdc_sent_bytes += count;
		  }
	  }

	  if(rxbuf_level > 0) {
		  int level = 1; // rxbuf_level > 8 ? 8 : rxbuf_level;
		  uint8_t b[8];
		  get_cdc_received_data(b, level);
		  for(int i=0; i<level; i++) {
			  switch(b[i]) {
			  case '0':	// Led off
			  case '1': // led on  (write low level)
				  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, !(b[i] == '1'));
				  break;
			  }
		  }
		  // print_debug_stuff( serial_index, b[0]);
	  }
  }

  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
  HAL_Delay(3000);
  char *s = "HEXTIr ported to STM32 by Erik Piehl 2023\r\n";
  CDC_Transmit_FS((uint8_t *)s, strlen(s));
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

  hextir_main();

  while (1)
  {
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
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
  * @brief TIM10 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM10_Init(void)
{

  /* USER CODE BEGIN TIM10_Init 0 */

  /* USER CODE END TIM10_Init 0 */

  /* USER CODE BEGIN TIM10_Init 1 */

  /* USER CODE END TIM10_Init 1 */
  htim10.Instance = TIM10;
  htim10.Init.Prescaler = 0;
  htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim10.Init.Period = 50000;
  htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim10) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM10_Init 2 */

  /* USER CODE END TIM10_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DEBUG_GPIO_Port, DEBUG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SD_CS_Pin|DB_D1_Pin|DB_D0_Pin|DB_D2_Pin
                          |DB_D3_Pin|DB_HSK_Pin|DB_BAV_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY_Pin */
  GPIO_InitStruct.Pin = KEY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DEBUG_Pin SD_CS_Pin */
  GPIO_InitStruct.Pin = DEBUG_Pin|SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : DB_D1_Pin DB_D0_Pin DB_D2_Pin DB_D3_Pin
                           DB_HSK_Pin DB_BAV_Pin */
  GPIO_InitStruct.Pin = DB_D1_Pin|DB_D0_Pin|DB_D2_Pin|DB_D3_Pin
                          |DB_HSK_Pin|DB_BAV_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

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
