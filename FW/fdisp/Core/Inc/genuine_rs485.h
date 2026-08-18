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
#define RS485_RX_BUFFER_SIZE      36
#define RS485_TX_TIMEOUT_MS       200
#define RS485_RX_TIMEOUT_MS 50

#define DISP_POLL_RATE 200
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
	DISP_STATUS_AFTER_RESTART = 0x06 // Nozzle offline
//	DISP_STATUS_SENDING = 0xFF // dispenser offline
} DISP_Status_t;

/* Error codes returned by device and mcu*/
typedef enum {
	DISP_OK                            = 0x00,
	/* Device errors (0x8X response) */
	DISP_WRONG_CMD                     = 0x01,
	DISP_DEVICE_STOP_WORKING           = 0x03,
	DISP_RESEND_COMMAND                = 0x04,
	DISP_DEVICE_BUSY                   = 0x05,
	DISP_DEVICE_OFFLINE                = 0x06,
	DISP_CRC_ERROR                     = 0x07,

	/* Local communication errors */
	DISP_RS485_UART_DMA_ERROR          = 0x70,
	DISP_RS485_UART_RX_TIMEOUT         = 0x71,
	DISP_RS485_UART_TX_TIMEOUT         = 0x72,
	DISP_RS485_UART_RX_OVERRUN         = 0x73,
	DISP_RS485_UART_RX_FRAMING         = 0x74,
	DISP_RS485_UART_RX_NOISE           = 0x75,
	DISP_RS485_UART_RX_PARITY          = 0x76,
	DISP_RS485_UART_RX_BUSY            = 0x77,
	DISP_RS485_UART_RX_UNKNOWN_STATE   = 0x78,
	DISP_RS485_UART_RX_UNEXPECTED_EVENT= 0x79,
	DISP_PROTOCOL_INVALID_LENGTH       = 0x80,
	DISP_PROTOCOL_PARSE_ERROR          = 0x90,
	DISP_ERROR                         = 0x91,
	DISP_NULL_POINTER                  = 0x92,

} DISP_ErrorCode_t;

typedef enum
{
    DISP_PARSE_OK = 0x00,
	DISP_PARSE_INFO_WRONG,
    DISP_PARSE_NULL_BUFFER,
    DISP_PARSE_TOO_SHORT,
    DISP_PARSE_LENGTH_MISMATCH,
    DISP_PARSE_INVALID_HEADER,
    DISP_PARSE_CRC_ERROR

} DISP_ParseError_t;

/* Init */
void RS485_Init(UART_HandleTypeDef *uart,
		GPIO_TypeDef *dePort, uint16_t dePin);

/* Set Dispenser mode*/
bool Disp_SetMode(uint8_t mode);

/* Get device status */
DISP_ErrorCode_t Disp_ReadStatus(DISP_Status_t *status);
/* Get offline records */
DISP_ErrorCode_t Disp_CheckOfflineFuelingCount(int16_t *count);

DISP_ErrorCode_t Disp_GetOfflineFuelingRecord(uint16_t recordNumber,
		float *volume, float *sale);
DISP_ErrorCode_t Disp_GetAccumulatedData(float *volume, float *sale);
DISP_ErrorCode_t Disp_GetDataWhenStopWorking(float *volume, float *sale);
DISP_CommState_t Disp_CheckCommunication(uint8_t retries);
#endif /* INC_GENUINE_RS485_H_ */
