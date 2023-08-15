# Main makefile for building the firmware for Drop Alt

# Application name will be used for .bin, .elf, and .map files.
# The version number should match that in launch.json.
APPLICATION = dropalt-0.9.1

# Directory for RIOT repo.
RIOTBASE := $(CURDIR)/riot

BOARD := board-dropalt
EXTERNAL_BOARD_DIRS := $(CURDIR)

# All output files will generate in qmk_firmware/.build/board-alt by default.
BINDIRBASE ?= $(CURDIR)/.build

# See https://github.com/cortexm/baremetal/blob/master/CMakeLists.txt
# for compiler options for building embedded system.
CXXEXFLAGS += -std=c++17    # for C++ features such as inline (const) variables
CXXEXFLAGS += -fno-exceptions
CXXEXFLAGS += -fno-ms-extensions
CXXEXFLAGS += -fno-rtti     # We don't need RTTI as no ambiguous base classes are used.
CXXEXFLAGS += -fno-threadsafe-statics
CXXEXFLAGS += -fno-use-cxa-atexit
# INCLUDES += -I...

FEATURES_HPP = features.hpp

# Peripherals and features to be used from the board.
FEATURES_REQUIRED += cpp  # Todo: "cpp libstdcpp" ???
FEATURES_REQUIRED += periph_adc_get
FEATURES_REQUIRED += periph_gpio_irq
FEATURES_REQUIRED += periph_matrix
FEATURES_REQUIRED += periph_rtt         # For ZTIMER_MSEC
FEATURES_REQUIRED += periph_seeprom
FEATURES_REQUIRED += periph_sr_595
FEATURES_REQUIRED += periph_usb2422
FEATURES_REQUIRED += periph_wdt

# Enable DMA for I2C transfer
FEATURES_REQUIRED += periph_dma     # will #define MODULE_PERIPH_DMA

$(shell grep -q 'RGB_LED_ENABLE = true' $(FEATURES_HPP))
ifeq ($(.SHELLSTATUS),0)
    FEATURES_REQUIRED += is31fl3733
endif

# RIOT modules
# USEMODULE += cpp11-compat
# USEMODULE += newlib_nano          # used by default
USEMODULE += usbus
DISABLE_MODULE += usbus_hid         # We use our own implementation of usbus_hid,
DISABLE_MODULE += auto_init_usbus   # and do not execute auto_init_usb().
$(shell grep -q 'VIRTSER_ENABLE = true' $(FEATURES_HPP))
ifeq ($(.SHELLSTATUS),0)
    USEMODULE += stdio_cdc_acm      # Use CDC ACM as default STDIO.
else
    USEMODULE += stdio_null         # Or, avoid using stdio and UART at all.
endif
USEMODULE += ztimer
USEMODULE += ztimer_msec
USEMODULE += ztimer_usec            # will use ztimer_periph_timer (timer_config[0])

# Subdirectory modules
EXTERNAL_MODULE_DIRS += $(CURDIR)
USEMODULE += adc
USEMODULE += keymap
USEMODULE += log_write
USEMODULE += rgb
USEMODULE += usb

# Configure linker script (See Table 9-1. Physical Memory Map in SAM D5x data sheet.)
ROM_START_ADDR  = 0x00000000
# Total flash size = NVMCTRL->PARAM.bit.NVMP * 512 (= 256KB)
# SmartEEPROM raw space size = 2 * NVMCTRL->SEESTAT.bit.SBLK * 8K (= 16KB)
ROM_LEN         = 0x0003C000  # 256K - 16K
ROM_OFFSET      = 0x00004000  # The first 16K is occupied by bootloader.
RAM_START_ADDR  = 0x20000000
RAM_LEN         = 0x00020000  # 128K
# Backup RAM means the persistent memory powered by an external battery, but the keyboard
# does not enable it.
BACKUP_RAM_ADDR = 0x47000000
BACKUP_RAM_LEN  = 0x00002000  # 8K

# The default ISR_STACKSIZE (=512 bytes) is not enough for LOG_ERROR() in
# hard_fault_handler().
CFLAGS += -DISR_STACKSIZE=THREAD_STACKSIZE_DEFAULT

# The default THREAD_STACKSIZE_MAIN (=1536 bytes) is excessive.
CFLAGS += -DTHREAD_STACKSIZE_MAIN=THREAD_STACKSIZE_DEFAULT

VERBOSE_ASSERT := 1
# Enabling VERBOSE_ASSERT will set:
#  - DEVELHELP := 1                    # Enable assert() and SCHED_TEST_STACK.
#  - CFLAGS += -DDEBUG_ASSERT_VERBOSE  # assert() will print the filename and line number.
QUIET ?= 1

# LOG_LEVEL = LOG_INFO by default, which filters out LOG_DEBUG.
# Note also that LOG_ERROR() will save the log messages in NVM, if not filtered out.
LOG_LEVEL = LOG_DEBUG

# PROGRAMMER = jlink

include $(RIOTBASE)/Makefile.include
