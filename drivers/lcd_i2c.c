#include "lcd_i2c.h"
#include <avr/io.h>
#include <util/delay.h>

/* PCF8574 bit mapping (common) */
#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0x04
#define LCD_RW         0x02
#define LCD_RS         0x01

/* ---------- Low-level I2C (TWI) ---------- */

static void i2c_init(void)
{
    TWSR = 0x00;                       // Prescaler = 1
    TWBR = 72;                         // ~100 kHz @ 16 MHz
    TWCR = (1 << TWEN);
}

static void i2c_start(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

static void i2c_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    _delay_us(10);
}

static void i2c_write(uint8_t data)
{
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

/* ---------- LCD Low-Level ---------- */

static void lcd_write_nibble(uint8_t nibble, uint8_t control)
{
    uint8_t data = nibble | control | LCD_BACKLIGHT;

    i2c_start();
    i2c_write(LCD_I2C_ADDR << 1);
    i2c_write(data | LCD_ENABLE);
    _delay_us(1);
    i2c_write(data & ~LCD_ENABLE);
    i2c_stop();
}

static void lcd_send(uint8_t value, uint8_t mode)
{
    lcd_write_nibble(value & 0xF0, mode);
    lcd_write_nibble((value << 4) & 0xF0, mode);
    _delay_us(50);
}

static void lcd_command(uint8_t cmd)
{
    lcd_send(cmd, 0);
}

static void lcd_data(uint8_t data)
{
    lcd_send(data, LCD_RS);
}

/* ---------- Public Functions ---------- */

void lcd_init(void)
{
    i2c_init();
    _delay_ms(50);

    /* Initialization sequence */
    lcd_write_nibble(0x30, 0);
    _delay_ms(5);
    lcd_write_nibble(0x30, 0);
    _delay_us(150);
    lcd_write_nibble(0x20, 0);

    lcd_command(0x28); // 4-bit, 2-line
    lcd_command(0x0C); // Display ON, cursor OFF
    lcd_command(0x06); // Entry mode
    lcd_clear();
}

void lcd_clear(void)
{
    lcd_command(0x01);
    _delay_ms(2);
}

void lcd_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t address = (row == 0) ? 0x80 : 0xC0;
    lcd_command(address + col);
}

void lcd_print(const char *str)
{
    while (*str) {
        lcd_data(*str++);
    }
}

void lcd_print_uint16(uint16_t value)
{
    char buf[6];
    uint8_t i = 0;

    if (value == 0) {
        lcd_data('0');
        return;
    }

    while (value > 0 && i < 5) {
        buf[i++] = (value % 10) + '0';
        value /= 10;
    }

    while (i--) {
        lcd_data(buf[i]);
    }
}
