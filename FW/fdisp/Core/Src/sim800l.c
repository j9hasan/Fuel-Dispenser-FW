/**
 ******************************************************************************
 * @file           : sim800l.c
 * @brief          : Minimal SIM800L GSM/GPRS module driver for connectivity
 *                    testing over a HAL UART.
 ******************************************************************************
 */

#include "sim800l.h"
#include <string.h>
#include <stdbool.h>

/* Private variables -----------------------------------------------------------*/
static UART_HandleTypeDef *sim800l_uart = NULL;
static char sim800l_rx_buf[128];

/* Private function prototypes --------------------------------------------------*/
static void SIM800L_FlushRx(void);
static uint8_t bearerOpened = 0;
bool deviceOffline = 1;

/**
 * @brief  Bind the driver to the UART handle used to talk to the module.
 * @param  huart Pointer to an already-initialized UART_HandleTypeDef (e.g. &huart1)
 * @retval None
 */
SIM800L_StatusTypeDef SIM800L_Initialize(UART_HandleTypeDef *huart,
		uint32_t timeout_ms) {
	sim800l_uart = huart;
	uint32_t start = HAL_GetTick();
	SIM800L_StatusTypeDef status;

	while ((HAL_GetTick() - start) < timeout_ms) {
		status = SIM800L_ConnectInternet();

		if (status == SIM800L_OK) {
//			int8_t sq = SIM800L_GetSignalQuality();
			return SIM800L_OK;
		}

		HAL_Delay(SIM800L_RETRY_INTERVAL_MS);     // Retry every 2 seconds
	}

	return status;           // Return the last error encountered
}
/**
 * @brief  Check whether the SIM800L Internet bearer is active.
 *
 * @retval true   Internet bearer is connected.
 * @retval false  Internet bearer is not connected.
 */
bool SIM800L_IsInternetConnected(void) {
	if (sim800l_uart == NULL)
		return false;

	if (!SIM800L_SendCommand("AT+SAPBR=2,1", 3000))
		return false;

	return (strstr(sim800l_rx_buf, "+SAPBR: 1,1") != NULL);
}
/**
 * @brief Wait until the expected substring is received or timeout occurs.
 *
 * @param expected     Substring to search for.
 * @param timeout_ms   Timeout in milliseconds.
 *
 * @retval 1 Expected string received.
 * @retval 0 Timeout or error.
 */
//static uint8_t SIM800L_ReadResponse(const char *expected, uint32_t timeout_ms) {
//	if (sim800l_uart == NULL)
//		return 0U;
//
////	SIM800L_FlushRx();
//	memset(sim800l_rx_buf, 0, sizeof(sim800l_rx_buf));
//
//	uint32_t start = HAL_GetTick();
//	uint16_t idx = 0;
//
//	while ((HAL_GetTick() - start) < timeout_ms) {
//		uint8_t byte;
//
//		if (HAL_UART_Receive(sim800l_uart, &byte, 1, 20) == HAL_OK) {
//			if (idx < (sizeof(sim800l_rx_buf) - 1)) {
//				sim800l_rx_buf[idx++] = (char) byte;
//				sim800l_rx_buf[idx] = '\0';      // Keep string terminated
//			}
//
//			if ((expected != NULL)
//					&& (strstr(sim800l_rx_buf, expected) != NULL)) {
//				return 1U;
//			}
//		}
//	}
//
//	return 0U;
//}
/**
 * @brief Receive a complete SIM800L response.
 *
 * Reads until one of the following occurs:
 *   - "\r\nOK\r\n"
 *   - "\r\nERROR\r\n"
 *   - timeout
 *
 * The complete response is stored in sim800l_rx_buf.
 *
 * @retval 1 Response completed with OK.
 * @retval 0 Timeout or ERROR.
 */
static uint8_t SIM800L_ReadResponse(uint32_t timeout_ms) {
	if (sim800l_uart == NULL)
		return 0U;

	memset(sim800l_rx_buf, 0, sizeof(sim800l_rx_buf));

	uint32_t start = HAL_GetTick();
	uint16_t idx = 0;

	while ((HAL_GetTick() - start) < timeout_ms) {
		uint8_t byte;

		if (HAL_UART_Receive(sim800l_uart, &byte, 1, 20) == HAL_OK) {
			/* Restart timeout whenever new data arrives */
			start = HAL_GetTick();

			if (idx < sizeof(sim800l_rx_buf) - 1) {
				sim800l_rx_buf[idx++] = byte;
				sim800l_rx_buf[idx] = '\0';
			}

			/* Entire response received */
			if (strstr(sim800l_rx_buf, "\r\nOK\r\n") != NULL) {
				return 1U;
			}

			/* Command failed */
			if (strstr(sim800l_rx_buf, "\r\nERROR\r\n") != NULL) {
				return 0U;
			}
		}
	}

	return 0U;
}
/**
 * @brief Send an AT command and wait for the expected response.
 *
 * @param cmd         Command WITHOUT trailing CR/LF.
 * @param expected    Expected substring (e.g. "OK", "DOWNLOAD").
 * @param timeout_ms  Timeout in milliseconds.
 *
 * @retval 1 Success
 * @retval 0 Failure
 */
