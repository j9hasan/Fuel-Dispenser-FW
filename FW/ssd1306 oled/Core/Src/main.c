/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
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
#include "ssd1306.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "ssd1306_fonts.h"
#include "display.h"
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
I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ==========================================================================
 *  SIMULATION LAYER  (stand-in for genuine_rs485.c + 4G modem status)
 * ========================================================================== */

typedef struct {
	float volume;
	float sale;
} Sim_Record_t;

/* ---- Configure the demo scenario here ---------------------------------- */
#define SIM_OFFLINE_RECORD_COUNT   9      /* set to 0 to test "no records" path */
#define SIM_NEW_RECORD_PERIOD_MS   3000   /* how often "new" fueling data appears */
#define SIM_ERROR_INJECT_EVERY_N   5      /* every Nth working-page tick shows an error */
#define SIM_NET_TOGGLE_PERIOD_MS   5000   /* how often the 4G status flips, for testing */
/* -------------------------------------------------------------------------*/

static const Sim_Record_t SIM_OFFLINE_RECORDS[
		SIM_OFFLINE_RECORD_COUNT > 0 ? SIM_OFFLINE_RECORD_COUNT : 1] = { {
		12.50f, 1875.00f }, { 5.00f, 750.00f }, { 20.00f, 3000.00f }, { 8.25f,
		1237.50f }, { 15.75f, 2362.50f }, { 3.10f, 465.00f },
		{ 30.00f, 4500.00f }, { 7.60f, 1140.00f }, { 18.40f, 2760.00f }, };

static uint8_t Sim_GetOfflineCount(void) {
	return SIM_OFFLINE_RECORD_COUNT;
}

static void Sim_GetOfflineRecord(uint8_t index_1based, float *volume,
		float *sale) {
	uint8_t idx = (index_1based == 0) ? 0 : (uint8_t) (index_1based - 1);
	if (idx >= SIM_OFFLINE_RECORD_COUNT)
		idx = 0;
	*volume = SIM_OFFLINE_RECORDS[idx].volume;
	*sale = SIM_OFFLINE_RECORDS[idx].sale;
}

/* Simulated "return status" printed after processing the offline page. */
static uint8_t Sim_GetOfflinePageReturnStatus(void) {
	return 0x00; /* 0x00 = OK */
}

/* Fakes a fueling cycle over time so the working page has something to
 * "hold" and then update, without any real hardware. */
static void Sim_UpdateFuelingData(float *volume, float *sale, bool *isNewRecord) {
	static uint32_t lastChangeTick = 0;
	static float curVol = 0.0f, curSale = 0.0f;
	static bool first = true;

	uint32_t now = HAL_GetTick();

	if (first || (now - lastChangeTick) >= SIM_NEW_RECORD_PERIOD_MS) {
		curVol += 4.35f;
		curSale += 652.50f;
		lastChangeTick = now;
		first = false;
		*isNewRecord = true;
	} else {
		*isNewRecord = false;
	}

	*volume = curVol;
	*sale = curSale;
}

/* Fakes an error code that appears occasionally on the working page. */
static uint8_t Sim_GetErrorCode(void) {
	static uint32_t counter = 0;
	counter++;
	if ((counter % SIM_ERROR_INJECT_EVERY_N) == 0) {
		return 0x07; /* pretend CRC error */
	}
	return 0x00; /* OK */
}

/* Fakes the 4G modem connectivity state, flipping periodically so you can
 * confirm the header indicator updates correctly on every page. Replace
 * with a real modem status read (e.g. AT+CREG / netif state) later. */
static Display_NetStatus_t Sim_GetNetStatus(void) {
	return DISPLAY_NET_OFFLINE;
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

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
	MX_I2C1_Init();
	/* USER CODE BEGIN 2 */

	ssd1306_Init();
	/* ---- Splash page ---------------------------------------------- */
	Display_SetNetStatus(Sim_GetNetStatus());
	Display_ShowSplash();
	HAL_Delay(2000);

	/* ---- Offline records page --------------------------------------- */
	uint8_t offlineCount = Sim_GetOfflineCount();

	Display_SetNetStatus(Sim_GetNetStatus());

	if (offlineCount == 0) {
		Display_ShowNoOfflineRecords();
		HAL_Delay(2000);
	} else {
		Display_ShowOfflineSummary(offlineCount);
		HAL_Delay(1500);

		for (uint8_t i = 1; i <= offlineCount; i++) {
			float vol, sale;
			Sim_GetOfflineRecord(i, &vol, &sale);

			Display_SetNetStatus(Sim_GetNetStatus());
			Display_ShowOfflineRecord(i, offlineCount, vol, sale);
			HAL_Delay(1000); /* one record per second */
		}
	}

	Display_SetNetStatus(Sim_GetNetStatus());
	Display_ShowOfflineReturnStatus(Sim_GetOfflinePageReturnStatus());
	HAL_Delay(2000);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	float vol = 0.0f, sale = 0.0f;
	bool isNew = false;
	uint8_t err = 0;
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */
		Sim_UpdateFuelingData(&vol, &sale, &isNew);
		err = Sim_GetErrorCode();
		Display_SetNetStatus(Sim_GetNetStatus());

		/* Only redraw volume/sale when new data is available - hold the
		 * last known values otherwise. Header (incl. 4G status) and the
		 * error line are still refreshed every cycle. */
		if (isNew) {
			Display_ShowWorkingPage(vol, sale, err);
		} else {
			Display_UpdateWorkingError(err);
		}

		HAL_Delay(200);
		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
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
