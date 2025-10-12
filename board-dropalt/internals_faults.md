#### [RIOT handlers]
* Handling Hardfault (vector_cortexm.c)
```
  cortex_vector_base[2]
  -> hard_fault_default()
       # ifdef DEVELHELP:
       -> hard_fault_handler()
            -> core_panic()

       # ifndef DEVELHELP:
       -> core_panic()
```

* core_panic(core_panic_t crash_code, const char *message)
```
  -> panic_app(crash_code, message)
  -> panic_arch() in riot/cpu/cortexm_common/panic.c
      pauses at __asm__("bkpt #0") if a debugger is connected
  -> pm_reboot() if CONFIG_CORE_REBOOT_ON_PANIC && defined(MODULE_PERIPH_PM)
  -> OR usb_board_reset_in_bootloader() if defined(MODULE_USB_BOARD_RESET)
  -> OR pm_off() if defined(MODULE_PERIPH_PM)
      does while(1) {} for SAMD51
  -> OR while(1) {}
```

* assert()
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
The assert_panic addresses can be resolved with tools like `arm-none-eabi-addr2line`, `objdump`, or `gdb` (using the command e.g. `info line *(0x89abcdef)`) to identify the line the assertion failed in.

#### [Common fault handler on RIOT]
* ARMv6-M: Cortex-M0/M0+/M1
  ARMv7-M/ARMv7E-M: Cortex-M3/M4/M7
  ARMv8-M: Cortex-M23/M33

* UsageFault:
  Instruction Execution Errors, Divide By Zero, Unaligned Access
  ```
  void usagefault_example() {
    // udf 0xff
    __builtin_trap();
  }
  ```

  BusFault:
  Memory Access Error, Can be Imprecise (fault may be triggered after executing the fault instruction (due to caching)).
  ```
  void busfault_example() {
    *(uint32_t*)0xdead0000 = 0x20;
  }
  ```

  MemManage:
  Memory Protection Unit, Execute Never (XN) violation (0xe0000000 - 0xffffffff)
  ```
  void memmanage_example() {
    void (*f)() = (void*)0xe0000000;
    f();
  }
  ```

* Dual stack
  - PSP (Processor Stack Pointer) used by:
    Interrupt handlers, Fault handlers, System-level code
  - MSP (Main Stack Pointer or ISR Stack Pointer) used by:
    User threads
  Note: Cortex-M0/M0+ does not support dual stack pointers, only uses MSP.

* `SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk` is safe across all Cortex-M cores, including Cortex-M0/M0+.
  - Cortex-M0/M0+ (ARMv6-M)
  - Cortex-M3/M4/M7/M33/M55/M85 (ARMv7-M and ARMv8-M)
  - It holds the number of the currently active exception (ISR number).
    3   Hard fault
    4   Mem manage fault (only for Cortex-M3+)
    5   Bus fault
    6   Usage fault
    7   Secure fault
    12  Debug monitor

* When an exception (like an interrupt or fault) occurs on a Cortex-M processor, the processor automatically pushes a set of registers onto the active stack (PSP or MSP). This collection of saved registers is called the Exception Stack Frame (ESF).
  - R0-R3: General-purpose registers
  - R12: Scratch register
  - LR (Link Register, stack[5])
  - PC (Program Counter, stack[6]) <- the address of the instruction that caused the fault
  - xPSR (Program Status Register)
  - FPU registers (if EXC_RETURN & ?? != 0)
    18 registers, 72 bytes
  - 4 byte padding (if EXC_RETURN & ?? != 0)

  Then, the processor sets
  - SP to be MSP
  - xPSR 
    = Bits 0..8: exception number or 0
      (1 = Reset, 2 = NMI, 3 = HardFault, 4 = MemManage, 5 = BusFault, 6 = UsageFault)
    = Bit 9: whether or not padding was used (in xPSR saved on ESF)
  - LR to a special value called EXC_RETURN
    = Bit 2: 0 = MSP saves ESF, 1 = PSP saves ESF
    = Bit 3: 0 = exception occurred in handler mode, 1 = in thread mode
    = Bit 4: 0 = FPU registers saved in ESF, 1 = not saved
  - PC to the fault handler to trigger

* Exit the fault handler
  - `bx lr` will exit the fault handler and restore `{r0-r3, r12, lr, pc, xpsr}` from the stack.
  - So, if the fault has occurred from an ISR or a fault handler, MSP should point to the ESF correctly.

* Recovering the call stack
  - set $exc_frame = ($lr & 0x4) ? $psp : $msp
  - set $stacked_xpsr = ((uint32_t*)$exc_frame)[7]
    set $exc_frame_len = 4 * (
        8 +
        (($stacked_xpsr & (1 << 9)) ? 1 : 0) +
        (($lr & 0x10) ? 0 : 18) )
  - set $sp = $exc_frame + $exc_frame_len
    set $lr = ((uint32_t*)$exc_frame)[5]
    set $pc = ((uint32_t*)$exc_frame)[6]

* [MemManage fault] SCB->CFSR contains MMFSR that indicates the type of fault:
    0   Instruction access violation
    1   Data access violation
    3   Unstacking error
    4   Stacking error
    7   MMFAR is valid

  SCB->MMFAR contains the address that causesd the memory fault.

