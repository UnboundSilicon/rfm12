PROJECT   = ping-pong
BUILD_DIR = build

MCU       = atmega328p
F_CPU     = 16000000UL

CC        = avr-gcc
OBJCOPY   = avr-objcopy
AVRDUDE   = avrdude

PROGRAMMER = arduino
PORT       = /dev/tty.usbserial-A900DIO1
BAUD       = 115200

SOURCES = ../main.c \
          platform.c \
          ../../../src/rfm12.c

INCLUDES = -I.. -I../../../include

CFLAGS = -mmcu=$(MCU) \
         -DF_CPU=$(F_CPU) \
         -Os \
         -Wall \
         -Wextra \
         -std=gnu11 \
         -ffunction-sections \
         -fdata-sections \
         $(INCLUDES)

LDFLAGS = -mmcu=$(MCU) \
          -Wl,--gc-sections \
          -Wl,-u,vfprintf \
          -lprintf_min

ELF = $(BUILD_DIR)/$(PROJECT).elf
HEX = $(BUILD_DIR)/$(PROJECT).hex

.PHONY: all flash clean

all: $(HEX)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ELF): $(SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

$(HEX): $(ELF)
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

flash: $(HEX)
	$(AVRDUDE) \
	    -c $(PROGRAMMER) \
	    -p $(MCU) \
	    -P $(PORT) \
	    -b $(BAUD) \
	    -U flash:w:$<:i

clean:
	rm -rf $(BUILD_DIR)