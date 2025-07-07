SRCXX =  key_queue.cpp lfwlib.cpp lkeymap.cpp lua.cpp timer.cpp

ifneq (,$(filter stdio_cdc_acm,$(USEMODULE)))
    SRCXX += timed_stdin.cpp
endif

ifneq (,$(filter lua_repl,$(USEMODULE)))
    SRCXX += repl.cpp
endif

include $(RIOTBASE)/Makefile.base
