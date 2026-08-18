/*
 * genuine_rs485.c
 *
 *  Created on: May 20, 2026
 *      Author: jubaid_h
 */

#include "genuine_rs485.h"
#include <string.h>


static void RS485_TXEnable(void);
static void RS485_RXEnable(void);
static DISP_ErrorCode_t Disp_Transaction(uint8_t *txBuf,
                                         uint16_t txLen,
                                         uint8_t *response,
                                         uint16_t expectedRxLen,
                                         uint32_t txTimeout,
										 uint32_t rxTimeout);

static uint16_t Disp_BuildCustomCommand(const uint8_t *payload, uint8_t payloadLen,
		uint8_t *txBuf);
static uint16_t Disp_BuildRead(uint8_t addr, uint8_t origin, uint8_t len,
		uint8_t *txBuf) ;
uint16_t Disp_BuildWrite(uint8_t addr, uint8_t origin, uint8_t dataLen,
		uint8_t *payload, uint8_t *txBuf);
static uint8_t Disp_DecToBCD(uint8_t val);
static uint16_t Disp_BuildEventRead(uint8_t addr, uint16_t recordIndex, uint8_t len,
		uint8_t *txBuf);
static uint8_t Disp_CRC8(uint8_t *buf, uint16_t len);

static uint8_t Disp_BCDToDec(uint8_t bcd);
static DISP_ParseError_t Disp_ParsePacket(uint8_t *buf, uint16_t len);
static void Disp_UART_FlushRx(void);
static DISP_ErrorCode_t Disp_MapUartError(uint32_t error);
static HAL_StatusTypeDef Disp_RxStop(void);


static UART_HandleTypeDef *rs485_uart;
static GPIO_TypeDef *rs485_dePort;
static uint16_t rs485_dePin;

void RS485_Init(UART_HandleTypeDef *uart, GPIO_TypeDef *dePort, uint16_t dePin) {

	rs485_uart  = uart;
	rs485_dePort = dePort;
	rs485_dePin = dePin;

}

/**
 * @brief Check communication with the rs485_handle.
 *
 * Attempts to read the rs485_handle status multiple times. If any
 * attempt succeeds, the rs485_handle is considered connected.
 *
 * @param retries Number of retry attempts.
 *
 * @retval DISP_CONNECTED
 * @retval DISP_DISCONNECTED
 */
DISP_CommState_t Disp_CheckCommunication(uint8_t retries)
{
    DISP_Status_t dispStatus;
    DISP_ErrorCode_t dispErr;

    for (uint8_t i = 0U; i < retries; i++)
    {
    	dispErr = Disp_ReadStatus(&dispStatus);

        if (dispErr == DISP_OK)
        {
            return DISP_CONNECTED;
        }

        HAL_Delay(300);
    }

    return DISP_DISCONNECTED;
}

/**
 * @brief Requests the current machine status.
 * Master: A5 | addr | 03 | 00 | len | CRC8
 * @return DISP_Status_t
 */
DISP_ErrorCode_t Disp_ReadStatus(DISP_Status_t *status) {

	uint8_t cmdBuffer[6];
	uint16_t cmdLen;

	if (status == NULL) {
		return 	DISP_NULL_POINTER;
	}

	*status = (DISP_Status_t)DISP_ERROR;

	cmdLen = Disp_BuildRead(0x01, DISP_ORIGIN_STATUS, DISP_LEN_STATUS, cmdBuffer);

    uint8_t rxBufferLen = 6;
    uint8_t rxBuffer[rxBufferLen];
	uint16_t txTimeout = 100;
	uint16_t rxTimeout = 500;

	DISP_ErrorCode_t err = Disp_Transaction(cmdBuffer, cmdLen, rxBuffer, rxBufferLen, txTimeout, rxTimeout);

	if (err != DISP_OK)
		return err;

	DISP_ParseError_t perr = Disp_ParsePacket(rxBuffer, rxBufferLen);

	if (perr != DISP_PARSE_OK)
	{
	    if (perr == DISP_PARSE_INFO_WRONG)
	    {
	        *status = (DISP_Status_t)rxBuffer[3];
	    }
	    return DISP_PROTOCOL_PARSE_ERROR;
	}

	*status = (DISP_Status_t) rxBuffer[4]; // status if success

	return DISP_OK;
}

typedef enum
{
    DISP_RX_IDLE = 0,
    DISP_RX_ACTIVE
} DISP_RxState_t;