* Detecting ISR stack overflow.
  - `reset_handler_default()` in vectors_cortexm.c fills the ISR stack with Canary words.
    ```
    #ifdef DEVELHELP
    uint32_t *top;
    /* Fill stack space with canary values up until the current stack pointer */
    /* Read current stack pointer from CPU register */
    __asm__ volatile ("mov %[top], sp" : [top] "=r" (top) : : );
    dst = &_sstack;
    while (dst < top) {
        *(dst++) = STACK_CANARY_WORD;
    }
    #endif
    ```
  - `common_fault_handler()` (or `hard_fault_handler()`) in vectors_cortexm.c checks if the stack was ever overflowed before.
    ```
    if (*(&_sstack) != STACK_CANARY_WORD) {
        LOG_ERROR("ISR stack overflowed");
    }
    ```
  - `thread_isr_stack_usage()` in cpu/cortexm_common/thread_arch.c determines stack usage by checking the remaining Canary words and computing the corresponding number of bytes consumed.
    ```
    uint32_t *ptr = &_sstack;

    while (((*ptr) == STACK_CANARY_WORD) && (ptr < &_estack)) {
        ++ptr;
    }
    ```
  - `reset_handler_default()` in vectors_cortexm.c configures the top 32 bytes of the ISR stack (`_sstack`) as read-only, causing any write attempts to trigger a MemManage fault. However, this appears to be redundant since `_sstack` is already placed at the start of RAM (0x20000000) as defined in the linker script, and any attempt to access memory beyond this address is inherently invalid and would trigger a MemManage fault regardless.
    ```
    #ifdef MODULE_MPU_STACK_GUARD
    if (((uintptr_t)&_sstack) != SRAM_BASE) {
        mpu_configure(
            1,                                              /* MPU region 1 */
            (uintptr_t)&_sstack + 31,                       /* Base Address (rounded up) */
            MPU_ATTR(1, AP_RO_RO, 0, 1, 0, 1, MPU_SIZE_32B) /* Attributes and Size */
        );

    }
    #endif
    ```
    ```
    # makefiles/features_modules.inc.mk
    # use mpu_stack_guard if the feature is used
    ifneq (,$(filter cortexm_mpu,$(FEATURES_USED)))
        USEMODULE += mpu_stack_guard
    endif
    ```

* Detecting thread stack overflow.
  - `thread_create()` initializes the stack by populating each entry with its own address, resembling the behavior of Canary words used for ISR stack.
    ```
    #if defined(DEVELHELP) || defined(SCHED_TEST_STACK) \
    || defined(MODULE_TEST_UTILS_PRINT_STACK_USAGE)
    if (flags & THREAD_CREATE_NO_STACKTEST) {
        /* create stack guard. Alignment has been handled above, so silence
         * -Wcast-align */
        *(uintptr_t *)(uintptr_t)stack = (uintptr_t)stack;
    }
    else {
        /* assign each int of the stack the value of it's address. Alignment
         * has been handled above, so silence -Wcast-align */
        uintptr_t *stackmax = (uintptr_t *)(uintptr_t)(stack + stacksize);
        uintptr_t *stackp = (uintptr_t *)(uintptr_t)stack;

        while (stackp < stackmax) {
            *stackp = (uintptr_t)stackp;
            stackp++;
        }
    }
    #endif
    ```
  - `thread_measure_stack_free()` leverages the stack's initialization pattern to determine how much of the stack has been used by scanning for overwritten entries.
    ```
    uintptr_t *stackp = (uintptr_t *)(uintptr_t)stack;
    uintptr_t end = (uintptr_t)stack + size;

    while (((uintptr_t)stackp < end) && (*stackp == (uintptr_t)stackp)) {
        stackp++;
    }
    ```
  - `sched_run()` in core/sched.c configures an MPU guard to detect stack overflows each time it switches to a new thread.
    ```
    #ifdef MODULE_MPU_STACK_GUARD
        mpu_configure(
            2,                                              /* MPU region 2 */
            (uintptr_t)next_thread->stack_start + 31,       /* Base Address (rounded up) */
            MPU_ATTR(1, AP_RO_RO, 0, 1, 0, 1, MPU_SIZE_32B) /* Attributes and Size */
            );
    #endif
    ```

* [Deprecated] HARDFAULT_HANDLER_REQUIRED_STACK_SPACE
  - `hard_fault_handle()` utilizes `LOG_*()` macros to report fault details, which requires a minimum amount of free stack space on the ISR stack to operate correctly. If this required space is unavailable, the function clears the entire ISR stack, potentially discarding previous context from the faulting ISR or any nested fault handlers.
  - This required ISR stack space (HARDFAULT_HANDLER_REQUIRED_STACK_SPACE) can be empirically determined by triggering a deliberate fault at the beginning of the main thread.
    ```
    __asm__ volatile (
        "cpsid i\n"
        "msr msp, %[estack]\n"
        "cpsie i\n"
        :
        : [estack] "r" (&_estack)
    );

    __attribute__((unused)) volatile int p = *(int*)(0);
    ```
  - By examining the resulting fault information, it's possible to measure how much stack was consumed during fault handling - 552 bytes in this case.
    ```
    CPU register context prior to fault:
      r0  = 0x00000001
      r1  = 0x0001bf16
      r2  = 0x00000000
      r3  = 0x00000000
      r12 = 0x00000001
      lr  = 0x0000964d
      pc  = 0x00004952
      psr = 0x61030000
    Fault status registers:
      CFSR (UFSR:BFSR:MMFSR) = 0x0000:0x00:0x82
      HFSR = 0x00000000
      DFSR = 0x00000000
      AFSR = 0x00000000
      MMFAR = 0x00000000
    EXC_RETURN: 0xfffffffd
    Active thread: 1 "main" (SP: 124 / 2048 bytes)
    Attempting to reconstruct state for debugging...
    In GDB:
      set $pc=0x4952
      frame 0
      bt
    *** RIOT kernel panic: [5] MEM MANAGE HANDLER
    *** halted.
    Inside isr -12
    ISR stack usage: 552 / 1024 bytes
    ```
