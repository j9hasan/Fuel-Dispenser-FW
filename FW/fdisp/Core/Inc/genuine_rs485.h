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
#define RS485_TX_TIMEOUT_MS       200
#define RS485_RX_TIMEOUT_MS 50

#define DISP_POLL_RATE 50
#define RS485_WAIT_AFTER_SEND 50
/* Device mode */

#define DISP_MODE_CONTROL    0x00
#define DISP_MODE_MONITOR    0x01

/* Protocol */
#define DISP_HEADER               0xA5

/* Function codes */
#define DISP_FUNC_READ 0x03U
#define	DISP_FUNC_WRITE  0x10U
#define	DISP_FUNC_EVENT  0x0CU

/* Data Length */
#define DISP_LEN_REALTIME          0x08U
#define DISP_LEN_STOPPED           0x08U
#define DISP_LEN_STATUS        0x01U
#define DISP_LEN_SET_UNIT_PRICE    0x03U
#define DISP_LEN_CHECK_UNIT_PRICE  0x04U
#define DISP_LEN_ACCUMULATED_SUM  0x0CU
#define DISP_LEN_OFFLINE_COUNT  0x0EU
#define DISP_LEN_OFFLINE_DATA  0x08U

typedef enum {
	DISP_DISCONNECTED, DISP_CONNECTED
} DISP_CommState_t;

typedef struct {
	float volume;     // Liters
	float sale;       // Money amount
	bool valid;
} DISP_OfflineRecord_t;

/* Origin address */
typedef enum {
	DISP_ORIGIN_STATUS = 0x00,
	DISP_ORIGIN_PRESET_LITER = 0x10,
	DISP_ORIGIN_PRESET_SALE = 0x14,
	DISP_ORIGIN_REALTIME = 0x20,
	DISP_ORIGIN_CURRENT = 0x30, //Stopped data
	DISP_ORIGIN_PRICE = 0x55,
	DISP_ORIGIN_MODE = 0x5A,
	DISP_ORIGIN_TOTAL = 0x60,
	DISP_ORIGIN_CLASS_TOTAL = 0x70,
	DISP_ORIGIN_OFFLINE = 0x80,
	DISP_ORIGIN_READ_WRONG_CODE = 0x83,
	DISP_ORIGIN_WRITE_WRONG_CODE = 0x90
} Disp_Origin_t;

/* Disp status response */
typedef enum {
	DISP_STATUS_IDLE = 0x00, // Nozzle idle
	DISP_STATUS_START_BY_VOLUME = 0x01, // start as litre
	DISP_STATUS_START_BY_SALE = 0x02,   // start as sale
	DISP_STATUS_STOPPED_FUELING = 0x03, // Status after Stop fueling
	DISP_STATUS_BUSY = 0x04, // Fueling
	DISP_STATUS_NOZZLE_NOT_RETURNED = 0x05,
	DISP_STATUS_AFTER_RESTART = 0x06, // Nozzle offline
	DISP_STATUS_AFTER_NO_RESP = 0xFF // dispenser offline
} DISP_Status_t;

/* Error codes returned by device when 0x8X happens*/
typedef enum {
	DISP_OK = 0x00,
	/* Device errors (0x8X response) */
	DISP_WRONG_CMD = 0x01,
	DISP_DEVICE_STOP_WORKING = 0x03,
	DISP_RESEND_COMMAND = 0x04,
	DISP_DEVICE_BUSY = 0x05,
	DISP_DEVICE_OFFLINE = 0x06,
	DISP_CRC_ERROR = 0x07,

	/* Local communication errors */
	DISP_RS485_SEND_ERROR = 0x80,
	DISP_RS485_RX_TIMEOUT = 0x81,
	DISP_RS485_FRAME_ERROR = 0x82,
	DISP_RS485_START_REC_ERROR = 0x83,
	DISP_RS485_TRANSACTION_ERROR = 0x84
} DISP_ErrorCode_t;

typedef struct {
	UART_HandleTypeDef *uart;

	GPIO_TypeDef *dePort;
	uint16_t dePin;

	uint8_t rxBuf[RS485_RX_BUFFER_SIZE];
	volatile uint16_t rxLen;
	volatile bool rxDone;

} RS485_Handle_t;

extern RS485_Handle_t dispenser;

/* Init */
void RS485_Init(RS485_Handle_t *h, UART_HandleTypeDef *uart,
		GPIO_TypeDef *dePort, uint16_t dePin);

/* TX/RX */
HAL_StatusTypeDef RS485_Send(RS485_Handle_t *h, uint8_t *data, uint16_t len);

HAL_StatusTypeDef RS485_StartReceive(RS485_Handle_t *h);

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

/* Get device status */
DISP_ErrorCode_t Disp_ReadStatus(DISP_Status_t *status);
/* Get offline records */
DISP_ErrorCode_t Disp_CheckOfflineFuelingCount(int16_t *count);

uint16_t Disp_BuildEventRead(uint8_t addr, uint16_t recordIndex, uint8_t len,
		uint8_t *txBuf);
DISP_ErrorCode_t Disp_GetOfflineFuelingRecord(uint16_t recordNumber,
		float *volume, float *sale);
DISP_ErrorCode_t Disp_GetAccumulatedData(float *volume, float *sale);
DISP_ErrorCode_t Disp_GetDataWhenStopWorking(float *volume, float *sale);
DISP_CommState_t Disp_CheckCommunication(uint8_t retries);
#endif /* INC_GENUINE_RS485_H_ */
