/* ssd1306.c - SSD1306 128x64 OLED driver using bit-banged I2C.
 * Wiring: PB8 = SCL, PB9 = SDA (open-drain with pull-ups), address 0x3C.
 * Drawing happens in a 1 KB frame buffer in RAM; ssd1306_update() sends it. */
#include "ssd1306.h"
#include "main.h"
#include <string.h>

#define I2C_PORT  GPIOB
#define SCL_PIN   GPIO_PIN_8
#define SDA_PIN   GPIO_PIN_9
#define OLED_ADDR 0x3C

/* 128 columns x 8 pages; each byte is a vertical strip of 8 pixels */
static uint8_t fb[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

/* ---- bit-banged I2C (write only) ---- */
static void i2c_delay(void)
{
    for (volatile int i = 0; i < 4; i++) { }
}

static void scl(int level)
{
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    i2c_delay();
}

static void sda(int level)
{
    HAL_GPIO_WritePin(I2C_PORT, SDA_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    i2c_delay();
}

static void i2c_start(void) { sda(1); scl(1); sda(0); scl(0); }
static void i2c_stop(void)  { sda(0); scl(1); sda(1); }

static void i2c_write(uint8_t byte)
{
    for (int bit = 7; bit >= 0; bit--) {
        sda((byte >> bit) & 1);
        scl(1);
        scl(0);
    }
    sda(1);         /* release SDA so the display can ACK */
    scl(1);         /* ninth clock: ACK (not checked in this version) */
    scl(0);
}

/* ---- SSD1306 commands ---- */
static void cmd(uint8_t c)
{
    i2c_start();
    i2c_write(OLED_ADDR << 1);   /* address + write bit */
    i2c_write(0x00);             /* control byte: command follows */
    i2c_write(c);
    i2c_stop();
}

void ssd1306_init(void)
{
    static const uint8_t init_seq[] = {
        0xAE,        /* display off                     */
        0xD5, 0x80,  /* clock divide                    */
        0xA8, 0x3F,  /* multiplex ratio: 64 rows        */
        0xD3, 0x00,  /* display offset 0                */
        0x40,        /* start line 0                    */
        0x8D, 0x14,  /* charge pump on                  */
        0x20, 0x00,  /* horizontal addressing mode      */
        0xA1,        /* mirror columns (normal layout)  */
        0xC8,        /* scan rows top to bottom         */
        0xDA, 0x12,  /* COM pin layout for 128x64       */
        0x81, 0xCF,  /* contrast                        */
        0xD9, 0xF1,  /* pre-charge period               */
        0xDB, 0x40,  /* VCOMH level                     */
        0xA4,        /* show RAM contents               */
        0xA6,        /* normal (not inverted)           */
        0xAF         /* display on                      */
    };
    GPIO_InitTypeDef g = {0};

    g.Pin   = SCL_PIN | SDA_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_OD;   /* I2C lines are open-drain */
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(I2C_PORT, &g);
    HAL_GPIO_WritePin(I2C_PORT, SCL_PIN | SDA_PIN, GPIO_PIN_SET);
    HAL_Delay(10);

    for (size_t i = 0; i < sizeof init_seq; i++)
        cmd(init_seq[i]);

    ssd1306_clear();
    ssd1306_update();
}

void ssd1306_clear(void) { memset(fb, 0x00, sizeof fb); }
void ssd1306_fill(void)  { memset(fb, 0xFF, sizeof fb); }

void ssd1306_set_pixel(int x, int y, int on)
{
    uint8_t *b, mask;
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) return;
    b = &fb[(y / 8) * SSD1306_WIDTH + x];
    mask = (uint8_t)(1u << (y & 7));
    if (on) *b |= mask; else *b &= (uint8_t)~mask;
}

void ssd1306_update(void)
{
    cmd(0x21); cmd(0); cmd(SSD1306_WIDTH - 1);        /* column range */
    cmd(0x22); cmd(0); cmd(SSD1306_HEIGHT / 8 - 1);   /* page range   */

    i2c_start();
    i2c_write(OLED_ADDR << 1);
    i2c_write(0x40);                 /* control byte: data follows */
    for (size_t i = 0; i < sizeof fb; i++)
        i2c_write(fb[i]);
    i2c_stop();
}
