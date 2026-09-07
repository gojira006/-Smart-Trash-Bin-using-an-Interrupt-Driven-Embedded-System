// sharp_ir.c - GP2Y0A41SK0F (4-30cm range)
// Stability updates:
//  - Median-of-5 filtering on raw ADC readings to suppress spikes
//  - Keeps the existing volts->distance conversion style

#include <avr/io.h>
#include <stdint.h>
#include <math.h>
#include "sharp_ir.h"

// Sharp IR sensor GP2Y0A41SK0F connected to A0 (ADC0/PC0)
#define SHARP_IR_ADC_CHANNEL 0
#define SHARP_IR_SAMPLES 5u

/* --- Internal helpers (C90-friendly) --- */

static uint16_t sharp_ir_read_raw_once(void)
{
    /* Select ADC channel */
    ADMUX = (ADMUX & 0xF8) | (SHARP_IR_ADC_CHANNEL & 0x07);

    /* Start conversion */
    ADCSRA |= (1 << ADSC);

    /* Wait for conversion to complete */
    while (ADCSRA & (1 << ADSC)) {
        /* busy wait */
    }

    return ADC;
}

static uint16_t median5_u16(uint16_t a0, uint16_t a1, uint16_t a2, uint16_t a3, uint16_t a4)
{
    /* Simple bubble-sort on 5 elements, return middle (median) */
    uint16_t a[5];
    uint16_t tmp;
    uint8_t i;
    uint8_t j;

    a[0] = a0;
    a[1] = a1;
    a[2] = a2;
    a[3] = a3;
    a[4] = a4;

    for (i = 0; i < 5u; i++) {
        for (j = 0; j < 4u; j++) {
            if (a[j] > a[j + 1u]) {
                tmp = a[j];
                a[j] = a[j + 1u];
                a[j + 1u] = tmp;
            }
        }
    }

    return a[2];
}

void sharp_ir_init(void)
{
    /* Set ADC reference to AVcc (5V) */
    ADMUX = (1 << REFS0);

    /* Enable ADC, set prescaler to 128 (16MHz/128 = 125kHz ADC clock) */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    /* Dummy read */
    (void)sharp_ir_read_raw_once();
}

uint16_t sharp_ir_read_raw(void)
{
    /* Median-of-5 to suppress spikes (no extra delays needed) */
    uint16_t s0;
    uint16_t s1;
    uint16_t s2;
    uint16_t s3;
    uint16_t s4;

    s0 = sharp_ir_read_raw_once();
    s1 = sharp_ir_read_raw_once();
    s2 = sharp_ir_read_raw_once();
    s3 = sharp_ir_read_raw_once();
    s4 = sharp_ir_read_raw_once();

    return median5_u16(s0, s1, s2, s3, s4);
}

uint16_t sharp_ir_read_distance_cm(void)
{
    uint16_t adc_value;
    float volts;
    float distance;

    adc_value = sharp_ir_read_raw();

    /* Convert ADC to volts: volts = adc_value * (5.0 / 1024) */
    volts = adc_value * 0.0048828125f;

    /* High voltage (>2.8V) indicates object is very close (< ~4cm) */
    if (volts > 2.8f) {
        return 2u; /* closer than minimum range */
    }

    /* Avoid division by very small voltages (far/no target) */
    if (volts < 0.4f) {
        return 30u; /* max range */
    }

    /* Simplified approximation */
    distance = 11.5f / volts;

    if (distance > 30.0f) {
        distance = 30.0f;
    }

    return (uint16_t)distance;
}

uint16_t sharp_ir_read_distance_mm(void)
{
    uint16_t adc_value;
    float volts;
    float distance;

    adc_value = sharp_ir_read_raw();

    volts = adc_value * 0.0048828125f;

    if (volts < 0.4f) {
        return 300u;
    }

    distance = 115.0f / volts;

    if (distance < 40.0f) {
        distance = 40.0f;
    }
    if (distance > 300.0f) {
        distance = 300.0f;
    }

    return (uint16_t)distance;
}
