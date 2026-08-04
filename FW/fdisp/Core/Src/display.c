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

//max 18 char
void Display_SetMiddleText(const char *text)
{
    char line1[19] = {0};
    char line2[19] = {0};
    uint8_t max_chars = 18; // SSD1306_WIDTH / Font_7x10.width

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

    /* available middle region: from DISPLAY_MID_ROW_Y to DISPLAY_BOTTOM_ROW_Y */
    uint8_t region_h = DISPLAY_BOTTOM_ROW_Y - DISPLAY_MID_ROW_Y;
    uint8_t text_h = (line2[0] != '\0') ? (2 * Font_7x10.height) : Font_7x10.height;
    uint8_t y = DISPLAY_MID_ROW_Y + (uint8_t)((region_h - text_h) / 2);

    ssd1306_FillRectangle(0, DISPLAY_MID_ROW_Y, SSD1306_WIDTH - 1,
                           DISPLAY_BOTTOM_ROW_Y - 1, Black);

    /* find longest line and use it to compute a single shared x */
    uint8_t len1 = (uint8_t)strlen(line1);
    uint8_t len2 = (uint8_t)strlen(line2);
    uint8_t max_len = (len1 > len2) ? len1 : len2;
    uint8_t x = (uint8_t)((SSD1306_WIDTH - (max_len * Font_7x10.width)) / 2);

    ssd1306_SetCursor(x, y);
    ssd1306_WriteString(line1, Font_7x10, White);

    if (line2[0] != '\0') {
        ssd1306_SetCursor(x, y + Font_7x10.height);
        ssd1306_WriteString(line2, Font_7x10, White);
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

static const uint8_t progress[] = {
     2,  5,  9, 14, 20,
    28, 37, 48, 60, 70,
    79, 86, 91, 95, 97,
    98, 99,100
};

static const SIM_BOOT_TIME = 12000; //12s
void SIM800_BootAnimation(void)
{
    char msg[24];

    const uint16_t totalTime = SIM_BOOT_TIME;                 // 10 s
    const uint16_t delay = totalTime / (sizeof(progress));

    for (uint8_t i = 0; i < sizeof(progress); i++)
    {
        snprintf(msg, sizeof(msg), "SIM Init %u%%", progress[i]);
        Display_SetMiddleText(msg);
        HAL_Delay(delay);
    }
}
