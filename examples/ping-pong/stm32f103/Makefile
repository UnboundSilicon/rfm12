PROJECT   = ping-pong
BUILD_DIR = build

CPU       = cortex-m3
DEVICE    = STM32F103xB

CC        = arm-none-eabi-gcc
OBJCOPY   = arm-none-eabi-objcopy
STFLASH   = st-flash

ifndef CMSISBASE
$(error CMSISBASE is not defined)
endif

DEVICE_DIR = $(CMSISBASE)/Device/STM32F1xx
STARTUP    = $(DEVICE_DIR)/src/startup_stm32f103xb.s
LINKER     = $(DEVICE_DIR)/linker/STM32F103XB_FLASH.ld

SOURCES = ../main.c \
          platform.c \
          ../../../src/rfm12.c \
          $(DEVICE_DIR)/src/system_stm32f1xx.c \
          $(STARTUP)

INCLUDES = -I.. \
           -I../../../include \
           -I$(CMSISBASE)/include \
           -I$(DEVICE_DIR)/include

CFLAGS = -mcpu=$(CPU) \
         -mthumb \
         -D$(DEVICE) \
         -Os \
         -Wall \
         -Wextra \
         -std=c11 \
         -ffunction-sections \
         -fdata-sections \
         $(INCLUDES)

LDFLAGS = -mcpu=$(CPU) \
          -mthumb \
          -T$(LINKER) \
          --specs=nosys.specs \
          --specs=nano.specs \
          -Wl,--gc-sections

ELF = $(BUILD_DIR)/$(PROJECT).elf
BIN = $(BUILD_DIR)/$(PROJECT).bin

.PHONY: all flash clean

all: $(BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ELF): $(SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

flash: $(BIN)
	$(STFLASH) write $< 0x08000000

clean:
	rm -rf $(BUILD_DIR)
