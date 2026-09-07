#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdio.h>

#include "sensor.h"
#include "servo.h"
#include "sharp_ir.h"
#include "lcd_i2c.h"

#define HAND_DETECTION_THRESHOLD_CM   10u
#define STARTUP_IGNORE_MS             1000u

#define OPEN_TIME_MS                  800u
#define CLOSE_TIME_MS                 400u
#define WAIT_BEFORE_CLOSE_TICKS       10u

#define LCD_UPDATE_INTERVAL_TICKS     5u

/* MG90S positional targets */
#define SERVO_CLOSED_ANGLE            180u
#define SERVO_OPEN_ANGLE              0u

volatile uint32_t g_millis = 0u;
volatile uint8_t timer_flag = 0u;
volatile uint8_t measurement_ready = 0u;

static volatile uint8_t us_in_flight = 0u;
static uint8_t hand_present = 0u;
static uint32_t state_start_ms = 0u;
static uint16_t wait_counter = 0u;

/* IR fill */
static uint8_t fill_percentage = 0u;

/* Mean filter state for IR */
#define IR_MEAN_SAMPLES  5u
static uint16_t ir_buf[IR_MEAN_SAMPLES];
static uint8_t  ir_buf_idx = 0u;
static uint8_t  ir_buf_count = 0u;

/* Empty baseline calibration for IR */
#define IR_CALIB_SAMPLES  20u
static uint8_t  ir_calibrated = 0u;
static uint8_t  ir_calib_count = 0u;
static uint32_t ir_calib_sum = 0u;
static uint16_t ir_empty_cm = 30u;

/* LCD tracking */
static uint8_t lcd_needs_init = 1u;
static uint8_t lcd_startup_phase = 0u;
static uint32_t lcd_startup_ms = 0u;
static uint8_t lcd_tick_counter = 0u;
static uint8_t last_hand = 0xFFu;
static uint8_t last_fill = 0xFFu;

typedef enum {
    STATE_BOOT = 0,
    STATE_IDLE,
    STATE_OPENING,
    STATE_OPEN_HOLD,
    STATE_WAIT_TO_CLOSE,
    STATE_CLOSING
} SystemState;

static SystemState st = STATE_BOOT;

ISR(TIMER0_COMPA_vect)
{
    static uint8_t ms_counter = 0u;

    g_millis++;
    ms_counter++;

    if (ms_counter >= 100u) {
        ms_counter = 0u;
        timer_flag = 1u;
        measurement_ready = 1u;
    }
}

static void timer0_init(void)
{
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS01) | (1 << CS00);
    OCR0A  = 249;
    TIMSK0 = (1 << OCIE0A);
}

static uint16_t calculate_distance_cm(uint16_t pw)
{
    if (pw == 0u) return 9999u;
    return (uint16_t)(pw / 58u);
}

/* IR mapping constants */
#define BIN_HEIGHT_CM   30u
#define IR_MIN_CM       4u
#define IR_MAX_CM       30u

static void update_fill_level(void)
{
    uint16_t d_raw;
    uint32_t sum;
    uint16_t d_mean;
    uint8_t i;
    uint16_t span;

    d_raw = sharp_ir_read_distance_cm();

    /* mean filter buffer */
    ir_buf[ir_buf_idx] = d_raw;
    ir_buf_idx++;
    if (ir_buf_idx >= IR_MEAN_SAMPLES) ir_buf_idx = 0u;
    if (ir_buf_count < IR_MEAN_SAMPLES) ir_buf_count++;

    sum = 0u;
    for (i = 0u; i < ir_buf_count; i++) {
        sum += (uint32_t)ir_buf[i];
    }
    d_mean = (uint16_t)(sum / (uint32_t)ir_buf_count);

    /* empty baseline calibration */
    if (!ir_calibrated) {
        ir_calib_sum += (uint32_t)d_mean;
        ir_calib_count++;

        if (ir_calib_count >= IR_CALIB_SAMPLES) {
            ir_empty_cm = (uint16_t)(ir_calib_sum / (uint32_t)ir_calib_count);
            if (ir_empty_cm > IR_MAX_CM) ir_empty_cm = IR_MAX_CM;
            if (ir_empty_cm < (IR_MIN_CM + 1u)) ir_empty_cm = (IR_MIN_CM + 1u);
            ir_calibrated = 1u;
        }
    }

    if (!ir_calibrated) {
        fill_percentage = 0u;
        return;
    }

    span = (ir_empty_cm > IR_MIN_CM) ? (uint16_t)(ir_empty_cm - IR_MIN_CM) : 1u;

    if (d_mean >= ir_empty_cm) fill_percentage = 0u;
    else if (d_mean <= IR_MIN_CM) fill_percentage = 100u;
    else {
        fill_percentage = (uint8_t)(((uint32_t)(ir_empty_cm - d_mean) * 100u) / (uint32_t)span);
        if (fill_percentage > 100u) fill_percentage = 100u;
    }
}

