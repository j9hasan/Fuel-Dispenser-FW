
#ifndef DISPLAY_H
#define DISPLAY_H

#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <string.h>
#include <stdio.h>
typedef enum {
    DISPLAY_NET_OFFLINE = 0,
    DISPLAY_NET_ONLINE  = 1
} Display_NetStatus_t;

/* Row layout (128x64) */
// display.h — adjusted row layout
#define DISPLAY_TOP_ROW_Y     0    // net status, right-aligned (8px, Font_6x8)
#define DISPLAY_MID_ROW_Y     10   // middle text, up to 2 lines (36px, Font_11x18)
#define DISPLAY_BOTTOM_ROW_Y  46   // status response, bottom-left

void Display_SetNetStatus(Display_NetStatus_t status);
void Display_SetMiddleText(const char *text);
void Display_SetStatusText(const char *text);

void SIM800_BootAnimation(void);
#endif
