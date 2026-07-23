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
