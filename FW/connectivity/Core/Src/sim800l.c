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

/**
 * @brief  Bind the driver to the UART handle used to talk to the module.
 * @param  huart Pointer to an already-initialized UART_HandleTypeDef (e.g. &huart1)
 * @retval None
 */
void SIM800L_Init(UART_HandleTypeDef *huart) {
	sim800l_uart = huart;
}

/**
 * @brief  Send an AT command and check whether a given substring shows up
 *         in the module's response within the timeout window.
 * @param  cmd       Command string WITHOUT trailing \r\n (it is appended here)
 * @param  expected  Substring to look for in the response, e.g. "OK"
 * @param  timeout_ms Max time to wait for the response
 * @retval 1 if expected substring found, 0 otherwise
 */
uint8_t SIM800L_SendCommand(const char *cmd, const char *expected,
		uint32_t timeout_ms) {
	if (sim800l_uart == NULL) {
		return 0U;
	}

	SIM800L_FlushRx();
	memset(sim800l_rx_buf, 0, sizeof(sim800l_rx_buf));

	HAL_UART_Transmit(sim800l_uart, (uint8_t*) cmd, (uint16_t) strlen(cmd),
			100);
	HAL_UART_Transmit(sim800l_uart, (uint8_t*) "\r\n", 2, 100);

	/* Poll byte-by-byte so a short/garbled response doesn't block forever */
	uint32_t start = HAL_GetTick();
	uint16_t idx = 0U;
	while ((HAL_GetTick() - start) < timeout_ms
			&& idx < (sizeof(sim800l_rx_buf) - 1U)) {
		uint8_t byte;
		if (HAL_UART_Receive(sim800l_uart, &byte, 1U, 20U) == HAL_OK) {
			sim800l_rx_buf[idx++] = (char) byte;
			if (expected != NULL && strstr(sim800l_rx_buf, expected) != NULL) {
				return 1U;
			}
		}
	}

	return (expected != NULL && strstr(sim800l_rx_buf, expected) != NULL) ?
			1U : 0U;
}

/**
 * @brief  Run a basic connectivity test:
 *           1) "AT"        -> expects "OK"      (module alive / baud correct)
 *           2) "AT+CPIN?"  -> expects "READY"   (SIM present & unlocked)
 *           3) "AT+CREG?"  -> expects ",1" or ",5" (registered home/roaming -
 *                             this is voice/SMS registration; if this fails
 *                             the module is effectively offline)
 *           4) "AT+CGATT?" -> expects "+CGATT: 1" (GPRS/data attach - this is
 *                             the step that commonly fails when a data plan
 *                             or balance has run out, even though step 3
 *                             (network registration) still succeeds)
 * @retval SIM800L_StatusTypeDef result code
 */
/**
 * @brief  Verify SIM800L is online and GPRS bearer is ready.
 *         If the bearer is closed, it is automatically reopened.
 *
 * @retval SIM800L_OK              Module is ready for HTTP.
 * @retval SIM800L_ERROR_NO_RESP   Module not responding.
 * @retval SIM800L_ERROR_NO_SIM    SIM missing or locked.
 * @retval SIM800L_ERROR_NO_NET    GSM network unavailable.
 * @retval SIM800L_ERROR_NO_DATA   GPRS not attached.
 * @retval SIM800L_ERROR_GPRS      Failed to open bearer.
 */
SIM800L_StatusTypeDef SIM800L_TestConnection(void) {
	static uint8_t bearerOpened = 0;

	if (sim800l_uart == NULL) {
		bearerOpened = 0;
		return SIM800L_ERROR_TIMEOUT;
	}

	/*----------------------------------------------------------
	 1. Module Alive
	 ----------------------------------------------------------*/
	uint8_t alive = 0;

	for (uint8_t i = 0; i < 3; i++) {
		if (SIM800L_SendCommand("AT", "OK", 1000)) {
			alive = 1;
			break;
		}

		HAL_Delay(500);
	}

	if (!alive) {
		bearerOpened = 0;
		return SIM800L_ERROR_NO_RESP;
	}

	/*----------------------------------------------------------
	 2. SIM Ready
	 ----------------------------------------------------------*/
	if (!SIM800L_SendCommand("AT+CPIN?", "READY", 2000)) {
		bearerOpened = 0;
		return SIM800L_ERROR_NO_SIM;
	}

	/*----------------------------------------------------------
	 3. Network Registered
	 ----------------------------------------------------------*/
	if (!SIM800L_SendCommand("AT+CREG?", ",1", 2000)
			&& !SIM800L_SendCommand("AT+CREG?", ",5", 2000)) {
		bearerOpened = 0;
		return SIM800L_ERROR_NO_NET;
	}

	/*----------------------------------------------------------
	 4. GPRS Attached
	 ----------------------------------------------------------*/
	uint8_t attached = 0;

	for (uint8_t i = 0; i < 3; i++) {
		if (SIM800L_CheckGPRSAttach()) {
			attached = 1;
			break;
		}

		HAL_Delay(2000);
	}

	if (!attached) {
		bearerOpened = 0;
		return SIM800L_ERROR_NO_DATA;
	}

	/*----------------------------------------------------------
	 5. Open bearer only if needed
	 ----------------------------------------------------------*/
	if (!bearerOpened) {
		SIM800L_StatusTypeDef status = SIM800L_OpenBearer();

		if (status != SIM800L_OK) {
			bearerOpened = 0;
			return status;
		}

		bearerOpened = 1;
	}

	return SIM800L_OK;
}

