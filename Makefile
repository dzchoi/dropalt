# Main makefile for building the firmware for Drop Alt

# Application name will be used for .bin, .elf, and .map files.
APPLICATION = dropalt-0.1

# Directory for RIOT repo.
RIOTBASE := $(CURDIR)/riot

BOARD := board-dropalt
EXTERNAL_BOARD_DIRS := $(CURDIR)

# All RIOT output files will generate in qmk_firmware/.build/board-alt by default.
BINDIRBASE ?= $(CURDIR)/.build

# CFLAGS += ...
# INCLUDES += -I...

# Peripherals and features to be used from the board.
FEATURES_REQUIRED += cpp
FEATURES_REQUIRED += periph_sr_595
FEATURES_REQUIRED += periph_wdt

# RIOT modules
# USEMODULE += usbus
USEMODULE += stdio_null               	# Or, stdio_cdc_acm
USEMODULE += xtimer

# Configure linker script
ROM_START_ADDR  = 0x00000000
ROM_LEN         = 0x00040000  # 256K
ROM_OFFSET      = 0x00004000  # The first 16K is occupied by bootloader.
RAM_START_ADDR  = 0x20000000
RAM_LEN         = 0x00020000  # 128K
BACKUP_RAM_ADDR = 0x47000000
BACKUP_RAM_LEN  = 0x00002000  # 8K

DEVELHELP ?= 0
QUIET ?= 1

# PROGRAMMER = jlink

include $(RIOTBASE)/Makefile.include
