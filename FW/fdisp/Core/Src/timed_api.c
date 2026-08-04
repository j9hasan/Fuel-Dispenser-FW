/*
 * timed_api.c
 *
 *  Created on: Aug 4, 2026
 *      Author: jubaid_h
 */
#include"time_api.h"
#include "sim800l.h"
#include "time.h"
#include "string.h"
#include <stdio.h>

char timeStr[] = "26/08/04,12:37:00+24";


bool RTC_SyncFromString(const char *timeStr)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    int yy, mm, dd;
    int hh, min, ss;
    int tz;

    if (sscanf(timeStr,
               "%d/%d/%d,%d:%d:%d+%d",
               &yy,
               &mm,
               &dd,
               &hh,
               &min,
               &ss,
               &tz) != 7)
    {
        return false;
    }

    sTime.Hours   = hh;
    sTime.Minutes = min;
    sTime.Seconds = ss;

    sDate.Year  = yy;
    sDate.Month = mm;
    sDate.Date  = dd;

    /* STM32 RTC weekday must be valid */
    sDate.WeekDay = RTC_WEEKDAY_TUESDAY;    // Compute if needed

    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
        return false;

    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
        return false;

    return true;
}



char *RTC_GetDateTimeString(void)
{
    static char dateTime[32];

    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    snprintf(dateTime,
             sizeof(dateTime),
             "20%02u-%02u-%02u %02u:%02u:%02u",
             sDate.Year,
             sDate.Month,
             sDate.Date,
             sTime.Hours,
             sTime.Minutes,
             sTime.Seconds);

    return dateTime;
}
