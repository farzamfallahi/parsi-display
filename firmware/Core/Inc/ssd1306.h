/* ssd1306.h - SSD1306 128x64 monochrome OLED driver (I2C). */
#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64

void ssd1306_init(void);                       /* set up pins and the display */
void ssd1306_clear(void);                      /* all pixels off (in RAM)     */
void ssd1306_fill(void);                       /* all pixels on (in RAM)      */
void ssd1306_set_pixel(int x, int y, int on);  /* one pixel (in RAM)          */
void ssd1306_update(void);                     /* send the RAM buffer to OLED */

#endif /* SSD1306_H */
