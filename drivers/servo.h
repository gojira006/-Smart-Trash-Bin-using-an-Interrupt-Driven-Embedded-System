#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

/* --- Configuration (tune if needed) ---
 * Typical MG90S range is ~1000–2000 us, but many units support wider.
 * If your servo doesn't reach full travel, try 500–2500.
 */
#define SERVO_MIN_US   500u
#define SERVO_MAX_US   3000u

/* MG90S positional servo on OC1B (PB2 / Arduino D10) using Timer1 */
void servo_init(void);

/* Attach/detach the PWM output on OC1B */
void servo_enable(void);
void servo_disable(void);

uint8_t servo_is_enabled(void);

/* Position control */
void servo_set_pulse_us(uint16_t us);
void servo_set_angle(uint8_t angle_deg);

/* Project-friendly wrappers */
void servo_rotate_open(void);   /* default: 90 deg */
void servo_rotate_close(void);  /* default: 0 deg  */
void servo_stop(void);          /* disables PWM output */

#endif /* SERVO_H */
