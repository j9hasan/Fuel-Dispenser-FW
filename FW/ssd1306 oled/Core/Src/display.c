#include "display.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

#include <stdio.h>
#include <string.h>

static Display_NetStatus_t s_netStatus = DISPLAY_NET_OFFLINE;

/* Shared row layout (pixels) so every page lines up the same way and we
 * don't repeat magic numbers per page. Tuned for a 128x64 panel. */
#define ROW_HEADER   0   /* page tag (left) + 4G status (right), Font_6x8 */
#define ROW_LINE1    12  /* Font_7x10 content rows */
#define ROW_LINE2    26
#define ROW_LINE3    40
#define ROW_FOOTER   54  /* status/error line, Font_6x8 */

void Display_Init(void) {
    ssd1306_Init();
}

void Display_SetNetStatus(Display_NetStatus_t status) {
    s_netStatus = status;
}

/* Draws "<tag>" flush left and "4G:ON"/"4G:OFF" flush right on the header
 * row. Pass NULL/"" for tag to show only the connectivity indicator (used
 * on the splash screen). Does not Fill or UpdateScreen - caller controls
 * when the frame actually flushes to the panel. */
static void Display_DrawHeader(const char *tag) {
    if (tag != NULL && tag[0] != '\0') {
        ssd1306_SetCursor(0, ROW_HEADER);
        ssd1306_WriteString((char *)tag, Font_6x8, White);
    }

    const char *net = (s_netStatus == DISPLAY_NET_ONLINE) ? "4G:ON" : "4G:OFF";
    uint8_t w = (uint8_t)(strlen(net) * Font_6x8.width);
    uint8_t x = (SSD1306_WIDTH > w) ? (uint8_t)(SSD1306_WIDTH - w) : 0;

    ssd1306_SetCursor(x, ROW_HEADER);
    ssd1306_WriteString((char *)net, Font_6x8, White);
}

/* ------------------------------------------------------------------- */

void Display_ShowSplash(void) {
    ssd1306_Fill(Black);

    Display_DrawHeader(NULL); /* net indicator only, no page tag */

    ssd1306_SetCursor(9, 12);
    ssd1306_WriteString("Dispenser", Font_11x18, White);

    ssd1306_SetCursor(46, 34);
    ssd1306_WriteString("IOT", Font_11x18, White);

    ssd1306_UpdateScreen();
}

void Display_ShowNoOfflineRecords(void) {
    ssd1306_Fill(Black);
    Display_DrawHeader("OFFLINE");

    ssd1306_SetCursor(0, ROW_LINE2);
    ssd1306_WriteString("No offline record", Font_7x10, White);

    ssd1306_UpdateScreen();
}

void Display_ShowOfflineSummary(uint16_t count) {
    char line[24];
    snprintf(line, sizeof(line), "Total found: %u", count);

    ssd1306_Fill(Black);
    Display_DrawHeader("OFFLINE");

    ssd1306_SetCursor(0, ROW_LINE2);
    ssd1306_WriteString(line, Font_7x10, White);

    ssd1306_UpdateScreen();
}

void Display_ShowOfflineRecord(uint16_t index_1based, uint16_t total,
                                float volume, float sale) {
    char line[24];

    ssd1306_Fill(Black);
    Display_DrawHeader("OFFLINE");

    snprintf(line, sizeof(line), "Record #%u/%u", index_1based, total);
    ssd1306_SetCursor(0, ROW_LINE1);
    ssd1306_WriteString(line, Font_7x10, White);

    snprintf(line, sizeof(line), "Vol : %.2f L", volume);
    ssd1306_SetCursor(0, ROW_LINE2);
    ssd1306_WriteString(line, Font_7x10, White);

    snprintf(line, sizeof(line), "Sale: %.2f", sale);
    ssd1306_SetCursor(0, ROW_LINE3);
    ssd1306_WriteString(line, Font_7x10, White);

    ssd1306_UpdateScreen();
}

void Display_ShowOfflineReturnStatus(uint8_t statusCode) {
    char line[24];
    snprintf(line, sizeof(line), "Status: 0x%02X", statusCode);

    /* Refresh header (net status may have changed) without clearing the
     * offline-record frame already on screen. */
    Display_DrawHeader("OFFLINE");

    ssd1306_SetCursor(0, ROW_FOOTER);
    ssd1306_WriteString(line, Font_6x8, White);

    ssd1306_UpdateScreen();
}

void Display_ShowWorkingPage(float volume, float sale, uint8_t errCode) {
    char line[24];

    ssd1306_Fill(Black);
    Display_DrawHeader("WORKING");

    snprintf(line, sizeof(line), "Vol : %.2f L", volume);
    ssd1306_SetCursor(0, ROW_LINE1);
    ssd1306_WriteString(line, Font_7x10, White);

    snprintf(line, sizeof(line), "Sale: %.2f", sale);
    ssd1306_SetCursor(0, ROW_LINE2);
    ssd1306_WriteString(line, Font_7x10, White);

    snprintf(line, sizeof(line), "Status: 0x%02X", errCode);
    ssd1306_SetCursor(0, ROW_FOOTER);
    ssd1306_WriteString(line, Font_6x8, White);

    ssd1306_UpdateScreen();
}

void Display_UpdateWorkingError(uint8_t errCode) {
    char line[24];
    snprintf(line, sizeof(line), "Status: 0x%02X   ", errCode);

    Display_DrawHeader("WORKING"); /* keeps the 4G indicator live too */

    ssd1306_SetCursor(0, ROW_FOOTER);
    ssd1306_WriteString(line, Font_6x8, White);

    ssd1306_UpdateScreen();
}
