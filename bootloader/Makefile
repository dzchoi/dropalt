# Main makefile for building the bootloader for Drop Alt

# Usage:
#  - `make`: builds .build/board-dropalt/dropalt-boot.bin
#  - `make flash`: also flashes it using EDBG (CMSIS-DAP)

# Note: `make flash` triggers an external reset through EDBG after flashing, which
# causes the device to remain in DFU mode.


APPLICATION := dropalt-boot

# Version numbers encoded in USB device descriptor
DEVICE_VER := 0x0080            # bcdDevice =  0.80 for bootloader
HUB_DEVICE_VER := 0x2410        # bcdDevice = 24.10 for Hub
CFLAGS += -DDEVICE_VER=$(DEVICE_VER) -DHUB_DEVICE_VER=$(HUB_DEVICE_VER)

RIOTBASE := $(CURDIR)/../riot

BINDIRBASE ?= $(CURDIR)/.build

EXTERNAL_BOARD_DIRS := $(CURDIR)/..
BOARD := board-dropalt

# Enable Link-Time-Optimization.
LTO = 1

# RIOT modules
FEATURES_REQUIRED += no_idle_thread
FEATURES_REQUIRED += periph_pm
USEMODULE += riotboot_flashwrite
USEMODULE += usbus
USEMODULE += ztimer
USEMODULE += ztimer_msec
USEMODULE += ztimer_usec

# Subdirectory modules
EXTERNAL_MODULE_DIRS += $(CURDIR)/..
USEMODULE += dropalt_sr_595
USEMODULE += dropalt_usb2422
USEMODULE += dropalt_backup_ram

# No CDC ACM connected
LOG_LEVEL = LOG_NONE

include $(RIOTBASE)/bootloaders/riotboot_common.mk
