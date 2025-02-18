MODULE = board

SRC := board.c

# Each 'xxx.c' file in this directory will constitute a separate submodule named
# 'dropalt_xxx'.
BASE_MODULE := dropalt
SUBMODULES = 1

# The following submodules are defined:
# dropalt_adc_get       # dropalt_adc_get replaces periph_adc.
# dropalt_is31fl3733    # is31fl3733 rgb led driver
# dropalt_matrix        # keyboard matrix
# dropalt_seeprom       # SmartEEPROM
# dropalt_sr_595        # SR-595 shift register
# dropalt_usb2422       # USB2422 Hub

include $(RIOTBASE)/Makefile.base
