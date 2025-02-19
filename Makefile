# Main makefile for building the firmware for Drop Alt

# Usage:
#  - `make riotboot/slot0`: builds .build/board-dropalt/riotboot_files/slot0.$(APP_VER).bin
#  - `make riotboot/flash-slot0`: builds it and flashes it to slot 0 using dfu-util.
#  - `make PROGRAMMER=edgb riotboot/flash-slot0`


APPLICATION := dropalt-fw

FEATURES_HPP := features.hpp

# Version numbers encoded in USB device descriptor
DEVICE_VER = $(shell grep -oP '(?<=DEVICE_VER = )[^;]*' $(FEATURES_HPP))
HUB_DEVICE_VER := 0x2410        # bcdDevice = 24.10 for Hub
CFLAGS += -DDEVICE_VER=$(DEVICE_VER) -DHUB_DEVICE_VER=$(HUB_DEVICE_VER)

# Version number (in decimal) embedded in the slot header.
APP_VER := $(shell printf "%d" $(DEVICE_VER))

# Directory for RIOT repo
RIOTBASE := $(CURDIR)/riot

# All output files will generate in .build/board-alt by default.
BINDIRBASE ?= $(CURDIR)/.build

EXTERNAL_BOARD_DIRS := $(CURDIR)
BOARD := board-dropalt

# The default ISR_STACKSIZE (=512 bytes) is not enough for LOG_ERROR() in
# hard_fault_handler().
CFLAGS += -DISR_STACKSIZE=THREAD_STACKSIZE_DEFAULT  # 1024

# See https://github.com/cortexm/baremetal/blob/master/CMakeLists.txt
# for compiler options for building ARM embedded system.
CXXEXFLAGS += -std=c++17    # for C++ features such as inline (const) variables
CXXEXFLAGS += -fno-exceptions
CXXEXFLAGS += -fno-ms-extensions
CXXEXFLAGS += -fno-rtti     # We don't need RTTI as no ambiguous base classes are used.
CXXEXFLAGS += -fno-threadsafe-statics
CXXEXFLAGS += -fno-use-cxa-atexit
# INCLUDES += -I...

# Peripherals and features to be used from the board.
FEATURES_REQUIRED += cpp  # "cpp libstdcpp" ???
FEATURES_REQUIRED += riotboot

# RIOT modules
# USEMODULE += newlib_nano          # used by default
USEMODULE += ztimer
USEMODULE += ztimer_msec

# Subdirectory modules
EXTERNAL_MODULE_DIRS += $(CURDIR)
USEMODULE += dropalt_sr_595
USEMODULE += dropalt_usb2422

$(shell grep -q 'VIRTSER_ENABLE = true' $(FEATURES_HPP))
ifeq ($(.SHELLSTATUS),0)
    USEMODULE += stdio_cdc_acm      # Use CDC ACM as default STDIO.
    VERBOSE_ASSERT := 1
    # Enabling VERBOSE_ASSERT will set:
    #  - DEVELHELP := 1                    # Enable assert() and SCHED_TEST_STACK.
    #  - CFLAGS += -DDEBUG_ASSERT_VERBOSE  # assert() will print the filename and line number.

    QUIET ?= 1

    # LOG_LEVEL = LOG_INFO by default, which filters out LOG_DEBUG.
    # Note also that LOG_ERROR() will save the log messages in NVM, if not filtered out.
    LOG_LEVEL = LOG_DEBUG
else
    USEMODULE += stdio_null         # Or, avoid using stdio and UART at all.
endif

# Configure linker script (See Table 9-1. Physical Memory Map in SAM D5x data sheet.)
# ROM_START_ADDR  = 0x00000000
# Total NVM size = NVMCTRL->PARAM.bit.NVMP * 512 (= 256KB)
# SmartEEPROM raw space size = 2 * NVMCTRL->SEESTAT.bit.SBLK * 8K (= 16KB)
# ROM_LEN         = 0x0003C000  # 256K - 16K
# ROM_OFFSET      = 0x00004000  # The first 16K is occupied by bootloader.
# RAM_START_ADDR  = 0x20000000
# RAM_LEN         = 0x00020000  # 128K
# We can still access the backup RAM (BBRAM) though Drop Alt does not connect it to
# a battery.
# BACKUP_RAM_ADDR = 0x47000000
# BACKUP_RAM_LEN  = 0x00002000  # 8K

PROGRAMMER = dfu-util
# PROGRAMMER = edbg  # uses EDBG (CMSIS-DAP) for flashing

include $(RIOTBASE)/Makefile.include
