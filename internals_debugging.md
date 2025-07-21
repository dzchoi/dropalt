#### System reset
$ openocd -f `f openocd.cfg` -c "reset; exit"

#### Boot into bootloader
$ `f edbg` -bt samd51 -x 10

#### Dump symbols
Useful to check for generating duplicate memories:  
$ arm-none-eabi-objdump -t *.elf | sort

Sort the data size:  
$ arm-none-eabi-nm --print-size --size-sort --radix=d *.elf

Size of memory that a class instance occupies:  
Search for the variable name in .elf

#### Disassemble
$ arm-none-eabi-objdump -S cpu.o >cpu.s

$ arm-none-eabi-objdump -d `f slot0.elf`|less

#### Get compiler options
Should be compiled with '-g'.
See [Get the compiler options from a compiled executable?](https://stackoverflow.com/a/65507874/10451825)  
$ arm-none-eabi-readelf --debug-dump=info ./*.elf |less

#### View logs in real-time
$ tio -mINLCRNL `ls -t /dev/ttyACM* |head -n1`

$ while true; do cat /dev/ttyACMx 2>/dev/null; done

#### Retrieve logs in DFU mode
$ dfu-util -a0 -U filename

However, `dfu-util -a0 -U >(less)` seems not supported (yet).

#### Handling Hardfault (vector_cortexm.c)
```
  cortex_vector_base[2]
  -> hard_fault_default()
       # ifdef DEVELHELP:
       -> hard_fault_handler()
            -> core_panic()

       # ifndef DEVELHELP:
       -> core_panic()
```

#### core_panic(core_panic_t crash_code, const char *message)
```
  -> panic_app(crash_code, message)
  -> panic_arch() in riot/cpu/cortexm_common/panic.c
      pauses at __asm__("bkpt #0") if a debugger is connected
  -> pm_reboot() if CONFIG_CORE_REBOOT_ON_PANIC && defined(MODULE_PERIPH_PM)
  -> OR usb_board_reset_in_bootloader() if defined(MODULE_USB_BOARD_RESET)
  -> OR pm_off() if defined(MODULE_PERIPH_PM)
      does while(1) {} on SAMD51
  -> OR while(1) {}
```

#### assert()
```
  -> Do nothing #ifdef NDEBUG
  -> OR _assert_failure() #ifdef DEBUG_ASSERT_VERBOSE
      -> print filename and line number
  -> OR _assert_panic()
      -> print the address where the assert has failed
  -> print a backtrace if MODULE_BACKTRACE is enabled
  -> trigger a debug breakpoint #ifdef DEBUG_ASSERT_BREAKPOINT
  -> core_panic()
```
The address can be used with tools like `addr2line` (or e.g. `arm-none-eabi-addr2line` for ARM-based code), `objdump`, or `gdb` (with the command `info line *(0x89abcdef)`) to identify the line the assertion failed in.

#### Autonomous conditional break point
```
static int count = 0;
if ( ++count == 10 )
    __asm__("bkpt #0");
```

#### DEVELHELP
set `DEVELHELP = 1` in Makefile along with `CFLAGS += -DDEBUG_ASSERT_VERBOSE`.

#### SCHED_TEST_STACK, THREAD_CREATE_STACKTEST, DEBUG_EXTRA_STACKSIZE, DEBUG_BREAKPOINT(), PANIC_STACK_OVERFLOW

#### Debugging switchover using CDC-ACM
* Connect to the same host with both ports.
* The same serial (e.g. /dev/ttyACM0) in the host can be used for CDC-ACM.

#### Stack overflow
* Use thread_measure_stack_free() for threads created with THREAD_CREATE_STACKTEST.
* Use thread_stack_print() and thread_print_stack().
* See test_utils_print_stack_usage() in the module with the same name.

Example. Stack overflowed while executing vsnprintf() in backup_ram::write().
```
(gdb) mon reset run
[at91samd51j18.cpu] halted due to debug-request, current mode: Handler MemManage
xPSR: 0x81000004 pc: 0x000052c4 msp: 0x200003a8
(gdb) info threads
[New Thread 2]
[New Thread 3]
  Id   Target Id                                               Frame 
* 1    Thread 1 "main" (Name: main, Blocked any flag)          panic_arch ()
    at /home/stem/projects/atsamd51/dropalt/riot/cpu/cortexm_common/panic.c:49
  2    Thread 2 "usbus" (Name: usbus, Blocked any flag)        0x0000502c in _thread_flags_wait_any (mask=mask@entry=7)
    at /home/stem/projects/atsamd51/dropalt/riot/core/thread_flags.c:106
  3    Thread 3 "usbhub_thread" (Name: usbhub_thread, Running) panic_arch ()
    at /home/stem/projects/atsamd51/dropalt/riot/cpu/cortexm_common/panic.c:49
(gdb) bt
#0  panic_arch ()
    at /home/stem/projects/atsamd51/dropalt/riot/cpu/cortexm_common/panic.c:49
#1  0x00004d66 in core_panic (crash_code=crash_code@entry=PANIC_MEM_MANAGE, 
    message=message@entry=0xb1c5 "MEM MANAGE HANDLER")
    at /home/stem/projects/atsamd51/dropalt/riot/core/lib/panic.c:81
#2  0x0000542e in mem_manage_default ()
    at /home/stem/projects/atsamd51/dropalt/riot/cpu/cortexm_common/vectors_cortexm.c:478
#3  <signal handler called>
#4  0x20001008 in _thread_stack ()
Backtrace stopped: previous frame identical to this frame (corrupt stack?)
(gdb) quit
```

* `fw.stack_usage()` in Lua shows the current stack usage.
   - ISR stack: 576 / 1024 bytes
   - main stack: 1380 / 2048 bytes
   - usbhub_thread stack: 552 / 1024 bytes
   - usbus stack: 932 / 1024 bytes
   - matrix_thread stack: 608 / 1024 bytes

#### Hardfault when jumping to nullptr.
```
2023-06-03 20:50:54,761 # Context before hardfault:
2023-06-03 20:50:54,762 #    r0: 0x00000000
2023-06-03 20:50:54,763 #    r1: 0x20000e40
2023-06-03 20:50:54,764 #    r2: 0x00000001
2023-06-03 20:50:54,765 #    r3: 0x00004c9d
2023-06-03 20:50:54,766 #   r12: 0x00000022
2023-06-03 20:50:54,767 #    lr: 0x000071b1
2023-06-03 20:50:54,768 #    pc: 0x00000000
2023-06-03 20:50:54,769 #   psr: 0x40000000
2023-06-03 20:50:54,770 # 
2023-06-03 20:50:54,770 # FSR/FAR:
2023-06-03 20:50:54,771 #  CFSR: 0x00020000
2023-06-03 20:50:54,772 #  HFSR: 0x40000000
2023-06-03 20:50:54,772 #  DFSR: 0x00000000
2023-06-03 20:50:54,773 #  AFSR: 0x00000000
2023-06-03 20:50:54,773 # Misc
2023-06-03 20:50:54,774 # EXC_RET: 0xfffffffd
2023-06-03 20:50:54,775 # Active thread: 5 "keymap_thread"
2023-06-03 20:50:54,776 # Attempting to reconstruct state for debugging...
2023-06-03 20:50:54,776 # In GDB:
2023-06-03 20:50:54,776 #   set $pc=0x0
2023-06-03 20:50:54,777 #   frame 0
2023-06-03 20:50:54,777 #   bt
2023-06-03 20:50:54,778 # *** RIOT kernel panic (6): HARD FAULT HANDLER
2023-06-03 20:50:54,779 # *** halted.
2023-06-03 20:50:54,779 # Inside isr -13
```

#### Improve to trace Watchdog reset
  - See hard_fault_handler() in vectors_cortexm.c
  - Configure the Watchdog Timer (WDT) to generate an Early Warning interrupt before the reset occurs. This allows you to execute specific actions, such as logging the program counter (PC) or saving system state, before the reset.
    Ensure the Early Warning interrupt is configured with enough time before the watchdog timeout to allow for state-saving operations.
    ```
    WDT->EWCTRL.reg = WDT_EWCTRL_EWOFFSET(0x3); // Set Early Warning offset
    NVIC_EnableIRQ(WDT_IRQn);                   // Enable WDT interrupt in NVIC
    ```
  - In the ISR, capture the current program counter or other relevant state information. This can help you identify where the system was executing when the interrupt occurred.
    ```
    void WDT_Handler() {
        // Log the program counter or other state information
        uint32_t currentPC = __get_MSP(); // Example: Get Main Stack Pointer
        // pc = sp[6]
        save_state_to_memory(currentPC);  // Save state to non-volatile memory

        // Clear the Early Warning interrupt flag
        WDT->INTFLAG.reg = WDT_INTFLAG_EW;
    }
    ```
  - After the system resets, check the reset cause register to confirm that the reset was triggered by the watchdog. The SAMD51J has a Reset Cause (RCAUSE) register that indicates the source of the reset.
    ```
    if (RSTC->RCAUSE.bit.WDT) {
        // Watchdog reset occurred
        uint32_t savedPC = read_state_from_memory(); // Retrieve saved state
        debug_log("Watchdog reset at PC: 0x%08X", savedPC);
    }
    ```

# Debugging USB (Capture USB traffic directly using `usbmon`)

- Enable the usbmon module
```
$ sudo modprobe usbmon
```

- Find the Bus# and the Device ID for "Drop ALT" and "Drop Hub".
```
$ lsusb
  Bus 003 Device 018: ID 04d8:eed3 Microchip Technology, Inc. Drop ALT
  Bus 003 Device 017: ID 04d8:eec5 Microchip Technology, Inc. Drop Hub
```

- Start capturing raw USB traffic (on Bus 3). Stop with Ctrl+C.
```
$ sudo cat /sys/kernel/debug/usb/usbmon/3u >/tmp/3u.log
```

- Investigate /tmp/3u.log
```
ffff94cc297c5c80 3631660527 S Co:3:018:0 s 21 0a 0000 0002 0000 0
ffff94cc297c5c80 3631662500 C Co:3:018:0 0 0
--> USB_HID: request: 0xa type: 0x21 value: 0 length: 0 state: 0

ffff94cc297c5c80 3631662517 S Ci:3:018:0 s 81 06 2200 0002 0039 57 <
ffff94cc297c5c80 3631665501 C Ci:3:018:0 0 57 = 05010906 a1010507 19e029e7 15002501 95087501 81020507 190029f7 15002501
--> USB_HID: request: 0xa type: 0x21 value: 0 length: 0 state: 0

ffff94cc297c52c0 3631665587 S Co:3:018:0 s 21 09 0200 0002 0001 1 = 00
ffff94cc297c52c0 3631668501 C Co:3:018:0 0 1 >
--> USB_HID: request: 0x9 type: 0x21 value: 2 length: 1 state: 0
    USB_HID: request: 0x9 type: 0x21 value: 2 length: 1 state: 3
    USB_HID: set led_lamp_state=0x0 @1307

ffff94cc297c5c80 3631665654 S Ii:3:018:3 -115:8 32 <  (Host's interrupt IN request)
ffff94cc297c5c80 3631674523 C Ii:3:018:3 0:8 32 = 00100000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
--> USB_HID: register press (0x4 A)

ffff94cc297c5c80 3631674524 S Ii:3:018:3 -115:8 32 <
ffff94cc297c5c80 3631682522 C Ii:3:018:3 0:8 32 = 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
--> USB_HID: register release (0x4 A)
```
