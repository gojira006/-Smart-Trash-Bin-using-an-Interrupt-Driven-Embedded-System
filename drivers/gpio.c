#include "gpio.h"

// Helper arrays to map enum to actual registers
static volatile uint8_t* ddr_registers[] = {&DDRB, &DDRC, &DDRD};
static volatile uint8_t* port_registers[] = {&PORTB, &PORTC, &PORTD};
static volatile uint8_t* pin_registers[] = {&PINB, &PINC, &PIND};

void gpio_set_direction(GpioPort_t port, uint8_t pin_num, GpioDirection_t direction) {
    if (direction == GPIO_PIN_OUTPUT) {
        *ddr_registers[port] |= (1 << pin_num);
    } else {
        *ddr_registers[port] &= ~(1 << pin_num);
    }
}

void gpio_write(GpioPort_t port, uint8_t pin_num, GpioValue_t value) {
    if (value == GPIO_PIN_HIGH) {
        *port_registers[port] |= (1 << pin_num); // Set bit -> HIGH
    } else {
        *port_registers[port] &= ~(1 << pin_num); // Clear bit -> LOW
    }
}

void gpio_set_pullup(GpioPort_t port, uint8_t pin_num, uint8_t enable) {
    // HINT: Enabling a pull-up is the same as writing HIGH to an input pin.
    gpio_write(port, pin_num, enable ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
}

void gpio_toggle(uint8_t port, uint8_t pin) {
    volatile uint8_t *port_reg;

    switch (port) {
        case GPIO_PORT_B: port_reg = &PORTB; break;
        case GPIO_PORT_C: port_reg = &PORTC; break;
        case GPIO_PORT_D: port_reg = &PORTD; break;
        default: return; // invalid port
    }

    *port_reg ^= (1 << pin); // XOR toggles the bit
}

GpioValue_t gpio_read(GpioPort_t port, uint8_t pin_num) {
    if ((*pin_registers[port]) & (1 << pin_num)) {
        return GPIO_PIN_HIGH;   // Bit is set
    } else {
        return GPIO_PIN_LOW;    // Bit is cleared
    }
}