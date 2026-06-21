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
#include<string.h>
#include<stdio.h>
#include<stdbool.h>
#include "genuine_rs485.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
RS485_Handle_t dispenser;

/*
 * Power-on offline-data handshake (per "flow_chart_when_power_on"):
 *   1. read status
 *   2. if status == 0x06 (nozzle offline): read 4.11 summary (event count)
 *   3. if count > 0: read each buffered record, newest first (count..1)
 *   4. read status again
 *
 * Verified byte-for-byte against a real capture:
 *  - 4.12 request uses func 0x0C, but the REPLY comes back as func 0x03
 *    (not 0x0C echoed).
 *  - a single offline record is 4 bytes volume (/10000) + 4 bytes sale
 *    (/100) - the same layout as the live realtime read, not the 3+5
 *    split implied by the datasheet's text example.
 */
typedef struct {
	uint16_t index;
	float volume;
	float sale;
} OfflineFuelRecord_t;

#define OFFLINE_FUEL_LOG_MAX 2000

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Fallback defines in case these aren't already in genuine_rs485.h */
#ifndef DISP_OFFLINE_TOTAL
#define DISP_OFFLINE_TOTAL 0x80  /* 4.11: data bag offline */
#endif

#ifndef DISP_FUNC_EVENT
#define DISP_FUNC_EVENT 0x0C    /* 4.12 request: event / offline record read */
#endif

#define DISP_STATUS_OFFLINE 0x06 /* status code: nozzle offline, needs handshake */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart1_rx;

/* USER CODE BEGIN PV */

OfflineFuelRecord_t offlineLog[OFFLINE_FUEL_LOG_MAX];
volatile uint16_t offlineLogCount = 0;
bool flag = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* Disp_CRC8 lives in genuine_rs485.c; declared here in case it isn't
 * already exposed via genuine_rs485.h. */
extern uint8_t Disp_CRC8(uint8_t *buf, uint16_t len);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
	if (huart == &huart1) {
		dispenser.rxLen = Size;

		dispenser.rxDone = true;

		HAL_UARTEx_ReceiveToIdle_DMA(&huart1, dispenser.rxBuf,
		RS485_RX_BUFFER_SIZE);
	}
}
/*
 * Get real time fueling data
 *
 */
void Disp_GetFuelingRealtime(void) {
	uint8_t txBuf[16];

	uint16_t txLen = Disp_BuildRead(0x01, DISP_REALTIME, 0x08, txBuf);

	RS485_Send(&dispenser, txBuf, txLen);
}

/* Get Dispenser status
 *
 */

void Disp_ReadStatus(void) {
	uint8_t tx[16];

	uint16_t len = Disp_BuildRead(0x01, DISP_STATUS, 1, tx);

	RS485_Send(&dispenser, tx, len);
}
void CheckUnitPrice(void) {
	uint8_t tx[16];

	uint16_t len = Disp_BuildRead(0x01, DISP_PRICE, 0x04, tx);

	RS485_Send(&dispenser, tx, len);
}
void accumulatedInjectedFuelAndSumOfSalesClassTotal(void) {
	uint8_t tx[16];

	uint16_t len = Disp_BuildRead(0x01, DISP_CLASS_TOTAL, 0x0C, tx);

	RS485_Send(&dispenser, tx, len);
}
void accumulatedInjectedFuelAndSumOfSales(void) {
	uint8_t tx[16];

	uint16_t len = Disp_BuildRead(0x01, DISP_TOTAL, 0x0C, tx);

	RS485_Send(&dispenser, tx, len);
}
typedef struct {
	float volume;
	float sale;
} FuelData_t;

FuelData_t fuelData;

