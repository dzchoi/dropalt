#### 4 threads
* There are 4 threads running concurrently.
  ```
  > fw.ps()
  pid   name              state     pri  lr        pc        stack usage
    -   isr_stack         -           -  -         -         576/1024
    1  >main              running     7  running   running   1252/2048
    2   usbhub_thread     bl anyfl    3  00004E6D  00004E7C  568/1024
    3   usbus             pending     1  00004E6D  00004E7C  948/1024
    4   matrix_thread     bl mutex    2  00009A39  00009A58  572/1024
  ```

* The boot code (`board_init()` and `main()`) executes in Thread mode (`sched_active_thread == NULL`), but using the Main Stack Pointer (MSP) instead of the Process Stack Pointer (PSP).
* The "main" thread runs the Lua interpreter, handling keymap and lamp events, as well as the Lua REPL. While executing in Lua, other signals like USB suspend are delayed until Lua execution is finished.
* The `usbhub_thread` manages the USB hub state machine, detecting and controlling port connections via ADC measurements. While ADC measurements are scheduled via interrupts and don't require a dedicated thread, the resulting notifications must be handled promptly, justifying the need of this thread.
* The `matrix_thread` monitors the state of each physical key using both interrupt and polling modes. It acts solely as an event producer and does not process signals.
* The "usbus" thread handles the USB protocol stack, supporting both USB HID and CDC ACM classes.