static volatile DISP_RxState_t rxState = DISP_RX_IDLE;

static volatile uint8_t rxComplete = 0;
static volatile uint32_t rxError = HAL_UART_ERROR_NONE;

static uint8_t rxBuf[64];

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == rs485_uart)
    {
        rxComplete = 1;
    }
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == rs485_uart)
    {
        rxError = HAL_UART_GetError(huart);
        rxComplete = 1;
    }
}

static DISP_ErrorCode_t Disp_Transaction(uint8_t *txBuf,
                                         uint16_t txLen,
                                         uint8_t *response,
                                         uint16_t expectedRxLen,
                                         uint32_t txTimeout,
										 uint32_t rxTimeout)
{
    HAL_StatusTypeDef status;
    uint32_t startTick;

    /* Basic argument validation */
	if ((txBuf == NULL) || (response == NULL) || (txLen == 0)
			|| (expectedRxLen == 0) || (expectedRxLen > sizeof(rxBuf))) {
		return DISP_PROTOCOL_INVALID_LENGTH;
	}


    /* ---------------------------------------------------------
     * 1. Check driver RX state
     * --------------------------------------------------------- */

    if (rxState != DISP_RX_IDLE)
    {
        return DISP_RS485_UART_RX_BUSY;
    }

    /* ---------------------------------------------------------
     * 2. Reset transaction variables
     * --------------------------------------------------------- */
    rxComplete = 0;
    rxError = HAL_UART_ERROR_NONE;

    /* ---------------------------------------------------------
     * 1. Stop any previous RX transaction
     * --------------------------------------------------------- */

    /* Clear previous UART errors */
    __HAL_UART_CLEAR_OREFLAG(rs485_uart);
    __HAL_UART_CLEAR_FEFLAG(rs485_uart);
    __HAL_UART_CLEAR_NEFLAG(rs485_uart);
    __HAL_UART_CLEAR_PEFLAG(rs485_uart);

    /* ---------------------------------------------------------
     * 2. Start RX DMA
     *
     * We already know exactly how many bytes are expected.
     * Therefore use normal DMA.
     * --------------------------------------------------------- */

    status = HAL_UART_Receive_DMA(rs485_uart,
                                  rxBuf,
                                  expectedRxLen);

    if (status != HAL_OK)
    {
        if (status == HAL_BUSY)
        {
            return DISP_RS485_UART_RX_BUSY;
        }

        return DISP_RS485_UART_DMA_ERROR;
    }

    /* DMA successfully started */
    rxState = DISP_RX_ACTIVE;

    /* ---------------------------------------------------------
     * 3. Enable RS485 transmitter
     * --------------------------------------------------------- */

    RS485_TXEnable();

    /* ---------------------------------------------------------
     * 4. Transmit command
     * --------------------------------------------------------- */

    status = HAL_UART_Transmit(rs485_uart,
                               txBuf,
                               txLen,
                               txTimeout);

    if (status != HAL_OK)
    {
        RS485_RXEnable();

        Disp_RxStop();

        return DISP_RS485_UART_TX_TIMEOUT;
    }

    /* ---------------------------------------------------------
     * 5. Wait until final TX byte physically leaves UART
     * --------------------------------------------------------- */

    startTick = HAL_GetTick();

    while (__HAL_UART_GET_FLAG(rs485_uart, UART_FLAG_TC) == RESET)
    {
        if ((HAL_GetTick() - startTick) >= txTimeout)
        {
            RS485_RXEnable();

            Disp_RxStop();

            return DISP_RS485_UART_TX_TIMEOUT;
        }
    }

    /* ---------------------------------------------------------
     * 6. Switch RS485 transceiver to RX
     * --------------------------------------------------------- */

    RS485_RXEnable();

    /* ---------------------------------------------------------
     * 7. Wait for DMA RX complete
     * --------------------------------------------------------- */

    startTick = HAL_GetTick();

    while (!rxComplete)
    {
        if ((HAL_GetTick() - startTick) >= rxTimeout)
        {
        	Disp_RxStop();

            return DISP_RS485_UART_RX_TIMEOUT;
        }
    }

    /* ---------------------------------------------------------
     * 10. RX transaction has finished
     * --------------------------------------------------------- */

    rxState = DISP_RX_IDLE;

    /* ---------------------------------------------------------
     * 8. Check UART error
     * --------------------------------------------------------- */

    if (rxError != HAL_UART_ERROR_NONE)
    {
    	Disp_RxStop();

        return Disp_MapUartError(rxError);
    }

    /* ---------------------------------------------------------
     * 9. Copy received data
     * --------------------------------------------------------- */

    memcpy(response, rxBuf, expectedRxLen);

    return DISP_OK;
}
static HAL_StatusTypeDef Disp_RxStop(void)
{
    HAL_StatusTypeDef status = HAL_OK;

    if (rxState == DISP_RX_ACTIVE)
    {
        status = HAL_UART_AbortReceive(rs485_uart);
    }

    rxComplete = 0;
    rxError = HAL_UART_ERROR_NONE;
    rxState = DISP_RX_IDLE;

    return status;
}
static DISP_ErrorCode_t Disp_MapUartError(uint32_t error)
{
    if (error & HAL_UART_ERROR_ORE)
    {
        return DISP_RS485_UART_RX_OVERRUN;
    }

    if (error & HAL_UART_ERROR_FE)
    {
        return DISP_RS485_UART_RX_FRAMING;
    }

    if (error & HAL_UART_ERROR_NE)
    {
        return DISP_RS485_UART_RX_NOISE;
    }

    if (error & HAL_UART_ERROR_PE)
    {
        return DISP_RS485_UART_RX_PARITY;
    }

    return DISP_RS485_UART_DMA_ERROR;
}
/*
 * This function Builds read command according to following frame mentioned in datasheet
 * A5 = header
 * 01 = rs485_handle address
 * 03 = read
 * 00 = status register
 * 01 = read length
 * D2 = CRC
 */