/**
 * @brief  Check whether the module is attached to the GPRS/data network.
 *         Registered-on-network-but-not-attached is the usual pattern when
 *         a SIM's data plan/balance has run out (voice/SMS may still work).
 * @retval 1 if attached ("+CGATT: 1" seen), 0 otherwise
 */
uint8_t SIM800L_CheckGPRSAttach(void) {
	return SIM800L_SendCommand("AT+CGATT?", "+CGATT: 1", 5000U);
}
void SIM800L_CloseBearer(void) {
	bearerOpened = 0;
}
SIM800L_StatusTypeDef SIM800L_OpenBearer(void) {
	/* Close previous bearer if open */
	SIM800L_SendCommand("AT+SAPBR=0,1", "OK", 5000);

	/* Configure bearer */
	if (!SIM800L_SendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", 3000))
		return SIM800L_ERROR_GPRS;

	/* Airtel/Robi Bangladesh APN */
	if (!SIM800L_SendCommand("AT+SAPBR=3,1,\"APN\",\"internet\"", "OK", 3000))
		return SIM800L_ERROR_GPRS;

	/* Open bearer */
	if (!SIM800L_SendCommand("AT+SAPBR=1,1", "OK", 15000))
		return SIM800L_ERROR_GPRS;

	/* Verify IP address */
	if (!SIM800L_SendCommand("AT+SAPBR=2,1", "+SAPBR: 1,1", 5000))
		return SIM800L_ERROR_GPRS;

	return SIM800L_OK;
}
/**
 * @brief  Query signal quality via AT+CSQ.
 * @retval RSSI index 0-31 (higher is better, 99 = unknown mapped to -1 on error)
 */
int8_t SIM800L_GetSignalQuality(void) {
	if (!SIM800L_SendCommand("AT+CSQ", "OK", 2000U)) {
		return -1;
	}

	char *p = strstr(sim800l_rx_buf, "+CSQ:");
	if (p == NULL) {
		return -1;
	}

	int rssi = -1;
	sscanf(p, "+CSQ: %d", &rssi);
	return (int8_t) rssi;
}

bool SIM800L_Cloud_SendJson(const char *json) {
	char cmd[128];

	/* Initialize HTTP */
	SIM800L_SendCommand("AT+HTTPINIT\r\n", "OK", 3000);

	/* Use GPRS bearer */
	SIM800L_SendCommand("AT+HTTPPARA=\"CID\",1\r\n", "OK", 3000);

	/* URL */
	sprintf(cmd,
			"AT+HTTPPARA=\"URL\",\"http://webhook.site/416af966-9be4-4870-b7ca-ed2f161406c8\"\r\n");
	SIM800L_SendCommand(cmd, "OK", 3000);

	/* Content type */
	SIM800L_SendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n",
			"OK", 3000);

	/* Tell SIM800 how many bytes are coming */
	sprintf(cmd, "AT+HTTPDATA=%d,10000\r\n", strlen(json));

	if (!SIM800L_SendCommand(cmd, "DOWNLOAD", 3000))
		return false;

	/* Send JSON */
	if (!SIM800L_SendCommand(json, "OK", 10000))
		return false;

	/* HTTP POST */
	if (!SIM800L_SendCommand("AT+HTTPACTION=1\r\n", "+HTTPACTION:", 20000))
		return false;

	/* Read server response (optional) */
	SIM800L_SendCommand("AT+HTTPREAD\r\n", "OK", 5000);

	/* Close HTTP */
	SIM800L_SendCommand("AT+HTTPTERM\r\n", "OK", 3000);

	return true;
}

/**
 * @brief  Discard any stale bytes sitting in the UART before issuing a new command.
 */
static void SIM800L_FlushRx(void) {
	uint8_t dump;
	while (HAL_UART_Receive(sim800l_uart, &dump, 1U, 5U) == HAL_OK) {
		/* discard */
	}
}
