#include <avr/io.h>
#include <stdint.h>
#include "servo.h"

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

/* Prescaler = 8:
 * 16 MHz / 8 = 2 MHz => 0.5 us per tick => 2 ticks per us
 */
#define TICKS_PER_US   2u

/* Servo output pin: OC1B = PB2 = Arduino D10 */
#define SERVO_DDR      DDRB
#define SERVO_PORT     PORTB
#define SERVO_PIN      PB2

/* 20 ms period => 20000 us * 2 ticks/us = 40000 ticks
 * Mode 15 uses TOP = OCR1A, so set OCR1A = TOP-1
 */
#define SERVO_TOP_TICKS ((uint16_t)(20000u * TICKS_PER_US))  /* 40000 */
#define SERVO_TOP_REG   ((uint16_t)(SERVO_TOP_TICKS - 1u))   /* 39999 */

static uint8_t g_enabled = 0u;

static uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void servo_init(void)
{
    /* OC1B pin as output */
    SERVO_DDR |= (1 << SERVO_PIN);

    /* Stop Timer1 while configuring */
    TCCR1A = 0u;
    TCCR1B = 0u;

    /* Fast PWM mode 15: WGM13..0 = 1111, TOP = OCR1A */
    TCCR1A |= (1 << WGM10) | (1 << WGM11);
    TCCR1B |= (1 << WGM12) | (1 << WGM13);

    /* TOP for 20ms period */
    OCR1A = SERVO_TOP_REG;

    /* Prescaler = 8 */
    TCCR1B |= (1 << CS11);

    /* Start with output disconnected to avoid twitch */
    servo_disable();

    /* Safe default position (also enables output) */
    servo_set_angle(90u);
}

void servo_enable(void)
{
    /* Non-inverting on OC1B */
    TCCR1A |= (1 << COM1B1);
    g_enabled = 1u;
}

void servo_disable(void)
{
    /* Disconnect OC1B and drive pin low */
    TCCR1A &= (uint8_t)~(1 << COM1B1);
    SERVO_PORT &= (uint8_t)~(1 << SERVO_PIN);
    g_enabled = 0u;
}

uint8_t servo_is_enabled(void)
{
    return g_enabled;
}

void servo_set_limits_us(uint16_t min_us, uint16_t max_us)
{
    /* If your servo.h has SERVO_MIN_US/SERVO_MAX_US as fixed macros,
       you can ignore this function. Keeping it for API compatibility. */
    (void)min_us;
    (void)max_us;
}

void servo_set_pulse_us(uint16_t us)
{
    /* Uses SERVO_MIN_US and SERVO_MAX_US from servo.h */
    us = clamp_u16(us, SERVO_MIN_US, SERVO_MAX_US);

    /* Convert microseconds to ticks (0.5us per tick => 2 ticks/us) */
    uint16_t ticks = (uint16_t)(us * TICKS_PER_US);

    /* In Fast PWM with OCR1A as TOP, OCR1B must be <= OCR1A */
    if (ticks > OCR1A) ticks = OCR1A;

    OCR1B = ticks;

    if (!g_enabled) {
        servo_enable();
    }
}

void servo_set_angle(uint8_t angle)
{
    if (angle > 180u) angle = 180u;

    uint32_t pulse =
        (uint32_t)SERVO_MIN_US +
        (((uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * (uint32_t)angle) / 180u);

    servo_set_pulse_us((uint16_t)pulse);
}

/* Project wrappers */
void servo_rotate_open(void)  { servo_set_angle(90u); }
void servo_rotate_close(void) { servo_set_angle(0u);  }
void servo_stop(void)         { servo_disable();       }
