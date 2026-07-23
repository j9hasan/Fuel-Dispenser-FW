#ifndef JSON_BUILDER_H
#define JSON_BUILDER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Offline JSON */

#define JSON_BUFFER_SIZE 4096

void JSON_OfflineBegin(char *buffer, size_t bufferSize, uint16_t totalRecords,
		uint8_t nozzleNumber);

void JSON_OfflineAddRecord(float amount, float liter,
bool isLast);

int JSON_OfflineEnd(void);

/* Sale JSON */
void JSON_SaleBegin(char *buffer, size_t bufferSize, const char *timestamp,
		float amount, float liter, uint8_t nozzleNumber);

int JSON_SaleEnd(void);

/* Common */
char* JSON_GetBuffer(void);

#endif
