* dfu-suffix
  dfu-util: Invalid DFU suffix signature
  dfu-util: A valid DFU suffix will be required in a future dfu-util release!!!
  https://dfu-util.sourceforge.net/dfu-suffix.1.html

* ztimer_acquire():
  Make sure to call ztimer_acquire() before fetching the clock's current time.
  Calling ztimer_acquire before using ztimer_now() is the preferred way to guarantee that a clock is continuously active. Make sure to call the corresponding ztimer_release after the last ztimer_now() call.
  Make sure to call ztimer_acquire() before making use of ztimer_periodic_wakeup. After usage ztimer_release() should be called.

* Redefine Riot-independent #define constants using "static const" and "static inline".