/* LCD helpers */
static void lcd_init_safe(uint32_t now_ms)
{
    if (lcd_needs_init) {
        lcd_init();
        lcd_clear();
        lcd_set_cursor(0, 0);
        lcd_print("Smart Trash Bin");
        lcd_set_cursor(1, 0);
        lcd_print("Initializing...");
        lcd_needs_init = 0u;
        lcd_startup_phase = 1u;
        lcd_startup_ms = now_ms;
    }

    if (lcd_startup_phase == 1u) {
        if ((uint32_t)(now_ms - lcd_startup_ms) >= 1000u) {
            lcd_clear();
            lcd_startup_phase = 2u;
        }
    }
}

static void lcd_update_safe(uint8_t hand, uint8_t fill)
{
    char line2[17];

    if (hand != last_hand) {
        lcd_set_cursor(0, 0);
        if (hand) lcd_print("HAND DETECTED ");
        else      lcd_print("WAITING...    ");
        last_hand = hand;
    }

    if (fill != last_fill) {
        snprintf(line2, sizeof(line2), "FILL: %3u%%     ", (unsigned)fill);
        lcd_set_cursor(1, 0);
        lcd_print(line2);
        last_fill = fill;
    }
}

int main(void)
{
    uint32_t now_ms;
    uint32_t boot_start_ms;
    uint16_t dist;

    timer0_init();
    servo_init();      /* OC1B/D10, Timer1 mode 15, TOP=OCR1A, prescaler 8 */
    sensor_init();
    sharp_ir_init();

    sei();

    /* hold closed at startup */
    servo_set_angle(SERVO_CLOSED_ANGLE);

    boot_start_ms = g_millis;
    st = STATE_BOOT;

    while (1) {
        now_ms = g_millis;

        lcd_init_safe(now_ms);

        if (measurement_ready) {
            measurement_ready = 0u;

            if (!us_in_flight) {
                sensor_trigger();
                us_in_flight = 1u;
            }

            update_fill_level();
        }

        if (us_in_flight && sensor_is_measurement_complete()) {
            dist = calculate_distance_cm(sensor_get_pulse_width());
            hand_present = (dist > 2u && dist <= HAND_DETECTION_THRESHOLD_CM) ? 1u : 0u;
            us_in_flight = 0u;
        }

        if (timer_flag) {
            timer_flag = 0u;

            lcd_tick_counter++;
            if (lcd_tick_counter >= LCD_UPDATE_INTERVAL_TICKS) {
                lcd_tick_counter = 0u;
                if (lcd_startup_phase >= 2u) {
                    lcd_update_safe(hand_present, fill_percentage);
                }
            }

            switch (st) {
                case STATE_BOOT:
                    if ((uint32_t)(now_ms - boot_start_ms) >= STARTUP_IGNORE_MS) {
                        st = STATE_IDLE;
                    }
                    break;

                case STATE_IDLE:
                    /* Removed continuous servo_set_angle(CLOSED) here */
                    if (hand_present) {
                        servo_set_angle(SERVO_OPEN_ANGLE);
                        state_start_ms = now_ms;
                        st = STATE_OPENING;
                    }
                    break;

                case STATE_OPENING:
                    if ((uint32_t)(now_ms - state_start_ms) >= OPEN_TIME_MS) {
                        st = STATE_OPEN_HOLD;
                    }
                    break;

                case STATE_OPEN_HOLD:
                    if (!hand_present) {
                        wait_counter = 0u;
                        st = STATE_WAIT_TO_CLOSE;
                    }
                    break;

                case STATE_WAIT_TO_CLOSE:
                    if (hand_present) {
                        st = STATE_OPEN_HOLD;
                    } else if (wait_counter >= WAIT_BEFORE_CLOSE_TICKS) {
                        servo_set_angle(SERVO_CLOSED_ANGLE);
                        state_start_ms = now_ms;
                        st = STATE_CLOSING;
                    } else {
                        wait_counter++;
                    }
                    break;

                case STATE_CLOSING:
                    if ((uint32_t)(now_ms - state_start_ms) >= CLOSE_TIME_MS) {
                        st = STATE_IDLE;
                    }
                    break;

                default:
                    st = STATE_IDLE;
                    break;
            }
        }
    }

}