//uint8_t SIM800L_SendCommand(const char *cmd, const char *expected,
//		uint32_t timeout_ms) {
//	if (sim800l_uart == NULL)
//		return 0U;
//	SIM800L_FlushRx();
//
//	HAL_StatusTypeDef status;
//
//	status = HAL_UART_Transmit(sim800l_uart, (uint8_t*) cmd,
//			(uint16_t) strlen(cmd), 100);
//
//	if (status != HAL_OK)
//		return 0U;
//
//	status = HAL_UART_Transmit(sim800l_uart, (uint8_t*) "\r\n", 2, 100);
//
//	if (status != HAL_OK)
//		return 0U;
//
//	return SIM800L_ReadResponse(expected, timeout_ms);
//}
/**
 * @brief Send an AT command and receive the complete response.
 *
 * @param cmd Command without CR/LF.
 * @param timeout_ms Response timeout.
 *
 * @retval 1 Received OK.
 * @retval 0 ERROR or timeout.
 */
uint8_t SIM800L_SendCommand(const char *cmd, uint32_t timeout_ms) {
	if (sim800l_uart == NULL)
		return 0U;

	SIM800L_FlushRx();

	if (HAL_UART_Transmit(sim800l_uart, (uint8_t*) cmd, strlen(cmd), 100)
			!= HAL_OK) {
		return 0U;
	}

	if (HAL_UART_Transmit(sim800l_uart, (uint8_t*) "\r\n", 2, 100) != HAL_OK) {
		return 0U;
	}

	return SIM800L_ReadResponse(timeout_ms);
}
uint8_t SIM800L_SendData(const uint8_t *data, uint16_t length) {
	if (sim800l_uart == NULL)
		return 0;

	return (HAL_UART_Transmit(sim800l_uart, (uint8_t*) data, length, 5000)
			== HAL_OK);
}
/**
 * @brief  Connect SIM800L to the Internet.
 *
 * Sequence:
 *   1. Check module is alive.
 *   2. Verify SIM card is ready.
 *   3. Verify GSM network registration.
 *   4. Verify GPRS attachment.
 *   5. Open GPRS bearer.
 *
 * @retval SIM800L_OK
 * @retval SIM800L_ERROR_NO_RESP
 * @retval SIM800L_ERROR_NO_SIM
 * @retval SIM800L_ERROR_NO_NET
 * @retval SIM800L_ERROR_NO_DATA
 * @retval SIM800L_ERROR_GPRS
 */
SIM800L_StatusTypeDef SIM800L_ConnectInternet(void) {
	if (sim800l_uart == NULL)
		return SIM800L_ERROR_TIMEOUT;

	/*----------------------------------------------------------
	 1. Module Alive
	 ----------------------------------------------------------*/
	for (uint8_t i = 0; i < 3; i++) {
		if (SIM800L_SendCommand("AT", 1000)) {
			break;
		}

		if (i == 2)
			return SIM800L_ERROR_NO_RESP;

		HAL_Delay(500);
	}

	/*----------------------------------------------------------
	 2. SIM Ready
	 ----------------------------------------------------------*/
	if (!SIM800L_SendCommand("AT+CPIN?", 2000)) {
		return SIM800L_ERROR_NO_SIM;
	}

	if (strstr(sim800l_rx_buf, "+CPIN: READY") == NULL) {
		return SIM800L_ERROR_NO_SIM;
	}

	/*----------------------------------------------------------
	 3. GSM Network Registration
	 ----------------------------------------------------------*/
	uint8_t registered = 0;

	for (uint8_t i = 0; i < 30; i++)        // Wait up to 60 seconds
			{
		if (SIM800L_SendCommand("AT+CREG?", 2000)) {
			if (strstr(sim800l_rx_buf, "+CREG: 0,1")
					|| strstr(sim800l_rx_buf, "+CREG: 0,5")) {
				registered = 1;
				break;
			}
		}

		HAL_Delay(2000);
	}

	if (!registered) {
		return SIM800L_ERROR_NO_NET;
	}

	/*----------------------------------------------------------
	 4. GPRS Attached
	 ----------------------------------------------------------*/
	uint8_t attached = 0;

	for (uint8_t i = 0; i < 10; i++) {
		if (SIM800L_SendCommand("AT+CGATT?", 2000)) {
			if (strstr(sim800l_rx_buf, "+CGATT: 1")) {
				attached = 1;
				break;
			}
		}

		HAL_Delay(1000);
	}

	if (!attached) {
		return SIM800L_ERROR_NO_DATA;
	}

	/*----------------------------------------------------------
	 5. Open GPRS Bearer
	 ----------------------------------------------------------*/
	SIM800L_StatusTypeDef status = SIM800L_OpenBearer();

	if (status != SIM800L_OK) {
		return status;
	}

	return SIM800L_OK;
}

