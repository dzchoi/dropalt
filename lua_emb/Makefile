SRCXX =  lua.cpp lfwlib.cpp key_event_queue.cpp lkeymap.cpp
SRCXX += if.cpp map.cpp literal.cpp

ifneq (,$(filter stdio_cdc_acm,$(USEMODULE)))
    SRCXX += timed_stdin.cpp
endif

ifneq (,$(filter lua_repl,$(USEMODULE)))
    SRCXX += repl.cpp
endif

include $(RIOTBASE)/Makefile.base
