MODULE = board

SRC := board.c

ifneq (,$(filter is31fl3733,$(FEATURES_USED)))
    SRC += is31fl3733.c
endif
ifneq (,$(filter periph_adc_get,$(FEATURES_USED)))
    SRC += adc_get.c
endif
ifneq (,$(filter periph_matrix,$(FEATURES_USED)))
    SRC += matrix.c
endif
ifneq (,$(filter periph_sr_595,$(FEATURES_USED)))
    SRC += sr_595.c
endif
ifneq (,$(filter periph_usb2422,$(FEATURES_USED)))
    SRC += usb2422.c
endif

include $(RIOTBASE)/Makefile.base
