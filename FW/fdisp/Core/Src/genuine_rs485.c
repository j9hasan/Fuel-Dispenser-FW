/*
 * genuine_rs485.c
 *
 *  Created on: May 20, 2026
 *      Author: jubaid_h
 */

#include "genuine_rs485.h"
#include <string.h>

static void RS485_TXEnable(RS485_Handle_t *h) {
	HAL_GPIO_WritePin(h->dePort, h->dePin, GPIO_PIN_SET);
}

static void RS485_RXEnable(RS485_Handle_t *h) {
	HAL_GPIO_WritePin(h->dePort, h->dePin, GPIO_PIN_RESET);
}

void RS485_Init(RS485_Handle_t *h, UART_HandleTypeDef *uart,
		GPIO_TypeDef *dePort, uint16_t dePin) {
	memset(h, 0, sizeof(RS485_Handle_t));

	h->uart = uart;
	h->dePort = dePort;
	h->dePin = dePin;

	RS485_RXEnable(h);

	RS485_StartReceive(h);
}

HAL_StatusTypeDef RS485_Send(RS485_Handle_t *h, uint8_t *data, uint16_t len) {
	HAL_StatusTypeDef ret;

	RS485_TXEnable(h);

	ret = HAL_UART_Transmit(h->uart, data, len,
	RS485_TX_TIMEOUT_MS);

	while (__HAL_UART_GET_FLAG(
			h->uart,
			UART_FLAG_TC) == RESET)
		;

	RS485_RXEnable(h);

	return ret;
}

void RS485_StartReceive(RS485_Handle_t *h) {
	HAL_UART_Receive_IT(h->uart, &h->rxByte, 1);
}

//echoes back 12 bytes
void RS485_RxCallback(RS485_Handle_t *h) {
	if (h->rxIndex < RS485_RX_BUFFER_SIZE) {
		h->rxBuf[h->rxIndex++] = h->rxByte;
	} else {
		h->rxDone = true;     // buffer full
		h->rxIndex = 0;
	}

	HAL_UART_Receive_IT(h->uart, &h->rxByte, 1);
}

uint16_t Disp_BuildRead(uint8_t addr, uint8_t origin, uint8_t len,
		uint8_t *txBuf) {
	txBuf[0] = DISP_HEADER;
	txBuf[1] = addr;
	txBuf[2] = DISP_FUNC_READ;
	txBuf[3] = origin;
	txBuf[4] = len;

	txBuf[5] = Disp_CRC8(&txBuf[1], 4);

	return 6;
}

uint16_t Disp_BuildWrite(uint8_t addr, uint8_t origin, uint8_t dataLen,
		uint8_t *payload, uint8_t *txBuf) {
	uint16_t i;

	txBuf[0] = DISP_HEADER;
	txBuf[1] = addr;
	txBuf[2] = DISP_FUNC_WRITE;
	txBuf[3] = origin;
	txBuf[4] = dataLen;

	for (i = 0; i < dataLen; i++) {
		txBuf[5 + i] = payload[i];
	}

	txBuf[5 + dataLen] = Disp_CRC8(&txBuf[1], dataLen + 4);

	return dataLen + 6;
}

bool Disp_ParsePacket(uint8_t *buf, uint16_t len) {
	uint8_t crc;

	if (len < 5)
		return false;

	if (buf[0] != DISP_HEADER)
		return false;

	crc = Disp_CRC8(&buf[1], len - 2);

	if (crc != buf[len - 1])
		return false;

	return true;
}

uint8_t Disp_CRC8(uint8_t *buf, uint16_t len) {
	uint8_t crc = 0;

	while (len--) {
		crc ^= *buf++;
	}

	return crc;
}

