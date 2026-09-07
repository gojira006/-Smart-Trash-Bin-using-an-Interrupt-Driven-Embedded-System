#ifndef SHARP_IR_H
#define SHARP_IR_H

#include <stdint.h>

// Initialize ADC for Sharp IR sensor
void sharp_ir_init(void);

// Read raw ADC value (0-1023)
uint16_t sharp_ir_read_raw(void);

// Read distance in cm
uint16_t sharp_ir_read_distance_cm(void);

// Read distance in mm for better precision
uint16_t sharp_ir_read_distance_mm(void);

#endif
