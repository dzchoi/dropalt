* dfu-suffix
  dfu-util: Invalid DFU suffix signature
  dfu-util: A valid DFU suffix will be required in a future dfu-util release!!!
  https://dfu-util.sourceforge.net/dfu-suffix.1.html

* log_write()
  Save logs that occur while dte is disconnected, and show them when connected.
  Patch RIOT error handlers (assert and panic) to use LOG_ERROR() instead of printf().
  Save the error logs (in backup RAM, not in NVM) to show them on reboot. Check during reboot if watchdog reset, memory bank switch reset, or if a special error-indicating flag is set.

* Redefine Riot-independent #define constants using "static const" and "static inline".
