# Main makefile for building the firmware for Drop Alt

# Application name will be used for .bin, .elf, and .map files.
APPLICATION = dropalt-0.1

# Directory for RIOT repo.
RIOTBASE := $(CURDIR)/riot

BOARD := board-dropalt
EXTERNAL_BOARD_DIRS := $(CURDIR)

# All output files will generate in qmk_firmware/.build/board-alt by default.
BINDIRBASE ?= $(CURDIR)/.build

# See https://github.com/cortexm/baremetal/blob/master/CMakeLists.txt
# for compiler options for building embedded system.
CXXEXFLAGS += -std=c++17    # for inline (const) variables
CXXEXFLAGS += -fno-exceptions
CXXEXFLAGS += -fno-ms-extensions
CXXEXFLAGS += -fno-rtti     # We don't need RTTI as no ambiguous base classes are used.
CXXEXFLAGS += -fno-threadsafe-statics
CXXEXFLAGS += -fno-use-cxa-atexit
# INCLUDES += -I...

# Peripherals and features to be used from the board.
FEATURES_REQUIRED += cpp  # Todo: "cpp libstdcpp" ???
FEATURES_REQUIRED += periph_adc_get
FEATURES_REQUIRED += periph_gpio_irq
FEATURES_REQUIRED += periph_matrix
FEATURES_REQUIRED += periph_sr_595
FEATURES_REQUIRED += periph_usb2422
FEATURES_REQUIRED += periph_wdt

# Enable DMA for I2C transfer
FEATURES_REQUIRED += periph_dma     # will #define MODULE_PERIPH_DMA

# Todo:
# /* The Cortex-m0 based ATSAM devices can use the Single-cycle I/O Port for GPIO.
# * When used, the gpio_t is mapped to the IOBUS area and must be mapped back to
# * the peripheral memory space for configuration access. When it is not
# * available, the _port_iobus() and _port() functions behave identical.
# */
# FEATURES_OPTIONAL += PERIPH_GPIO_FAST_READ

FEATURES_HPP = features.hpp

# Subdirectory modules
EXTERNAL_MODULE_DIRS += $(CURDIR)
USEMODULE += keymap

# Todo: Separate usb module from the application directory.

# RIOT modules
# USEMODULE += cpp11-compat
USEMODULE += usbus
DISABLE_MODULE += usbus_hid         # We use our own implementation of usbus_hid,
DISABLE_MODULE += auto_init_usbus   # and do not execute auto_init_usb().
$(shell grep -q 'VIRTSER_ENABLE = true' $(FEATURES_HPP))
ifeq ($(.SHELLSTATUS),0)
    USEMODULE += stdio_cdc_acm      # Use CDC ACM as default STDIO.
else
    USEMODULE += stdio_null         # Or, avoid using stdio and UART at all.
endif
USEMODULE += xtimer

# Configure linker script
ROM_START_ADDR  = 0x00000000
ROM_LEN         = 0x00040000  # 256K
ROM_OFFSET      = 0x00004000  # The first 16K is occupied by bootloader.
RAM_START_ADDR  = 0x20000000
RAM_LEN         = 0x00020000  # 128K
BACKUP_RAM_ADDR = 0x47000000
BACKUP_RAM_LEN  = 0x00002000  # 8K

VERBOSE_ASSERT := 1
# Enabling VERBOSE_ASSERT will set:
#  - DEVELHELP := 1                    # Enable assert() and SCHED_TEST_STACK.
#  - CFLAGS += -DDEBUG_ASSERT_VERBOSE  # assert() will print the filename and line number.
QUIET ?= 1

# PROGRAMMER = jlink

include $(RIOTBASE)/Makefile.include