/**
 * @brief Check whether GPRS is attached.
 *
 * @retval 1 GPRS attached.
 * @retval 0 GPRS not attached.
 */
uint8_t SIM800L_CheckGPRSAttach(void) {
	if (!SIM800L_SendCommand("AT+CGATT?", 5000)) {
		return 0U;
	}

	return (strstr(sim800l_rx_buf, "+CGATT: 1") != NULL);
}
/**
 * @brief Close GPRS bearer.
 */
void SIM800L_CloseBearer(void) {
	SIM800L_SendCommand("AT+SAPBR=0,1", 5000);
}
/**
 * @brief Open GPRS bearer.
 *
 * @retval SIM800L_OK
 * @retval SIM800L_ERROR_GPRS
 */
SIM800L_StatusTypeDef SIM800L_OpenBearer(void) {
	/* Close previous bearer (ignore result) */
	SIM800L_SendCommand("AT+SAPBR=0,1", 5000);

	HAL_Delay(1000);

	/* Configure bearer type */
	if (!SIM800L_SendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 3000)) {
		return SIM800L_ERROR_GPRS;
	}

	/* Configure APN */
	if (!SIM800L_SendCommand("AT+SAPBR=3,1,\"APN\",\"internet\"", 3000)) {
		return SIM800L_ERROR_GPRS;
	}

	/* Open bearer */
	if (!SIM800L_SendCommand("AT+SAPBR=1,1", 15000)) {
		return SIM800L_ERROR_GPRS;
	}

	/* Verify bearer */
	if (!SIM800L_SendCommand("AT+SAPBR=2,1", 5000)) {
		return SIM800L_ERROR_GPRS;
	}

	if (strstr(sim800l_rx_buf, "+SAPBR: 1,1") == NULL) {
		return SIM800L_ERROR_GPRS;
	}

	return SIM800L_OK;
}

int16_t SIM800L_RSSItoDbm(int8_t rssi) {
	if (rssi == 99)
		return -999;     // Unknown

	if (rssi < 0 || rssi > 31)
		return -999;

	return -113 + (2 * rssi);
}

/**
 * @brief  Query signal quality (AT+CSQ).
 *
 * @retval  0~31  RSSI index (higher is better)
 * @retval 99     Signal strength unknown
 * @retval -1     Communication or parsing error
 */
int8_t SIM800L_GetSignalQuality(void) {
	if (!SIM800L_SendCommand("AT+CSQ", 2000)) {
		return -1;
	}

	char *p = strstr(sim800l_rx_buf, "+CSQ:");
	if (p == NULL) {
		return -1;
	}

	int rssi;
	int ber;

	/* Expected response:
	 * +CSQ: <rssi>,<ber>
	 * Example:
	 * +CSQ: 23,0
	 */
	if (sscanf(p, "+CSQ: %d,%d", &rssi, &ber) != 2) {
		return -1;
	}

	return (int8_t) rssi;
}

bool SIM800L_WaitForHTTPAction(uint32_t timeout) {
	memset(sim800l_rx_buf, 0, sizeof(sim800l_rx_buf));

	uint32_t start = HAL_GetTick();
	uint16_t idx = 0;

	while ((HAL_GetTick() - start) < timeout) {
		uint8_t ch;

		if (HAL_UART_Receive(sim800l_uart, &ch, 1, 20) == HAL_OK) {
			start = HAL_GetTick();

			if (idx < sizeof(sim800l_rx_buf) - 1) {
				sim800l_rx_buf[idx++] = ch;
				sim800l_rx_buf[idx] = '\0';
			}

			if (strstr(sim800l_rx_buf, "+HTTPACTION:"))
				return true;
		}
	}

	return false;
}

