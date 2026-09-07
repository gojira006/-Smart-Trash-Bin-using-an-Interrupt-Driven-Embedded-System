DEVICE     = atmega328p
CLOCK      = 16000000
PROGRAMMER = -c arduino -b 115200 -P COM3
FUSES      = -U hfuse:w:0xde:m -U lfuse:w:0xff:m -U efuse:w:0x05:m

TARGET = main

# Look for all .c files in root and drivers/
SRC = $(wildcard *.c) $(wildcard drivers/*.c)
OBJS = $(SRC:.c=.o)

# Needed for printf floating point
LDFLAGS = -Wl,-u,vfprintf -lprintf_flt -lm

AVRDUDE = avrdude $(PROGRAMMER) -p $(DEVICE)
CC = avr-gcc

# Add include path for drivers/
CFLAGS = -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE) -Idrivers
COMPILE = $(CC) $(CFLAGS)

# symbolic targets:
all: main.hex

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

flash: all
	$(AVRDUDE) -U flash:w:main.hex:i

fuse:
	$(AVRDUDE) $(FUSES)

install: flash fuse

load: all
	bootloadHID main.hex

clean:
	rm -f main.hex main.elf $(OBJS)

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
	avr-size --format=avr --mcu=$(DEVICE) main.elf

disasm: main.elf
	avr-objdump -d main.elf

cpp:
	$(COMPILE) -E main.c
