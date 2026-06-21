/*
 * genuine_rs485.h
 *
 *  Created on: May 20, 2026
 *      Author: jubaid_h
 */

#ifndef INC_GENUINE_RS485_H_
#define INC_GENUINE_RS485_H_

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* Configuration */
#define RS485_RX_BUFFER_SIZE      24
#define RS485_TX_TIMEOUT_MS       100
#define RS485_RX_TIMEOUT_MS       200

/* Device mode */

#define DISP_MODE_CONTROL    0x00
#define DISP_MODE_MONITOR    0x01

/* Protocol */
#define DISP_HEADER               0xA5

/* Function codes */
typedef enum {
	DISP_FUNC_READ = 0x03, DISP_FUNC_WRITE = 0x10, DISP_FUNC_EVENT = 0x0C

} DispFunction_t;

/* Origin address */
typedef enum {
	DISP_STATUS = 0x00,
	DISP_PRESET_LITER = 0x10,
	DISP_PRESET_SALE = 0x14,
	DISP_REALTIME = 0x20,
	DISP_CURRENT = 0x30, //Stopped data
	DISP_PRICE = 0x55,
	DISP_MODE = 0x5A,
	DISP_TOTAL = 0x60,
	DISP_CLASS_TOTAL = 0x70,
	DISP_OFFLINE = 0x80,
	DISP_READ_WRONG_CODE = 0x83,
	DISP_WRITE_WRONG_CODE = 0x90

} Disp_Origin_t;

/*Disp status response*/
typedef enum {
	DISP_STATUS_IDLE = 0x00, DISP_STATUS_START_BY_VOLUME = 0x01, // start as litre
	DISP_STATUS_START_BY_SALE = 0x02,   // start as sale
	DISP_STATUS_STOP_REQUIRED = 0x03,
	DISP_STATUS_BUSY = 0x04,
	DISP_STATUS_NOZZLE_NOT_RETURNED = 0x05,
	DISP_STATUS_AFTER_RESTART = 0x06

} DISP_Status_t;

/* Error codes returned by device when 0x83 happens*/
typedef enum {
	DISP_WRONG_CMD = 0x01,
	DISP_DEVICE_STOP_WORKING = 0x03,
	DISP_RESEND_COMMAND = 0x04,
	DISP_DEVICE_BUSY = 0x05,
	DISP_DEVICE_OFFLINE = 0x06,
	DISP_CRC_ERROR = 0x07

} DISP_ErrorCode_t;

typedef struct {
	UART_HandleTypeDef *uart;

	GPIO_TypeDef *dePort;
	uint16_t dePin;

	uint8_t rxBuf[RS485_RX_BUFFER_SIZE];
	uint16_t rxLen;
	volatile bool rxDone;

} RS485_Handle_t;

extern RS485_Handle_t dispenser;

/* Init */
void RS485_Init(RS485_Handle_t *h, UART_HandleTypeDef *uart,
		GPIO_TypeDef *dePort, uint16_t dePin);

/* TX/RX */
HAL_StatusTypeDef RS485_Send(RS485_Handle_t *h, uint8_t *data, uint16_t len);

void RS485_StartReceive(RS485_Handle_t *h);

void RS485_RxCallback(RS485_Handle_t *h);

/* Packet helpers */
uint16_t Disp_BuildRead(uint8_t addr, uint8_t origin, uint8_t len,
		uint8_t *txBuf);

uint16_t Disp_BuildWrite(uint8_t addr, uint8_t origin, uint8_t dataLen,
		uint8_t *payload, uint8_t *txBuf);

/* Parser */
bool Disp_ParsePacket(uint8_t *buf, uint16_t len);

/* CRC */
uint8_t Disp_CRC8(uint8_t *buf, uint16_t len);

/* Set Dispenser mode*/
bool Disp_SetMode(uint8_t mode);

#endif /* INC_GENUINE_RS485_H_ */
