# Smart Trash Bin with Fill-Level Monitoring

Touchless smart trash bin with automatic lid control and real-time fill-level monitoring, built on an ATmega328P using interrupt-driven embedded C.

![Alt text](https://imgur.com/a/IiXgazg)

## What It Does

The bin opens its lid automatically when it detects a hand nearby and closes it a few seconds after the hand is removed — no touching required. At the same time, it continuously tracks how full the bin is and shows the fill level on an LCD screen. The goal is to reduce contact with shared trash bins and give a clear, at-a-glance indication of when the bin needs to be emptied.

## Tech Stack

- **MCU:** ATmega328P (Arduino Uno), programmed in bare-metal embedded C
- **Sensors:** HC-SR04 ultrasonic sensor (hand detection), Sharp GP2Y0A41SK0F IR distance sensor (fill-level)
- **Actuator:** MG996R servo motor (lid)
- **Display:** 16×2 I²C LCD
- **Enclosure:** Custom 3D-printed lid with mounted sensors and servo

## How It Works

The firmware is a non-blocking, interrupt-driven system built around a 3-state finite state machine (`STATE_CLOSED`, `STATE_OPEN`, `STATE_DEBOUNCE`). No blocking delays are used anywhere — all timing is handled through hardware interrupts, so the system stays responsive at all times.

**Three hardware timers do the heavy lifting:**
| Timer | Role |
|---|---|
| Timer 0 | 1ms system heartbeat / software timers |
| Timer 1 | Ultrasonic pulse capture (sensor data acquisition) |
| Timer 2 | Servo PWM generation (lid actuation) |

The ADC reads the Sharp IR sensor for fill-level, and the I²C (TWI) peripheral drives the LCD.

**System architecture (4 layers):**
1. **Hardware** — MCU, sensors, display, GPIO
2. **Firmware** — ISRs, ADC, I²C drivers
3. **Interface** — reusable modules (servo control, ultrasonic, IR distance, LCD, UART)
4. **Application** — FSM logic, hand detection, fill-level calculation, display management

## Hardware Setup

- Core controller: Arduino Uno (ATmega328P)
- Inputs: HC-SR04 (hand detection), Sharp GP2Y0A41SK0F (fill monitoring)
- Outputs: MG996R servo (lid), I²C LCD (status display)
- Power: DC battery pack, with the servo powered separately to isolate its higher current draw from the rest of the system

<!-- Add your circuit/wiring diagram here if you have one -->
![System Architecture](docs/images/architecture.png)

## Repository Structure

```
├── main.c              # Main program loop / FSM
├── drivers/             # Interface layer: servo, ultrasonic, IR, LCD, UART
├── Makefile             # Build configuration
└── docs/                # Diagrams, images
```

## How to Build & Flash

```bash
make
avrdude -c arduino -p atmega328p -P <your_port> -U flash:w:main.hex
```
<!-- Adjust to match your actual Makefile/avrdude setup -->

## Results

<!-- Fill in with real numbers if you have them, e.g.: -->
- Hand detection response time: ~___ ms
- Fill-level measurement accuracy: ±___%
- Lid open/close cycle tested over ___ trials

## What I'd Improve

- Originally planned IoT-based remote fill-level reporting; excluded due to Wi-Fi complexity in bare-metal C — would revisit with an ESP-based co-processor
- Second-gen version could use a load cell instead of/alongside IR for more reliable fill detection

## Team

- Ricardo Jose G. Vicente
- Emmanuel B. Himongon

Course: COE185 Embedded Systems — MSU-IIT

## License

MIT
