/*
 * time_api.h
 *
 *  Created on: Aug 4, 2026
 *      Author: jubaid_h
 */

#ifndef TIME_API_H
#define TIME_API_H

#include <stdbool.h>
//#include "stm32h7xx_hal_rtc.h"
#include "stm32h7xx_hal.h"

#include "main.h"

typedef enum
{
    SYNC_TIME_OK = 0,
    SYNC_TIME_NO_INTERNET,
    SYNC_TIME_SERVER_FAILED,
    SYNC_TIME_RTC_FAILED
} TimeSyncResult_t;

extern RTC_HandleTypeDef hrtc;
extern char timeStr[];


bool TimeAPI_ParseAndSetRTC(const char *response);

char *RTC_GetDateTimeString(void);
bool RTC_SyncFromString(const char *timeStr);


TimeSyncResult_t SyncServerTime(void);

const char *SyncServerTimeText(TimeSyncResult_t result);

#endif