static uint16_t Disp_BuildRead(uint8_t addr, uint8_t origin, uint8_t len,
		uint8_t *txBuf) {
	txBuf[0] = DISP_HEADER;
	txBuf[1] = addr;
	txBuf[2] = DISP_FUNC_READ;
	txBuf[3] = origin;
	txBuf[4] = len;

	txBuf[5] = Disp_CRC8(&txBuf[1], 4);

	return 6;
}

/*
 * Communication data packer format
 * Master: header + device address + function code + data origin + data length + crc
 */
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

/*
 * Build the 4.12 "read offline fueling record" frame.
 * Function code 0x0C, 2-byte BCD record index (1..2000), e.g. record
 * 2000 is sent as bytes 0x20 0x00.
 *
 * Master: A5 | addr | 0C | indexHiBCD | indexLoBCD | len | CRC8
 */
static uint16_t Disp_BuildEventRead(uint8_t addr, uint16_t recordIndex, uint8_t len,
		uint8_t *txBuf) {
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
 *
 * Data coming from uart DMA via idle detection interrupt
 * Packet format
 * Slave: header + device  address + function code + data length + data 0...N + crc
 * Response frame
 * A5    Header
 * 01    Dispenser address
 * 03    Read response
 * 01    Data length
 * 04    Status
 * CE    CRC
 */

static DISP_ParseError_t Disp_ParsePacket(uint8_t *buf, uint16_t len) {
	uint8_t crc;
	uint16_t expectedLen;

	if (buf == NULL)
		return DISP_PARSE_NULL_BUFFER;

	if (len < 5U)
		return DISP_PARSE_TOO_SHORT;

	expectedLen = (uint16_t) buf[3] + 5U;

	if (len != expectedLen)
		return DISP_PARSE_LENGTH_MISMATCH;

	if (buf[0] != DISP_HEADER)
		return DISP_PARSE_INVALID_HEADER;

	crc = Disp_CRC8(&buf[1], len - 2U);

	if (crc != buf[len - 1U])
		return DISP_PARSE_CRC_ERROR;

	if (buf[2] == 0x83 || buf[2] == 0x90)
	    return DISP_PARSE_INFO_WRONG;

	return DISP_PARSE_OK;
}

DISP_ErrorCode_t Disp_CheckOfflineFuelingCount(int16_t *count) {

	*count = 0;
	uint8_t cmdBuffer[6];
	uint16_t cmdLen;;
	uint8_t address = 0x01;

	cmdLen = Disp_BuildRead(address, DISP_ORIGIN_OFFLINE, DISP_LEN_OFFLINE_COUNT, cmdBuffer);


    uint8_t rxBufferLen = 19;
    uint8_t rxBuffer[rxBufferLen];
	uint16_t txTimeout = 150;
	uint16_t rxTimeout = 600;

	DISP_ErrorCode_t err = Disp_Transaction(cmdBuffer, cmdLen, rxBuffer, rxBufferLen, txTimeout, rxTimeout);

	if (err != DISP_OK)
		return err;

	DISP_ParseError_t perr = Disp_ParsePacket(rxBuffer, rxBufferLen);

	if (perr != DISP_PARSE_OK)
	{
	    if (perr == DISP_PARSE_INFO_WRONG)
	    {
			DISP_Status_t status = (DISP_Status_t) rxBuffer[3];
	    }
	    return DISP_PROTOCOL_PARSE_ERROR;
	}

	uint16_t bcd = ((uint16_t) rxBuffer[16] << 8) | rxBuffer[17];

	*count = ((bcd >> 12) & 0x0F) * 1000 + ((bcd >> 8) & 0x0F) * 100
			+ ((bcd >> 4) & 0x0F) * 10 + (bcd & 0x0F);

	return DISP_OK;
}

DISP_ErrorCode_t Disp_GetOfflineFuelingRecord(uint16_t recordIndex,
		float *volume, float *sale) {

	*volume = 0;
	*sale = 0;
	uint8_t cmdBuffer[6];
	uint16_t cmdLen;;
	uint8_t address = 0x01;

	cmdLen = Disp_BuildEventRead(address, recordIndex, DISP_LEN_OFFLINE_DATA, cmdBuffer);


    uint8_t rxBufferLen = 12;
    uint8_t rxBuffer[rxBufferLen];
	uint16_t txTimeout = 120;
	uint16_t rxTimeout = 550;

	DISP_ErrorCode_t err = Disp_Transaction(cmdBuffer, cmdLen, rxBuffer, rxBufferLen, txTimeout, rxTimeout);

	if (err != DISP_OK)
		return err;

	DISP_ParseError_t perr = Disp_ParsePacket(rxBuffer, rxBufferLen);

	if (perr != DISP_PARSE_OK)
	{
	    if (perr == DISP_PARSE_INFO_WRONG)
	    {
	    	DISP_Status_t status = (DISP_Status_t)rxBuffer[3];
	    }
	    return DISP_PROTOCOL_PARSE_ERROR;
	}
	/* Decode BCD data */
	uint32_t vol = Disp_BCDToDec(rxBuffer[4]) * 10000
			+ Disp_BCDToDec(rxBuffer[5]) * 100
			+ Disp_BCDToDec(rxBuffer[6]);

	uint64_t sal = (uint64_t) Disp_BCDToDec(rxBuffer[7]) * 100000000ULL
			+ (uint64_t) Disp_BCDToDec(rxBuffer[8]) * 1000000ULL
			+ (uint64_t) Disp_BCDToDec(rxBuffer[9]) * 10000ULL
			+ (uint64_t) Disp_BCDToDec(rxBuffer[10]) * 100ULL
			+ (uint64_t) Disp_BCDToDec(rxBuffer[11]);

	*volume = vol / 100.0f;
	*sale = sal / 100.0f;

	return DISP_OK;
}

//DISP_ErrorCode_t Disp_GetAccumulatedData(float *volume, float *sale) {
//
//	uint8_t buf[16];
//	uint16_t txLen;
//
//	*volume = 0.0f;
//	*sale = 0.0f;
//
//	memset(buf, 0, sizeof(buf));
//
//	/* Read function 0x03, address 0x60, length 0x0C */
//	txLen = Disp_BuildRead(0x01, 0x60, 0x0C, buf);
//
//	DISP_ErrorCode_t err;
//
//	err = Disp_Transaction(buf, txLen);
//
//	if (err != DISP_OK)
//		return err;
//
//	err = Disp_CheckResponse();
//
//	if (err != DISP_OK)
//		return err;
//
//	/* Expected response:
//	 * A5 01 03 0C
//	 * 00 00 00 58 37
//	 * 00 00 00 12 34 56 78
//	 * CRC
//	 */
//
//	uint64_t vol = (uint64_t) Disp_BCDToDec(rxBuffer[4]) * 100000000ULL
//			+ (uint64_t) Disp_BCDToDec(rxBuffer[5]) * 1000000ULL
//			+ (uint64_t) Disp_BCDToDec(rxBuffer[6]) * 10000ULL
//			+ (uint64_t) Disp_BCDToDec(rxBuffer[7]) * 100ULL
//			+ (uint64_t) Disp_BCDToDec(rxBuffer[8]);
//
//	uint64_t sal = (uint64_t) Disp_BCDToDec(rxBuffer[9])
//			* 1000000000000ULL
//			+ (uint64_t) Disp_BCDToDec(rxBuffer[10]) * 10000000000ULL
//			+ (uint64_t) Disp_BCDToDec(rxBuffer[11]) * 100000000ULL
//			+ (uint64_t) Disp_BCDToDec(rxBuffer[12]) * 1000000ULL
//			+ (uint64_t) Disp_BCDToDec(rxBuffer[13]) * 10000ULL
//			+ (uint64_t) Disp_BCDToDec(rxBuffer[14]) * 100ULL
//			+ (uint64_t) Disp_BCDToDec(rxBuffer[15]);
//
//	*volume = vol / 100.0f;
//	*sale = sal / 100.0f;
//
//	return DISP_OK;
//}

DISP_ErrorCode_t Disp_GetDataWhenStopWorking(float *volume, float *sale) {

	*volume = 0.0f;
	*sale = 0.0f;

	uint8_t cmdBuffer[6];
	uint16_t cmdLen;
	uint8_t address = 0x01;

	cmdLen = Disp_BuildRead(address, DISP_ORIGIN_CURRENT, DISP_LEN_OFFLINE_DATA, cmdBuffer);

//	*status = (DISP_Status_t)DISP_ERROR;

	cmdLen = Disp_BuildRead(0x01, DISP_ORIGIN_STATUS, DISP_LEN_STATUS,
			cmdBuffer);

	uint8_t rxBufferLen = 13;
	uint8_t rxBuffer[rxBufferLen];
	uint16_t txTimeout = 100;
	uint16_t rxTimeout = 500;

	DISP_ErrorCode_t err = Disp_Transaction(cmdBuffer, cmdLen, rxBuffer,
			rxBufferLen, txTimeout, rxTimeout);

	if (err != DISP_OK)
		return err;

	DISP_ParseError_t perr = Disp_ParsePacket(rxBuffer, rxBufferLen);

	if (perr != DISP_PARSE_OK) {
		if (perr == DISP_PARSE_INFO_WRONG) {
			DISP_Status_t status = (DISP_Status_t) rxBuffer[3];
		}
		return DISP_PROTOCOL_PARSE_ERROR;
	}

	/* Expected response:
	 * A5 01 03 0C
	 * 00 00 00 58 37
	 * 00 00 00 12 34 56 78
	 * CRC
	 */

	uint32_t vol = (uint64_t) Disp_BCDToDec(rxBuffer[4]) * 10000UL
			+ (uint64_t) Disp_BCDToDec(rxBuffer[5]) * 100UL
			+ (uint64_t) Disp_BCDToDec(rxBuffer[6]);

	uint32_t sal = (uint64_t) Disp_BCDToDec(rxBuffer[8]) * 1000000UL
			+ (uint64_t) Disp_BCDToDec(rxBuffer[9]) * 10000UL
			+ (uint64_t) Disp_BCDToDec(rxBuffer[10]) * 100UL
			+ (uint64_t) Disp_BCDToDec(rxBuffer[11]);

	*volume = vol / 100.0f;
	*sale = sal / 100.0f;

	return DISP_OK;
}
/*
 * Set device mode
 */
bool Disp_SetMode(uint8_t mode) {

	uint8_t cmdBuffer[7];
	uint16_t cmdLen;

	uint8_t address = 0x01;
	uint8_t origin = 0x5A;
	uint8_t datalen = 0x01;;

	cmdLen = Disp_BuildWrite(address, origin, datalen, &mode, cmdBuffer);

    uint8_t rxBufferLen = 6;
    uint8_t rxBuffer[rxBufferLen];
	uint16_t txTimeout = 100;
	uint16_t rxTimeout = 500;

	DISP_ErrorCode_t err = Disp_Transaction(cmdBuffer, cmdLen, rxBuffer, rxBufferLen, txTimeout, rxTimeout);

	if (err != DISP_OK)
		return 0;

	DISP_ParseError_t perr = Disp_ParsePacket(rxBuffer, rxBufferLen);

	if (perr != DISP_PARSE_OK)
	{
	    if (perr == DISP_PARSE_INFO_WRONG)
	    {
	    	DISP_Status_t status = (DISP_Status_t)rxBuffer[3];
	    }
	    return 0;
	}

	return 1;
}
static const uint8_t CRC8_TAB[256] = { 0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd,
		0x83, 0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41, 0x9d, 0xc3, 0x21,
		0x7f, 0xfc, 0xa2, 0x40, 0x1e, 0x5f, 0x01, 0xe3, 0xbd, 0x3e, 0x60, 0x82,
		0xdc, 0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0, 0xe1, 0xbf, 0x5d,
		0x03, 0x80, 0xde, 0x3c, 0x62, 0xbe, 0xe0, 0x02, 0x5c, 0xdf, 0x81, 0x63,
		0x3d, 0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff, 0x46, 0x18, 0xfa,
		0xa4, 0x27, 0x79, 0x9b, 0xc5, 0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb, 0x59,
		0x07, 0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x06, 0x58, 0x19, 0x47, 0xa5,
		0xfb, 0x78, 0x26, 0xc4, 0x9a, 0x65, 0x3b, 0xd9, 0x87, 0x04, 0x5a, 0xb8,
		0xe6, 0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24, 0xf8, 0xa6, 0x44,
		0x1a, 0x99, 0xc7, 0x25, 0x7b, 0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x05, 0xe7,
		0xb9, 0x8c, 0xd2, 0x30, 0x6e, 0xed, 0xb3, 0x51, 0x0f, 0x4e, 0x10, 0xf2,
		0xac, 0x2f, 0x71, 0x93, 0xcd, 0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc,
		0x92, 0xd3, 0x8d, 0x6f, 0x31, 0xb2, 0xec, 0x0e, 0x50, 0xaf, 0xf1, 0x13,
		0x4d, 0xce, 0x90, 0x72, 0x2c, 0x6d, 0x33, 0xd1, 0x8f, 0x0c, 0x52, 0xb0,
		0xee, 0x32, 0x6c, 0x8e, 0xd0, 0x53, 0x0d, 0xef, 0xb1, 0xf0, 0xae, 0x4c,
		0x12, 0x91, 0xcf, 0x2d, 0x73, 0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17,
		0x49, 0x08, 0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b, 0x57, 0x09, 0xeb,
		0xb5, 0x36, 0x68, 0x8a, 0xd4, 0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48,
		0x16, 0xe9, 0xb7, 0x55, 0x0b, 0x88, 0xd6, 0x34, 0x6a, 0x2b, 0x75, 0x97,
		0xc9, 0x4a, 0x14, 0xf6, 0xa8, 0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9,
		0xf7, 0xb6, 0xe8, 0x0a, 0x54, 0xd7, 0x89, 0x6b, 0x35 };

static uint8_t Disp_CRC8(uint8_t *buf, uint16_t len) {
	uint8_t index;
	uint8_t crc = 0;

	while (len--) {
		index = crc ^ (*buf++);

		crc = CRC8_TAB[index];
	}

	return crc;
}

static void RS485_TXEnable(void)
{
    HAL_GPIO_WritePin(rs485_dePort,
                      rs485_dePin,
                      GPIO_PIN_SET);
}

static void RS485_RXEnable(void)
{
    HAL_GPIO_WritePin(rs485_dePort,
                      rs485_dePin,
                      GPIO_PIN_RESET);
}
static void Disp_UART_FlushRx(void)
{
    UART_HandleTypeDef *huart = rs485_uart;

    if (huart == NULL)
        return;

//    rs485_handle.rxLen = 0;

    while (__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE) != RESET)
    {
        (void)huart->Instance->DR;
    }

    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);
}
/*
 * This functions builds costom read/write command
 * const uint8_t cmd[] = { 0x01, 0x0C, 0x00, 0x01, 0x08 };
 * const uint8_t cmd_status[] = { 0x01, 0x03, 0x00, 0x01 };
 */
static uint16_t Disp_BuildCustomCommand(const uint8_t *payload, uint8_t payloadLen,
		uint8_t *txBuf) {
	txBuf[0] = DISP_HEADER;      // 0xA5

	memcpy(&txBuf[1], payload, payloadLen);

	txBuf[payloadLen + 1] = Disp_CRC8(&txBuf[1], payloadLen);

	return payloadLen + 2;
}

/*
 * converts an 8-bit decimal number (0–99) into 1-byte packed BCD.
 * 45 -> 0x45
 */
static uint8_t Disp_DecToBCD(uint8_t val) {
	return (uint8_t) (((val / 10) << 4) | (val % 10));
}
static uint8_t Disp_BCDToDec(uint8_t bcd) {
	return ((bcd >> 4) * 10U) + (bcd & 0x0F);
}
