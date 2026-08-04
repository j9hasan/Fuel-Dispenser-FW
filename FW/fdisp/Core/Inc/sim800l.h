/**
 ******************************************************************************
 * @file           : sim800l.h
 * @brief          : Minimal SIM800L GSM/GPRS module driver for connectivity
 *                    testing over a HAL UART (USART1 by default).
 ******************************************************************************
 * Wiring:
 *   SIM800L TXD -> MCU RX (USART1_RX)
 *   SIM800L RXD -> MCU TX (USART1_TX)   (use a level shifter / voltage divider,
 *                                        SIM800L logic is 3.3V-tolerant on RX
 *                                        but check your specific board)
 *   SIM800L GND -> MCU GND (common ground, mandatory)
 *   SIM800L VCC -> separate 3.7V-4.2V supply able to source ~2A peak
 *                  (do NOT power it from the MCU board's 3.3V regulator)
 *
 * Usage:
 *   1. Call SIM800L_Init() once, after MX_USART1_UART_Init().
 *   2. Call SIM800L_ConnectInternet() to run a basic AT handshake and report
 *      whether the module responds, is unlocked (SIM present), and is
 *      registered on the network.
 ******************************************************************************
 */

#ifndef __SIM800L_H
#define __SIM800L_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define SIM800L_HTTP_URL "http://webhook.site/6eaeba4c-d4c9-49ab-bbe7-9a6021d5684b"

#define SIM800L_STARTUP_TIMEOUT_MS    60000U
#define SIM800L_RETRY_INTERVAL_MS      2000U

/* Exported types ------------------------------------------------------------*/
typedef enum {
	SIM800L_OK = 0x00U, /* Module responded, registered, and GPRS-attached */
	SIM800L_ERROR_NO_RESP = 0x01U, /* No response to "AT" (power/wiring/baud issue) */
	SIM800L_ERROR_NO_SIM = 0x02U, /* Module responds but SIM not detected/locked  */
	SIM800L_ERROR_NO_NET = 0x03U, /* SIM ok but not registered on the network (offline) */
	SIM800L_ERROR_TIMEOUT = 0x04U, /* UART timeout communicating with the module   */
	SIM800L_ERROR_NO_DATA = 0x05U, /* Registered on network, but GPRS attach failed
	 (commonly seen when a data plan/balance is
	 exhausted or data service isn't provisioned) */
	SIM800L_INITIALIZING = 0x99U, /* No test has completed yet (default/startup value) */
	SIM800L_ERROR_GPRS = 0x06U
} SIM800L_StatusTypeDef;

extern bool deviceOffline;

/* Exported functions prototypes ----------------------------------------------*/
SIM800L_StatusTypeDef SIM800L_Initialize(UART_HandleTypeDef *huart,
		uint32_t timeout_ms);
SIM800L_StatusTypeDef SIM800L_ConnectInternet(void);
bool SIM800L_SendCommand(const char *cmd, const char *expected,
		uint32_t timeout_ms);
int8_t SIM800L_GetSignalQuality(void);
uint8_t SIM800L_CheckGPRSAttach(void);
SIM800L_StatusTypeDef SIM800L_OpenBearer(void);
bool SIM800L_Cloud_SendJson(const char *json);
bool SIM800L_IsInternetConnected(void);

//bool TimeAPI_Get(char *response, uint16_t maxLen);


#ifdef __cplusplus
}
#endif

#endif /* __SIM800L_H */
