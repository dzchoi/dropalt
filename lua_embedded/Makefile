SRCXX = lexecute.cpp lfwlib.cpp lkeymap.cpp lua.cpp main_key_events.cpp timer.cpp

ifneq (,$(filter stdio_cdc_acm,$(USEMODULE)))
    SRCXX += timed_stdin.cpp
endif

ifneq (,$(filter lua_repl,$(USEMODULE)))
    SRCXX += repl.cpp
endif

include $(RIOTBASE)/Makefile.base
