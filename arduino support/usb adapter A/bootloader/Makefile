###############################################################################
# Makefile for Generation 7 Electronics USB adapter
###############################################################################

## General Flags
PROJECT = bootloader

#MCU = attiny45
MCU = attiny85

F_CPU = 16500000UL

BUILDDIR = build

AVRDUDE = avrdude
AVRDUDEFLAGS = -c stk500v2 -p $(MCU) -P /dev/ttyACM0
AVRDUDEFLAGSFAST = $(AVRDUDEFLAGS) -B 1
AVRDUDEFLAGSSLOW = $(AVRDUDEFLAGS) -B 5

TARGET = $(PROJECT).hex
CC = avr-gcc

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON) -DBOOTLOADER
CFLAGS += -Wall -gdwarf-2 -DF_CPU=$(F_CPU) -Os -fsigned-char
CFLAGS += -fno-move-loop-invariants
CFLAGS += -fno-tree-scev-cprop

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS += 

## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings

## Include Directories
INCLUDES = -I".." -I"../usbdrv" -I"../libs-device"

## Objects that must be built in order to link
OBJECTS = main.o usbdrv.o usbdrvasm.o osccal.o
BUILDOBJECTS = $(addprefix $(BUILDDIR)/,$(OBJECTS))

## Objects explicitly added by the user
LINKONLYOBJECTS = 


## Build
all: $(BUILDDIR) $(TARGET) $(BUILDDIR)/$(PROJECT).elf \
     $(BUILDDIR)/$(PROJECT).eep $(BUILDDIR)/$(PROJECT).lss size

## Program
program: $(PROJECT).hex
	$(AVRDUDE) $(AVRDUDEFLAGSFAST) -U flash:w:$^

# For fuses, see http://www.engbedded.com/fusecalc/
fuses:
	$(AVRDUDE) $(AVRDUDEFLAGSSLOW) -U lfuse:w:0xE1:m -U hfuse:w:0xDC:m

## Compile
.SUFFIXES:
.SUFFIXES: .c .S .o
$(shell mkdir -p $(BUILDDIR))

$(BUILDDIR)/main.o: main.c ../usbdrv/usbdrv.h
	$(CC) $(INCLUDES) $(CFLAGS) -c  $< -o $@

$(BUILDDIR)/usbdrvasm.o: ../usbdrv/usbdrvasm.S ../usbdrv/usbdrv.h ../usbconfig.h
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $< -o $@

$(BUILDDIR)/usbdrv.o: ../usbdrv/usbdrv.c ../usbdrv/usbdrv.h ../usbconfig.h
	$(CC) $(INCLUDES) $(CFLAGS) -c  $< -o $@

$(BUILDDIR)/osccal.o: ../libs-device/osccal.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $< -o $@

##Link
.SUFFIXES: .elf .eep .lss .hex
$(BUILDDIR)/%.elf: $(BUILDOBJECTS)
	$(CC) $(LDFLAGS) $(BUILDOBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $@

%.hex: $(BUILDDIR)/%.elf
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS) $< $@

$(BUILDDIR)/%.eep: $(BUILDDIR)/%.elf
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

$(BUILDDIR)/%.lss: $(BUILDDIR)/%.elf
	avr-objdump -h -S $< > $@

size: $(BUILDDIR)/$(PROJECT).elf
	@echo
	avr-size -C --mcu=$(MCU) $(BUILDDIR)/$(PROJECT).elf

## Clean target
.PHONY: clean
clean:
	-rm -rf $(BUILDDIR) $(TARGET)
