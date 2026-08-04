#include "json_builder.h"
#include <stdio.h>
#include <stdio.h>


static char *json;
static size_t jsonSize;
static int jsonLen;
static uint16_t recordIndex;
static uint16_t totalRecordCount;

void JSON_OfflineBegin(char *buffer,
                size_t bufferSize,
                uint16_t totalRecords,
                uint8_t nozzleNumber)
{
    json = buffer;
    jsonSize = bufferSize;
    jsonLen = 0;
    recordIndex = 0;
    totalRecordCount = totalRecords;

    jsonLen += snprintf(json + jsonLen,
                        jsonSize - jsonLen,
                        "{\"c\":%u,\"n\":%u,\"r\":[",
                        totalRecords,
                        nozzleNumber);
}

void JSON_OfflineAddRecord(float amount,
                    float liter,
                    bool isLast)
{
    jsonLen += snprintf(json + jsonLen,
                        jsonSize - jsonLen,
                        "[%.2f,%.2f]%s",
                        amount,
                        liter,
                        isLast ? "" : ",");
}

int JSON_OfflineEnd(void)
{
    jsonLen += snprintf(json + jsonLen,
                        jsonSize - jsonLen,
                        "]}");

    return jsonLen;
}

char *JSON_GetBuffer(void)
{
    return json;
}

void JSON_SaleBegin(char *buffer,
                    size_t bufferSize,
                    const char *timestamp,
                    float amount,
                    float liter,
                    uint8_t nozzleNumber)
{
    json = buffer;
    jsonSize = bufferSize;
    jsonLen = 0;

    jsonLen += snprintf(json + jsonLen,
                        jsonSize - jsonLen,
                        "{\"t\":\"%s\",\"a\":%.2f,\"l\":%.2f,\"n\":%u",
                        timestamp,
                        amount,
                        liter,
                        nozzleNumber);
}

int JSON_SaleEnd(void)
{
    jsonLen += snprintf(json + jsonLen,
                        jsonSize - jsonLen,
                        "}");
    return jsonLen;
}


// error code send
#include <stdio.h>
#include "json_builder.h"
#include "genuine_rs485.h"

int JSON_GenerateDispenserError(char *buffer,
                                size_t bufferSize,
                                DISP_ErrorCode_t err)
{
    const char *errorName = "UNKNOWN_ERROR";
    const char *timeStr = RTC_GetDateTimeString();

    switch (err)
    {
    case DISP_OK:
        errorName = "DISP_OK";
        break;
    case DISP_WRONG_CMD:
        errorName = "DISP_WRONG_CMD";
        break;
    case DISP_DEVICE_STOP_WORKING:
        errorName = "DISP_DEVICE_STOP_WORKING";
        break;
    case DISP_RESEND_COMMAND:
        errorName = "DISP_RESEND_COMMAND";
        break;
    case DISP_DEVICE_BUSY:
        errorName = "DISP_DEVICE_BUSY";
        break;
    case DISP_DEVICE_OFFLINE:
        errorName = "DISP_DEVICE_OFFLINE";
        break;
    case DISP_CRC_ERROR:
        errorName = "DISP_CRC_ERROR";
        break;
    case DISP_RS485_SEND_ERROR:
        errorName = "DISP_RS485_SEND_ERROR";
        break;
    case DISP_RS485_RX_TIMEOUT:
        errorName = "DISP_RS485_RX_TIMEOUT";
        break;
    case DISP_RS485_FRAME_ERROR:
        errorName = "DISP_RS485_FRAME_ERROR";
        break;
    case DISP_RS485_START_REC_ERROR:
        errorName = "DISP_RS485_START_REC_ERROR";
        break;
    case DISP_RS485_TRANSACTION_ERROR:
        errorName = "DISP_RS485_TRANSACTION_ERROR";
        break;
    }

    return snprintf(buffer,
                    bufferSize,
                    "{"
                    "\"type\":\"dispenser_error\","
                    "\"time\":\"%s\","
                    "\"errorCode\":%u,"
                    "\"errorName\":\"%s\""
                    "}",
                    timeStr,
                    (uint8_t)err,
                    errorName);
}
