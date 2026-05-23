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
#define RS485_RX_BUFFER_SIZE      124
#define RS485_TX_TIMEOUT_MS       100
#define RS485_RX_TIMEOUT_MS       200

/* Packet offsets */
#define PKT_HEADER_IDX            0
#define PKT_ADDR_IDX              1
#define PKT_FUNC_IDX              2
#define PKT_LEN_IDX               3

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
	DISP_CURRENT = 0x30,
	DISP_PRICE = 0x55,
	DISP_MODE = 0x5A,
	DISP_TOTAL = 0x60,
	DISP_CLASS_TOTAL = 0x70,
	DISP_OFFLINE = 0x80

} DispOrigin_t;

typedef struct {
	UART_HandleTypeDef *uart;

	GPIO_TypeDef *dePort;
	uint16_t dePin;

	uint8_t rxBuf[RS485_RX_BUFFER_SIZE];
	uint16_t rxLen;
	volatile bool rxDone;

} RS485_Handle_t;

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

/* Read Data */
//void Disp_GetFuelingRealtime(void);
//void Disp_ReadStatus(void);

#endif /* INC_GENUINE_RS485_H_ */
