#ifndef DISPLAY_H
#define DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Network (4G modem) connectivity status. Shown top-right on every page. */
typedef enum {
    DISPLAY_NET_OFFLINE = 0,
    DISPLAY_NET_ONLINE  = 1
} Display_NetStatus_t;

/* Call once at startup instead of ssd1306_Init() directly - it wraps it. */
void Display_Init(void);

/* Update the connectivity indicator drawn in the header of every page.
 * Call this whenever your modem/4G status changes, or once per loop /
 * page-change so the indicator stays current. */
void Display_SetNetStatus(Display_NetStatus_t status);

/* ---------------------------------------------------------------------
 * Pages
 * ------------------------------------------------------------------- */

/* Startup splash: "Dispenser" / "IOT" */
void Display_ShowSplash(void);

/* Offline-records page */
void Display_ShowNoOfflineRecords(void);
void Display_ShowOfflineSummary(uint16_t count);
void Display_ShowOfflineRecord(uint16_t index_1based, uint16_t total,
                                float volume, float sale);
/* Drawn on top of whatever offline-page frame is currently shown - does
 * not clear the screen, only refreshes the header + footer status line. */
void Display_ShowOfflineReturnStatus(uint8_t statusCode);

/* Working (live fueling) page */
void Display_ShowWorkingPage(float volume, float sale, uint8_t errCode);
/* Lightweight refresh of just the header + footer error line, used when
 * you want to hold the volume/sale numbers on screen but keep the error
 * code and connectivity indicator live. */
void Display_UpdateWorkingError(uint8_t errCode);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_H */
