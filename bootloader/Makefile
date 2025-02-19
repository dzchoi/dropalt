# Main makefile for building the bootloader for Drop Alt

# Usage:
#  - `make`: builds .build/board-dropalt/dropalt-boot.bin
#  - `make flash`: also flashes it using EDBG (CMSIS-DAP)

# Note:
#  - 16KB is the bootloader size.
#  - `make flash` does not alter the slot header of slot 0 or slot 1. Flashing a binary
#    will do.
#  - `make flash` triggers an external reset through EDBG after flashing, which causes
#    the device to enter DFU mode.


APPLICATION := dropalt-boot

# Version numbers encoded in USB device descriptor
DEVICE_VER := 0x0080            # bcdDevice =  0.80 for bootloader
HUB_DEVICE_VER := 0x2410        # bcdDevice = 24.10 for Hub
CFLAGS += -DDEVICE_VER=$(DEVICE_VER) -DHUB_DEVICE_VER=$(HUB_DEVICE_VER)

RIOTBASE := $(CURDIR)/../riot

BINDIRBASE ?= $(CURDIR)/.build

EXTERNAL_BOARD_DIRS := $(CURDIR)/..
BOARD := board-dropalt

# RIOT modules
USEMODULE += riotboot_usb_dfu
USEMODULE += ztimer
USEMODULE += ztimer_msec
USEMODULE += ztimer_usec

# Subdirectory modules
EXTERNAL_MODULE_DIRS += $(CURDIR)/..
USEMODULE += dropalt_sr_595
USEMODULE += dropalt_usb2422

# Configure linker script (See Table 9-1. Physical Memory Map in SAM D5x data sheet.)
# ROM_START_ADDR  = 0x00000000
# Total NVM size = NVMCTRL->PARAM.bit.NVMP * 512 (= 256KB)
# ROM_LEN         = 0x00040000  # 256K
# RAM_START_ADDR  = 0x20000000
# RAM_LEN         = 0x00020000  # 128K
# We can still access the backup RAM (BBRAM) though Drop Alt does not connect it to
# a battery.
# BACKUP_RAM_ADDR = 0x47000000
# BACKUP_RAM_LEN  = 0x00002000  # 8K

include $(RIOTBASE)/bootloaders/riotboot_common.mk
