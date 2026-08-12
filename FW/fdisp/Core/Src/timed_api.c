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

//char timeStr[] = "26/08/04,12:37:00+24";

bool RTC_SyncFromString(const char *timeStr)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    int yyyy, mm, dd;
    int hh, min, ss;

    if (timeStr == NULL)
        return false;

    /* Parse: 2026-08-11T09:22:02Z */
    if (sscanf(timeStr,
               "%d-%d-%dT%d:%d:%dZ",
               &yyyy,
               &mm,
               &dd,
               &hh,
               &min,
               &ss) != 6)
    {
        return false;
    }

    /* Validate basic ranges */
    if (yyyy < 2000 || yyyy > 2099 ||
        mm < 1 || mm > 12 ||
        dd < 1 || dd > 31 ||
        hh < 0 || hh > 23 ||
        min < 0 || min > 59 ||
        ss < 0 || ss > 59)
    {
        return false;
    }

    /*
     * STM32 RTC stores year as:
     * 0 = 2000
     * 26 = 2026
     */
    sDate.Year  = yyyy - 2000;
    sDate.Month = mm;
    sDate.Date  = dd;

    sTime.Hours   = hh;
    sTime.Minutes = min;
    sTime.Seconds = ss;

    /* Weekday must be valid */
    sDate.WeekDay = RTC_WEEKDAY_TUESDAY;  // Calculate if required

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
             "20%02u-%02u-%02uT%02u:%02u:%02uZ",
             sDate.Year,
             sDate.Month,
             sDate.Date,
             sTime.Hours,
             sTime.Minutes,
             sTime.Seconds);

    return dateTime;
}

TimeSyncResult_t SyncServerTime(void)
{
    char serverTime[32];

    if (!SIM800L_IsInternetConnected())
    {
        return SYNC_TIME_NO_INTERNET;
    }

    if (!SIM800L_ServerTimeSync(serverTime, sizeof(serverTime)))
    {
        return SYNC_TIME_SERVER_FAILED;
    }

    if (!RTC_SyncFromString(serverTime))
    {
        return SYNC_TIME_RTC_FAILED;
    }

    return SYNC_TIME_OK;
}

const char *SyncServerTimeText(TimeSyncResult_t result)
{
    static const char *text[] =
    {
        "TIME SYNC OK",
        "NO INTERNET",
        "SERVER FAIL",
        "RTC FAIL"
    };

    return text[result];
}


//bool RTC_SyncFromString(const char *timeStr)
//{
//    RTC_TimeTypeDef sTime = {0};
//    RTC_DateTypeDef sDate = {0};
//
//    int yy, mm, dd;
//    int hh, min, ss;
//    int tz;
//
//    if (sscanf(timeStr,
//               "%d/%d/%d,%d:%d:%d+%d",
//               &yy,
//               &mm,
//               &dd,
//               &hh,
//               &min,
//               &ss,
//               &tz) != 7)
//    {
//        return false;
//    }
//
//    sTime.Hours   = hh;
//    sTime.Minutes = min;
//    sTime.Seconds = ss;
//
//    sDate.Year  = yy;
//    sDate.Month = mm;
//    sDate.Date  = dd;
//
//    /* STM32 RTC weekday must be valid */
//    sDate.WeekDay = RTC_WEEKDAY_TUESDAY;    // Compute if needed
//
//    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
//        return false;
//
//    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
//        return false;
//
//    return true;
//}
