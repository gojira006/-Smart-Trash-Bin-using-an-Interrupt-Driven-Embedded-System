#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <stdint.h>

/* I2C address of PCF8574-based LCD backpack
 * Common values: 0x27 or 0x3F
 */
#define LCD_I2C_ADDR  0x27

/* Public API */
void lcd_init(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_print(const char *str);
void lcd_print_uint16(uint16_t value);

#endif