bool SIM800L_WaitForDownload(const char *cmd, uint32_t timeout) {
	SIM800L_FlushRx();

	HAL_UART_Transmit(sim800l_uart, (uint8_t*) cmd, strlen(cmd), 100);

	HAL_UART_Transmit(sim800l_uart, (uint8_t*) "\r\n", 2, 100);

	memset(sim800l_rx_buf, 0, sizeof(sim800l_rx_buf));

	uint32_t start = HAL_GetTick();
	uint16_t idx = 0;

	while ((HAL_GetTick() - start) < timeout) {
		uint8_t ch;

		if (HAL_UART_Receive(sim800l_uart, &ch, 1, 20) == HAL_OK) {
			start = HAL_GetTick();

			if (idx < sizeof(sim800l_rx_buf) - 1) {
				sim800l_rx_buf[idx++] = ch;
				sim800l_rx_buf[idx] = '\0';
			}

			if (strstr(sim800l_rx_buf, "DOWNLOAD"))
				return true;

			if (strstr(sim800l_rx_buf, "ERROR"))
				return false;
		}
	}

	return false;
}

bool SIM800L_Cloud_SendJson(const char *json, uint16_t len) {
	char cmd[128];

	/* HTTP Init */
	if (!SIM800L_SendCommand("AT+HTTPINIT", 3000))
		return false;

	/* Bearer */
	if (!SIM800L_SendCommand("AT+HTTPPARA=\"CID\",1", 3000))
		goto error;

	/* URL */
	sprintf(cmd,
			"AT+HTTPPARA=\"URL\",\"http://webhook.site/129418d6-d087-445e-b74c-1109dcf458db\"");

	if (!SIM800L_SendCommand(cmd, 3000))
		goto error;

	/* Content Type */
	if (!SIM800L_SendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"",
			3000)) {
		goto error;
	}

	/*-------------------------------------------------------
	 Upload JSON
	 -------------------------------------------------------*/

	sprintf(cmd, "AT+HTTPDATA=%u,10000", len);

	if (!SIM800L_WaitForDownload(cmd, 5000))
		goto error;

	if (!SIM800L_SendData((uint8_t*) json, len))
		goto error;

	/*-------------------------------------------------------
	 HTTP POST
	 -------------------------------------------------------*/

	if (!SIM800L_SendCommand("AT+HTTPACTION=1", 3000))
		goto error;

	if (!SIM800L_WaitForHTTPAction(30000))
		goto error;

	/* Check HTTP Status Code */

	int method, status, length;

	char *p = strstr(sim800l_rx_buf, "+HTTPACTION:");

	if (p == NULL)
		goto error;

	if (sscanf(p, "+HTTPACTION: %d,%d,%d", &method, &status, &length) != 3) {
		goto error;
	}

	if (status != 200 && status != 201) {
		goto error;
	}

	/*-------------------------------------------------------
	 Read Response (Optional)
	 -------------------------------------------------------*/

	SIM800L_SendCommand("AT+HTTPREAD", 5000);

	/*-------------------------------------------------------
	 Cleanup
	 -------------------------------------------------------*/

	SIM800L_SendCommand("AT+HTTPTERM", 3000);

	return true;

	error:

	SIM800L_SendCommand("AT+HTTPTERM", 3000);

	return false;
}
//bool SIM800L_Cloud_SendJson(const char *json, uint16_t len) {
//	char cmd[128];
//
//	if (!SIM800L_SendCommand("AT+HTTPINIT", "OK", 3000))
//		return false;
//
//	if (!SIM800L_SendCommand("AT+HTTPPARA=\"CID\",1", "OK", 3000))
//		return false;
//
//	sprintf(cmd,
//			"AT+HTTPPARA=\"URL\",\"http://webhook.site/416af966-9be4-4870-b7ca-ed2f161406c8\"");
//
//	if (!SIM800L_SendCommand(cmd, "OK", 3000))
//		return false;
//
//	if (!SIM800L_SendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"",
//			"OK", 3000))
//		return false;
//
//	sprintf(cmd, "AT+HTTPDATA=%u,10000", len);
//
//	/* Sends AT+HTTPDATA and waits for DOWNLOAD */
//	if (!SIM800L_SendCommand(cmd, "DOWNLOAD", 5000))
//		return false;
//
//	/* Send JSON payload */
//	if (!SIM800L_SendData((uint8_t*) json, len))
//		return false;
//
//	/* Wait for OK after upload */
//	if (!SIM800L_ReadResponse("OK", 10000))
//		return false;
//
//	if (!SIM800L_SendCommand("AT+HTTPACTION=1", "+HTTPACTION:", 30000))
//		return false;
//
//	SIM800L_SendCommand("AT+HTTPREAD", "OK", 5000);
//
//	SIM800L_SendCommand("AT+HTTPTERM", "OK", 3000);
//
//	return true;
//}

/**
 * @brief  Discard any stale bytes sitting in the UART before issuing a new command.
 */
static void SIM800L_FlushRx(void) {
	uint8_t dump;
	while (HAL_UART_Receive(sim800l_uart, &dump, 1U, 5U) == HAL_OK) {
		/* discard */
	}
}
