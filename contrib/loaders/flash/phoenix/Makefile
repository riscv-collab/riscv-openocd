BIN2C = ../../../../src/helper/bin2char.sh
RM = rm -fr

CROSS_COMPILE ?= riscv-none-embed-
GCC     = $(CROSS_COMPILE)gcc
ASM     = $(CROSS_COMPILE)as
OBJCOPY = $(CROSS_COMPILE)objcopy

ASMFLAGS = -march=rv32emac -mabi=ilp32e
CFLAGS = -march=rv32emac -mabi=ilp32e
CFLAGS += -ffunction-sections -fdata-sections 
CFLAGS += -msmall-data-limit=8
CFLAGS += -Wall
CFLAGS += -O2 -fno-builtin  -nostdlib


all: write.inc

write.elf: entry.o write.o 
	$(GCC) $(CFLAGS) -o $@ $^ -Wl,-T"ram.lds"

%.o: %.c
	$(GCC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(ASM) $(ASMFLAGS) -o $@ $<

%.bin: %.elf
	$(OBJCOPY) -Obinary $< $@

%.inc: %.bin
	$(BIN2C) < $< > $@

clean:
	@echo 'Cleaning Targets and Build Artifacts'
	$(RM) *.inc *.bin *.elf *.map *.o *.d

.PRECIOUS: %.bin

.PHONY: all clean
