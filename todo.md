* dfu-suffix
  dfu-util: Invalid DFU suffix signature
  dfu-util: A valid DFU suffix will be required in a future dfu-util release!!!
  https://dfu-util.sourceforge.net/dfu-suffix.1.html

* Allocate variables in .noinit section (NOINIT) unless initialization is necessary.

* log_write
  Show the saved log messages on reboot in cases of a watchdog reset, memory bank switch reset, or if a specific error-indicating flag is set.

* Check max stack size.

* Use likely(x) (== __builtin_expect((uintptr_t)(x), 1)) if appropriate.

* Redefine Riot-independent #define constants using "static const" and "static inline".

* Use MODULE_CORE_IDLE_THREAD to enable CPU sleep when idle.
