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