uint8_t BCD_To_Dec(uint8_t bcd) {
	return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

uint32_t ParseBCD(uint8_t *buf, uint8_t bytes) {
	uint32_t value = 0;

	for (uint8_t i = 0; i < bytes; i++) {
		value *= 100;
		value += BCD_To_Dec(buf[i]);
	}

	return value;
}

/*
 * Blocks until the UART RX-event callback flags a finished frame, or
 * the timeout elapses. Caller must clear rxDone right before sending,
 * so a stale flag from earlier traffic can't be mistaken for the new
 * response.
 */
static bool RS485_WaitResponse(uint32_t timeout_ms) {
	uint32_t start = HAL_GetTick();

	while ((HAL_GetTick() - start) < timeout_ms) {
		if (dispenser.rxDone) {
			dispenser.rxDone = false;
			return true;
		}
	}

	return false;
}

/* Formats value as "%d.%02d" without needing printf float support. */
static int FormatFixed2(char *buf, size_t bufSize, float value) {
	int whole = (int) value;
	int frac = (int) ((value - whole) * 100.0f + 0.5f);
	return snprintf(buf, bufSize, "%d.%02d", whole, frac);
}

static void UartPrint(const char *s) {
	HAL_UART_Transmit(&huart3, (uint8_t*) s, strlen(s), 1000);
}

/*
 * Step 1 / Step 4: blocking status read (origin 0x00, len 1).
 * Returns true and fills *status on success.
 */
static bool Disp_ReadStatusBlocking(uint8_t *status) {
	uint8_t tx[16];
	uint16_t txLen = Disp_BuildRead(0x01, DISP_STATUS, 0x01, tx);

	dispenser.rxDone = false;
	RS485_Send(&dispenser, tx, txLen);

	if (!RS485_WaitResponse(200)) {
		return false;
	}

	if (!Disp_ParsePacket(dispenser.rxBuf, dispenser.rxLen)) {
		return false;
	}

	if (dispenser.rxBuf[2] != 0x03 || dispenser.rxBuf[3] != 0x01) {
		return false;
	}

	*status = dispenser.rxBuf[4];
	return true;
}

static uint8_t Disp_DecToBCD(uint8_t val) {
	return (uint8_t) (((val / 10) << 4) | (val % 10));
}

/*
 * Build the 4.12 "read offline fueling record" frame.
 * Function code 0x0C, 2-byte BCD record index (1..2000), e.g. record
 * 2000 is sent as bytes 0x20 0x00.
 *
 * Master: A5 | addr | 0C | indexHiBCD | indexLoBCD | len | CRC8
 */
static uint16_t Disp_BuildEventRead(uint8_t addr, uint16_t recordIndex,
		uint8_t len, uint8_t *txBuf) {
	uint8_t hiBCD = Disp_DecToBCD((uint8_t) (recordIndex / 100));
	uint8_t loBCD = Disp_DecToBCD((uint8_t) (recordIndex % 100));

	txBuf[0] = DISP_HEADER;
	txBuf[1] = addr;
	txBuf[2] = DISP_FUNC_EVENT;
	txBuf[3] = hiBCD;
	txBuf[4] = loBCD;
	txBuf[5] = len;

	txBuf[6] = Disp_CRC8(&txBuf[1], 5);

	return 7;
}

/*
 * Step 2: 4.11 - accumulative volume/sale recorded while offline,
 * plus how many individual fueling events (0-2000) got buffered.
 * Data layout: 5 bytes volume + 7 bytes sale + 2 bytes BCD count.
 */
static bool Disp_ReadOfflineSummary(float *volume, float *sale, uint16_t *count) {
	uint8_t tx[16];
	uint16_t txLen = Disp_BuildRead(0x01, DISP_OFFLINE_TOTAL, 0x0E, tx);

	dispenser.rxDone = false;
	RS485_Send(&dispenser, tx, txLen);

	if (!RS485_WaitResponse(200)) {
		return false;
	}

	if (!Disp_ParsePacket(dispenser.rxBuf, dispenser.rxLen)) {
		return false;
	}

	if (dispenser.rxBuf[2] != 0x03 || dispenser.rxBuf[3] != 0x0E) {
		return false;
	}

	*volume = ParseBCD(&dispenser.rxBuf[4], 5) / 100.0f;
	*sale = ParseBCD(&dispenser.rxBuf[9], 7) / 100.0f;
	*count = (uint16_t) ParseBCD(&dispenser.rxBuf[16], 2);

	return true;
}

/*
 * Step 3 (x N): 4.12 - read one buffered offline record by index.
 * Request func is 0x0C, but per the real capture the REPLY comes
 * back as func 0x03 / len 0x08, with the same 4+4 BCD layout as the
 * live realtime read (volume /10000, sale /100).
 */
static bool Disp_ReadOfflineRecord(uint16_t index, float *volume, float *sale) {
	uint8_t tx[16];
	uint16_t txLen = Disp_BuildEventRead(0x01, index, 0x08, tx);

	dispenser.rxDone = false;
	RS485_Send(&dispenser, tx, txLen);

	if (!RS485_WaitResponse(200)) {
		return false;
	}

	if (!Disp_ParsePacket(dispenser.rxBuf, dispenser.rxLen)) {
		return false;
	}

	if (dispenser.rxBuf[2] != 0x03 || dispenser.rxBuf[3] != 0x08) {
		return false;
	}

	*volume = ParseBCD(&dispenser.rxBuf[4], 4) / 10000.0f;
	*sale = ParseBCD(&dispenser.rxBuf[8], 4) / 100.0f;

	return true;
}

/*
 * Full power-on handshake, exactly matching the flow chart:
 *   1. check status
 *   2. if offline (0x06): read 4.11 summary
 *   3. if any events buffered: read each one, newest -> oldest
 *   4. check status again
 * Does nothing beyond the step-1 check if the device isn't offline -
 * no blind 2000-record sweep.
 */
static void Disp_PowerOnHandshake(void) {
	uint8_t status;
	char line[80];
	char numA[16], numB[16];
	int len;

	// ---- Step 1 ----
	if (!Disp_ReadStatusBlocking(&status)) {
		UartPrint("Step1: status read failed (no reply / bad CRC)\r\n");
		return;
	}

	len = snprintf(line, sizeof(line), "Step1: status=0x%02X\r\n", status);
	HAL_UART_Transmit(&huart3, (uint8_t*) line, (uint16_t) len, 1000);

	if (status != DISP_STATUS_OFFLINE) {
		UartPrint("Nozzle not offline - no handshake needed.\r\n");
		return;
	}

	// ---- Step 2 ----
	float summaryVolume, summarySale;
	uint16_t offlineCount;

	if (!Disp_ReadOfflineSummary(&summaryVolume, &summarySale, &offlineCount)) {
		UartPrint("Step2: 4.11 summary read failed\r\n");
		return;
	}

	if (offlineCount > OFFLINE_FUEL_LOG_MAX) {
		offlineCount = OFFLINE_FUEL_LOG_MAX;
	}

	FormatFixed2(numA, sizeof(numA), summaryVolume);
	FormatFixed2(numB, sizeof(numB), summarySale);
	len = snprintf(line, sizeof(line),
			"Step2: offline volume=%sL sale=%s events=%u\r\n", numA, numB,
			offlineCount);
	HAL_UART_Transmit(&huart3, (uint8_t*) line, (uint16_t) len, 1000);

	// ---- Step 3 (only if events were actually buffered) ----
	if (offlineCount == 0) {
		UartPrint("Step3: 0 offline events - nothing to read.\r\n");
	} else {
		UartPrint("Step3: index,volume_L,sale\r\n");

		uint16_t i = offlineCount;
		while (i >= 1) {
			float volume, sale;

			if (Disp_ReadOfflineRecord(i, &volume, &sale)) {
				if (offlineLogCount < OFFLINE_FUEL_LOG_MAX) {
					offlineLog[offlineLogCount].index = i;
					offlineLog[offlineLogCount].volume = volume;
					offlineLog[offlineLogCount].sale = sale;
					offlineLogCount++;
				}

				FormatFixed2(numA, sizeof(numA), volume);
				FormatFixed2(numB, sizeof(numB), sale);
				len = snprintf(line, sizeof(line), "%u,%s,%s\r\n", i, numA,
						numB);
			} else {
				len = snprintf(line, sizeof(line), "%u,ERR,ERR\r\n", i);
			}

			HAL_UART_Transmit(&huart3, (uint8_t*) line, (uint16_t) len, 1000);

			if (i == 1) {
				break; // avoid uint16_t underflow past 0
			}
			i--;
		}
	}

	// ---- Step 4 ----
	if (Disp_ReadStatusBlocking(&status)) {
		len = snprintf(line, sizeof(line), "Step4: status=0x%02X\r\n", status);
		HAL_UART_Transmit(&huart3, (uint8_t*) line, (uint16_t) len, 1000);
	} else {
		UartPrint("Step4: status read failed\r\n");
	}

	UartPrint("DONE\r\n");
}

/* A5 01 0C 00 01 08 CRC8
 * uint8_t txBuf[16];
 * uint8_t cmd[] = {0x01, 0x0C, 0x00, 0x01, 0x08}; without header a5 and crc8
 * uint8_t cmd2[] = {0x01, 0x05, 0x01};
 * A5 + payload + CRC8(payload) format
 */

uint16_t Disp_BuildCustomCommand(const uint8_t *payload, uint8_t payloadLen,
		uint8_t *txBuf) {
	txBuf[0] = DISP_HEADER;      // 0xA5

	memcpy(&txBuf[1], payload, payloadLen);

	txBuf[payloadLen + 1] = Disp_CRC8(&txBuf[1], payloadLen);

	return payloadLen + 2;
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MPU Configuration--------------------------------------------------------*/
	MPU_Config();

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
	MX_USART1_UART_Init();
	MX_USART3_UART_Init();
	/* USER CODE BEGIN 2 */
	RS485_Init(&dispenser, &huart1,
	DE_GPIO_GPIO_Port,
	DE_GPIO_Pin);

	HAL_Delay(200); // let the bus settle before the first command

	// Power-on handshake per the flow chart: check status, and only
	// pull offline-buffered records if the dispenser reports offline.
	//	Disp_PowerOnHandshake();

	// read status
	uint8_t txBuf[16];

	const uint8_t cmd[] = { 0x01, 0x0C, 0x00, 0x01, 0x08 };
	const uint8_t cmd_status[] = { 0x01, 0x03, 0x00, 0x01 };

	uint16_t txLen = Disp_BuildCustomCommand(cmd_status, sizeof(cmd_status),
			txBuf);
	RS485_Send(&dispenser, txBuf, txLen);

	memset(txBuf, 0, sizeof(txBuf));

	txLen = Disp_BuildCustomCommand(cmd, sizeof(cmd), txBuf);
	RS485_Send(&dispenser, txBuf, txLen);

	// Solid LED = handshake finished (check USART3 terminal for the log)
	HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_SET);

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */

	while (1) {
		// Remark on the flow chart: POS checks status every 100 ms.

		const uint8_t cmd_status[] = { 0x01, 0x03, 0x00, 0x01 };

		uint16_t txLen = Disp_BuildCustomCommand(cmd_status, sizeof(cmd_status),
				txBuf);
		RS485_Send(&dispenser, txBuf, txLen);
		HAL_Delay(100);
		if (flag) {
			const uint8_t cmd[] = { 0x01, 0x0C, 0x00, 0x01, 0x08 };
			memset(txBuf, 0, sizeof(txBuf));

			txLen = Disp_BuildCustomCommand(cmd, sizeof(cmd), txBuf);
			RS485_Send(&dispenser, txBuf, txLen);
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
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Supply configuration update enable
	 */
	HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

	/** Configure the main internal regulator output voltage
	 */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

	while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1
			| RCC_CLOCKTYPE_D1PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

	/* USER CODE BEGIN USART1_Init 0 */

	/* USER CODE END USART1_Init 0 */

	/* USER CODE BEGIN USART1_Init 1 */

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 4800;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART3_UART_Init(void) {

	/* USER CODE BEGIN USART3_Init 0 */

	/* USER CODE END USART3_Init 0 */

	/* USER CODE BEGIN USART3_Init 1 */

	/* USER CODE END USART3_Init 1 */
	huart3.Instance = USART3;
	huart3.Init.BaudRate = 115200;
	huart3.Init.WordLength = UART_WORDLENGTH_8B;
	huart3.Init.StopBits = UART_STOPBITS_1;
	huart3.Init.Parity = UART_PARITY_NONE;
	huart3.Init.Mode = UART_MODE_TX_RX;
	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
	huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart3) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8)
			!= HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART3_Init 2 */

	/* USER CODE END USART3_Init 2 */

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Stream0_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
	/* DMAMUX1_OVR_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMAMUX1_OVR_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMAMUX1_OVR_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(DE_GPIO_GPIO_Port, DE_GPIO_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : DE_GPIO_Pin */
	GPIO_InitStruct.Pin = DE_GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(DE_GPIO_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : USER_LED_Pin */
	GPIO_InitStruct.Pin = USER_LED_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(USER_LED_GPIO_Port, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void) {
	MPU_Region_InitTypeDef MPU_InitStruct = { 0 };

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
