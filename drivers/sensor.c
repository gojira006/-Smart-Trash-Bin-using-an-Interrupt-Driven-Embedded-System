#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

#include "gpio.h"
#include "sensor.h"

/*
 * Ultrasonic using Timer1 Input Capture (ICP1 = PB0 / Arduino D8).
 *
 * IMPORTANT:
 * - Timer1 mode/prescaler/TOP are configured by the servo driver:
 *     Mode 15 (Fast PWM), TOP = OCR1A, Prescaler = 8
 * - We MUST NOT change Timer1 mode/prescaler/TOP here.
 * - We ONLY configure input capture bits and interrupt.
 *
 * With prescaler 8 @ 16MHz: Timer1 tick = 0.5 us.
 * We return pulse_width in microseconds (us).
 */

/* ---- Wiring ---- */
#define TRIG_PORT   GPIO_PORT_D
#define TRIG_PIN    7

#define ECHO_DDR    DDRB
#define ECHO_PIN    PB0    /* ICP1 */

/* ---- Timer1 assumptions (must match your servo.c) ----
 * 20ms period, prescaler 8 => 2 MHz timer => 40000 counts per 20ms.
 * In Mode 15, OCR1A is set to TOP-1 => 39999.
 */
#define TIMER1_COUNTS_PER_FRAME  40000u   /* full wrap span */
#define TICKS_TO_US_DIV2(ticks)  (((ticks) + 1u) >> 1)  /* us = ticks/2 with rounding */

/* ---- Measurement state ---- */
static volatile uint16_t icr_start = 0u;
static volatile uint8_t  waiting_rise = 1u;

volatile uint16_t pulse_width = 0u;          /* in microseconds */
volatile uint8_t  measurement_complete = 0u;

/* Non-blocking timeout support:
 * We store the Timer1 count at trigger time and consider it timed out if
 * too many counts elapse with no completion.
 *
 * Typical HC-SR04 max echo ~ 23ms. We set timeout ~ 30ms.
 * 30ms at 2MHz => 60000 ticks. Since Timer1 wraps every 40000, we compare
 * wrap-safe and allow >1 wrap using a conservative check.
 */
static volatile uint16_t trig_tcnt = 0u;
static volatile uint8_t  in_flight = 0u;

static inline uint16_t tcnt_delta_wrap(uint16_t now, uint16_t start)
{
    if (now >= start) return (uint16_t)(now - start);
    return (uint16_t)((uint16_t)TIMER1_COUNTS_PER_FRAME - start + now);
}

void sensor_init(void)
{
    /* TRIG as output low */
    gpio_set_direction(TRIG_PORT, TRIG_PIN, GPIO_PIN_OUTPUT);
    gpio_write(TRIG_PORT, TRIG_PIN, GPIO_PIN_LOW);

    /* ECHO (ICP1) as input */
    ECHO_DDR &= (uint8_t)~(1 << ECHO_PIN);

    /* Input Capture: start on rising edge */
    TCCR1B |= (1 << ICES1);

    /* Optional: noise canceler */
    TCCR1B |= (1 << ICNC1);

    /* Clear pending capture flag safely */
    TIFR1 |= (1 << ICF1);

    /* Enable Input Capture interrupt */
    TIMSK1 |= (1 << ICIE1);

    waiting_rise = 1u;
    measurement_complete = 0u;
    in_flight = 0u;
}

void sensor_trigger(void)
{
    /* Start a new measurement */
    measurement_complete = 0u;
    waiting_rise = 1u;
    in_flight = 1u;

    /* Snapshot timer count for timeout tracking */
    trig_tcnt = TCNT1;

    /* Ensure we capture rising edge first */
    TCCR1B |= (1 << ICES1);
    TIFR1 |= (1 << ICF1);

    /* 10us trigger pulse */
    gpio_write(TRIG_PORT, TRIG_PIN, GPIO_PIN_LOW);
    _delay_us(2);
    gpio_write(TRIG_PORT, TRIG_PIN, GPIO_PIN_HIGH);
    _delay_us(10);
    gpio_write(TRIG_PORT, TRIG_PIN, GPIO_PIN_LOW);
}

/* Fast, non-blocking ISR */
ISR(TIMER1_CAPT_vect)
{
    uint16_t now = ICR1;

    if (waiting_rise) {
        icr_start = now;
        waiting_rise = 0u;

        /* Next capture: falling edge */
        TCCR1B &= (uint8_t)~(1 << ICES1);
    } else {
        uint16_t ticks;

        /* Wrap-safe delta within the 20ms frame */
        if (now >= icr_start) {
            ticks = (uint16_t)(now - icr_start);
        } else {
            ticks = (uint16_t)((uint16_t)TIMER1_COUNTS_PER_FRAME - icr_start + now);
        }

        /* Simple noise reject: ignore extremely tiny pulses */
        if (ticks < 20u) {  /* <10us at 0.5us/tick */
            waiting_rise = 1u;
            TCCR1B |= (1 << ICES1);
            return;
        }

        /* Convert ticks to microseconds: tick=0.5us => us=ticks/2 */
        pulse_width = (uint16_t)TICKS_TO_US_DIV2(ticks);

        measurement_complete = 1u;
        in_flight = 0u;

        /* Re-arm for next measurement */
        waiting_rise = 1u;
        TCCR1B |= (1 << ICES1);
    }
}

uint8_t sensor_is_measurement_complete(void)
{
    /* Non-blocking timeout: if we're in flight too long, mark complete with 0 */
    if (in_flight) {
        uint16_t dt = tcnt_delta_wrap(TCNT1, trig_tcnt);

        /* 30ms timeout:
         * 30ms at 2MHz = 60000 ticks.
         * Since Timer1 wraps at 40000, we can't measure 60000 directly with one wrap-safe delta.
         * But if dt exceeds ~30000 within a frame, we're already past 15ms; if we hit that twice
         * we would have completed earlier. For simplicity: treat >30000 (~15ms) as timeout in this architecture.
         *
         * If you need full 30ms range, we can track wraps in a Timer0 tick instead.
         */
        if (dt > 30000u) { /* ~15ms */
            in_flight = 0u;
            pulse_width = 0u;
            measurement_complete = 1u;

            /* Re-arm rising edge */
            waiting_rise = 1u;
            TCCR1B |= (1 << ICES1);
        }
    }

    return measurement_complete;
}

uint16_t sensor_get_pulse_width(void)
{
    measurement_complete = 0u;
    return pulse_width;
}
