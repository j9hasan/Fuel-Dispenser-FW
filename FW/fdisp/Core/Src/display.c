/*
 * display.c
 *
 *  Created on: Aug 2, 2026
 *      Author: jubaid_h
 */


// display.c
#include "display.h"

void Display_SetNetStatus(Display_NetStatus_t status)
{
    const char *label = (status == DISPLAY_NET_ONLINE) ? "ONLINE" : "OFFLINE";
    uint8_t w = (uint8_t)(strlen(label) * Font_6x8.width);
    uint8_t x = SSD1306_WIDTH - w;

    ssd1306_FillRectangle(64, DISPLAY_TOP_ROW_Y, SSD1306_WIDTH - 1,
                           DISPLAY_TOP_ROW_Y + Font_6x8.height - 1, Black);
    ssd1306_SetCursor(x, DISPLAY_TOP_ROW_Y);
    ssd1306_WriteString((char *)label, Font_6x8, White);
    ssd1306_UpdateScreen();
}

//void Display_SetMiddleText(const char *text)
//{
//    ssd1306_FillRectangle(0, DISPLAY_MID_ROW_Y, SSD1306_WIDTH - 1,
//                           DISPLAY_MID_ROW_Y + Font_11x18.height - 1, Black);
//    ssd1306_SetCursor(0, DISPLAY_MID_ROW_Y);
//    ssd1306_WriteString((char *)text, Font_11x18, White);
//    ssd1306_UpdateScreen();
//}
// display.c
void Display_SetMiddleText(const char *text)
{
    char line1[12] = {0};
    char line2[12] = {0};
    uint8_t max_chars = 11; // SSD1306_WIDTH / Font_11x18.width

    uint8_t len = (uint8_t)strlen(text);

    if (len <= max_chars) {
        strncpy(line1, text, max_chars);
    } else {
        /* find split point: last space at or before max_chars */
        uint8_t split = max_chars;
        for (uint8_t i = max_chars; i > 0; i--) {
            if (text[i] == ' ') { split = i; break; }
        }

        strncpy(line1, text, split);
        line1[split] = '\0';

        const char *rest = text + split;
        while (*rest == ' ') rest++; // skip leading space

        strncpy(line2, rest, max_chars);
        line2[max_chars] = '\0';
    }

    ssd1306_FillRectangle(0, DISPLAY_MID_ROW_Y, SSD1306_WIDTH - 1,
                           DISPLAY_MID_ROW_Y + (2 * Font_11x18.height) - 1, Black);

    ssd1306_SetCursor(0, DISPLAY_MID_ROW_Y);
    ssd1306_WriteString(line1, Font_11x18, White);

    if (line2[0] != '\0') {
        ssd1306_SetCursor(0, DISPLAY_MID_ROW_Y + Font_11x18.height);
        ssd1306_WriteString(line2, Font_11x18, White);
    }

    ssd1306_UpdateScreen();
}
void Display_SetStatusText(const char *text)
{
    ssd1306_FillRectangle(0, DISPLAY_BOTTOM_ROW_Y, 90,
                           DISPLAY_BOTTOM_ROW_Y + Font_6x8.height - 1, Black);
    ssd1306_SetCursor(0, DISPLAY_BOTTOM_ROW_Y);
    ssd1306_WriteString((char *)text, Font_6x8, White);
    ssd1306_UpdateScreen();
}
