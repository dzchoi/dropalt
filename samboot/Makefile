# Makefile for samboot  produces the SAM-BA boot image that mdloader can flash.
#
# The image is a combined binary that mdloader writes to the device via the
# SAM-BA protocol.  It consists of the tiny BKSWRST application followed by
# 0xFF padding and then the monolithic firmware.  After mdloader flashes it,
# the SAM-BA bootloader jumps to the BKSWRST app, which issues a Bank Swap and
# Reset (BKSWRST), making the monolithic firmware in Bank B the active image.
#
# Usage:
#   make             build BKSWRST app AND combined firmware
#   make combined    (re-)generate the combined firmware only
#   make clean       remove build artefacts
#
# NOTE: The monolithic firmware must be built first (make -j8 in dropalt/).

BINDIR  ?= .build

CROSS   ?= arm-none-eabi-
AS      := $(CROSS)as
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

ASFLAGS := -mcpu=cortex-m4 -mthumb -g
LDFLAGS := -T bkswrst.ld

BKSWRST_OBJ   := $(BINDIR)/bkswrst.o
BKSWRST_ELF   := $(BINDIR)/bkswrst.elf
BKSWRST_BIN   := $(BINDIR)/bkswrst.bin
MONOLITHIC_FW := $(CURDIR)/../.build/board-dropalt/dropalt-fw.bin
SAMBOOT_BIN   := $(BINDIR)/dropalt-samboot.bin

FW_IMAGE_OFFSET := 0x1C000

.PHONY: all combined clean

all: combined

$(BINDIR):
	mkdir -p $@

$(BKSWRST_OBJ): bkswrst.s | $(BINDIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BKSWRST_ELF): $(BKSWRST_OBJ) bkswrst.ld
	$(LD) $(LDFLAGS) $< -o $@

$(BKSWRST_BIN): $(BKSWRST_ELF)
	$(OBJCOPY) -O binary $< $@

combined: $(BKSWRST_BIN) $(MONOLITHIC_FW)
	sh combine.sh $(BKSWRST_BIN) $(MONOLITHIC_FW) $(SAMBOOT_BIN) $(FW_IMAGE_OFFSET)

clean:
	rm -rf $(BINDIR)
