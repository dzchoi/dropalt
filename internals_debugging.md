#### System reset
$ dalua -e 'require("fw").system_reset()'

$ openocd -f `f openocd.cfg` -c "reset; exit"

#### Boot into bootloader
$ dalua -e 'require("fw").enter_bootloader()'

$ `f edbg` -bt samd51 -x 10

#### Dump symbols
Useful to check for generating duplicate memories:  
$ arm-none-eabi-objdump -t *.elf | sort

Sort the data size:  
$ arm-none-eabi-nm --print-size --size-sort --radix=d *.elf

Size of memory that a class instance occupies:  
Search for the variable name in .elf

#### Disassemble
$ arm-none-eabi-objdump -S slot0.elf |less

$ arm-none-eabi-objdump -S cpu.o >cpu.s

#### Get compiler options
Should be compiled with '-g'.
See [Get the compiler options from a compiled executable?](https://stackoverflow.com/a/65507874/10451825)  
$ arm-none-eabi-readelf --debug-dump=info ./*.elf |less

#### View logs in real-time
$ dalua -d

$ tio -mINLCRNL `ls -t /dev/ttyACM* |head -n1`

$ while true; do cat /dev/ttyACMx 2>/dev/null; done

#### Retrieve logs in DFU mode
$ dfu-util -a0 -U filename

Note:
  - `dfu-util -a0 -U - | less` is supported starting from v0.10.
  - `dfu-util -a0 -U >(less)` is not (or will not be) supported.

#### Autonomous conditional break point
```
static int count = 0;
if ( ++count == 10 )
    __asm__("bkpt #0");
```

#### DEVELHELP
set `DEVELHELP = 1` in Makefile along with `CFLAGS += -DDEBUG_ASSERT_VERBOSE`.

#### Debugging switchover using CDC-ACM
* Connect to the same host with both ports.
* The same serial (e.g. /dev/ttyACM0) in the host can be used for CDC-ACM.

#### Hard faults and failed ASSERTs
* When a debugger is attached, any hard fault (Hardfault, BusFault, UsageFault and MemManage fault) will trigger a breakpoint at the entry of the corresponding fault handler. VSCode (and gdb) will then show the point of the fault and the call stack.
* If no debugger is present, the fault logs are captured in backup RAM and the system proceeds to reboot into the bootloader for post-mortem analysis.

#### Post-mortem analysis example
```
// adc.c
void adc_v_con::isr_signal_report()
{
    __builtin_trap();
    usbhub_thread::isr_process_v_con_report();
}
```

```
Fault [3] occurred in ISR/IRQ 119
R0    200030B4  R7    2000E004
R1        0432  R8           0
R2    200007B0  R9    2000E004
R3    00012AB1  R10          0
R4    2000158C  R11          3
R5    2000DF90  R12   20009CE8
R6           4  SP    200003F8
LR        4DF9  PC    00012AB0
xPSR  610F0087
CFSR  00010000
HFSR  40000000
DFSR         0
AFSR         0
EXC_RETURN FFFFFFF1
*** RIOT kernel panic: HARD FAULT HANDLER
pid   name              state     pri  lr        pc        stack usage
  -   isr_stack         -           -  -         -         624/1024
  1  >main              running     7  00006C1B  0000C698  920/2048
  2   usbhub_thread     bl anyfl    3  00004E6D  00004E7C  568/1024
  3   usbus             pending     1  00004E6D  00004E7C  788/1024
  4   matrix_thread     bl mutex    2  00009A51  00009A70  208/1024
*** halted.
Inside ISR/IRQ -13
```

Note:
- `Fault [3]`: HardFault = 3, MemManage = 4, BusFault = 5, UsageFault = 6.
- `ISR/IRQ 119`: The fault occurred while running in the ISR for IRQ 119. If this number is negative (e.g. -13 = 3 - 16 for HardFault), it indicates a nested fault - the fault occurred while already handling another fault.
- `LR        4DF9  PC    00012AB0`: values captured just before the fault, representing the link register and program counter at the faulting instruction.
  * The addresses can be resolved using e.g. `arm-none-eabi-addr2line -e slot0.elf 0x12ab0` or `arm-none-eabi-objdump -S slot0.elf`.
- `>main`: The "main" thread was active when the interrupt occurred, leading to the subsequent HardFault.
- `0000C698  00006C1B`: PC and LR values in the main thread when it was interrupted. In this case, the interrupt is not related to the main thread.
  * If PC and LR show "running" (e.g. on assert failure), it means the fault occurred directly in the thread, not while it was interrupted.
- `bl anyfl`: Indicates the thread is Blocked for Anyflags. See `state_names[]` in core/thread.c for the full list of thread states.
- `Inside ISR/IRQ -13`: Also indicates the fault type: -13 + 16 = 3 (HardFault).

#### Watchdog fault
- Watchdog faults aren't faults in the traditional sense - they function more like a timer. Instead of signaling a specific error, the watchdog simply forces a system reset when the "main" thread fails to kick it within a defined period.
- A typical cause is a high-priority thread monopolizing CPU time without yielding. However, if the offending thread is blocked - such as waiting on a mutex or signal - it won’t trigger a watchdog fault, since it’s not actively consuming CPU.
- The Early Warning Interrupt (EWI) mechanism used by `wdt_setup_reboot_with_callback()` is limited for typical use cases where the EWI is expected to trigger shortly before the watchdog expires.
  This is because EWCTRL.EWOFFSET uses the same exponential encoding scheme as CONFIG.PER (e.g. 0 = ~8 ms, 1 = ~16 ms, 2 = ~32 ms, ..., 11 = ~16 seconds), but instead of representing time before expiration, it defines the absolute time interval from the start of the watchdog timeout period to the EWI trigger.
  As a result, the EWI can only be scheduled at coarse, front-loaded intervals (½, ¼, ⅛, etc. of the timeout), leaving a significant gap between the EWI and the actual expiration in most configurations. In fact, the closest possible EWI timing to the expiration is half the watchdog period.
- Although not supported due to the noted limitation, invoking `ps()` at the "close" moment of a watchdog fault would aid debugging. It would provide a snapshot of each thread’s state - such as Running, Suspended, Pending on a signal, or Waiting on a mutex - along with the current values of the PC and LR.

#### Stack overflow
* When MODULE_MPU_STACK_GUARD is enabled, the MPU (Memory Protection Unit) protects both the initial stack — used before any threads are created and also serving as the ISR stack — and each thread's individual stack.
* A stack overflow in a thread will trigger a MemManage fault. You can inspect the stack usage in the fault dump:
  ```
  *** RIOT kernel panic: MEM MANAGE HANDLER
  pid   name              state     pri  lr        pc        stack usage
    -   isr_stack         -           -  -         -         584/1024
    1  >main              running     7  00004671  0000466A  2016/2048  <---
    2   usbhub_thread     bl anyfl    3  00004E79  00004E88  568/1024
    3   usbus             pending     1  00004E79  00004E88  944/1024
    4   matrix_thread     bl mutex    2  00009A4D  00009A6C  204/1024
  Inside ISR/IRQ -12
  *** rebooting...
  ```
* Use thread_measure_stack_free() for threads created with THREAD_CREATE_STACKTEST.
* Use thread_stack_print() and thread_print_stack().
* See test_utils_print_stack_usage() in the module with the same name.
* SCHED_TEST_STACK, THREAD_CREATE_STACKTEST, DEBUG_EXTRA_STACKSIZE, DEBUG_BREAKPOINT(), PANIC_STACK_OVERFLOW
* `fw.ps()` in Lua shows the current stack usage.
  ```
  pid   name              state     pri  lr        pc        stack usage
    -   isr_stack         -           -  -         -         576/1024
    1  >main              running     7  running   running   1144/2048
    2   usbhub_thread     bl anyfl    3  00004E49  00004E58  568/1024
    3   usbus             pending     1  00004E49  00004E58  948/1024
    4   matrix_thread     bl mutex    2  00009A59  00009A78  572/1024
  ```

##### Debugging USB (Capture USB traffic directly using `usbmon`)

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

#### Debugging CDC ACM (using `wireshark` and `usbmon`)

- Enable the usbmon module
```
$ sudo modprobe usbmon
```

- Find the Bus# and the Device ID for "Drop ALT" and "Drop Hub".
```
$ lsusb
    Bus 007 Device 075: ID 04d8:eed3 Microchip Technology, Inc. Drop ALT
    Bus 007 Device 074: ID 04d8:eec5 Microchip Technology, Inc. Drop Hub
```
  So, in this case, `7.75` is the Bus# and the Device ID.

- Run wireshark and select `usbmon7` for the interface.
```
$ sudo wireshark
```
- Example of exchanging pings:  
  7.75.0: Control endpoint  
  7.75.1: Bulk OUT (host -> device) endpoint  
  7.75.2: Bulk IN (device -> host) endpoint

```
host -> 7.75.1 Bulk OUT

Frame 1193924: 68 bytes on wire (544 bits), 68 bytes captured (544 bits) on interface usbmon7, id 0
USB URB
    [Source: host]
    [Destination: 7.75.1]
    URB id: 0xffff8cd084605440
    URB type: URB_SUBMIT ('S')
    URB transfer type: URB_BULK (0x03)
    Endpoint: 0x01, Direction: OUT
    Device: 75
    URB bus id: 7
    Device setup request: not relevant ('-')
    Data: present (0)
    URB sec: 1757712584
    URB usec: 368556
    URB status: Operation now in progress (-EINPROGRESS) (-115)
    URB length [bytes]: 4
    Data length [bytes]: 4
    [Response in: 1193925]
    [bInterfaceClass: CDC-Data (0x0a)]
    Unused Setup Header
    Interval: 0
    Start frame: 0
    Copy of Transfer Flags: 0x00000004, No transfer DMA map
    Number of ISO descriptors: 0
Leftover Capture Data: 5b7d300a  <-- "[}0\n"
```

```
7.75.1 -> host Bulk OUT

Frame 1193925: 64 bytes on wire (512 bits), 64 bytes captured (512 bits) on interface usbmon7, id 0
USB URB
    [Source: 7.75.1]
    [Destination: host]
    URB id: 0xffff8cd084605440
    URB type: URB_COMPLETE ('C')
    URB transfer type: URB_BULK (0x03)
    Endpoint: 0x01, Direction: OUT
    Device: 75
    URB bus id: 7
    Device setup request: not relevant ('-')
    Data: not present ('>')
    URB sec: 1757712584
    URB usec: 368588
    URB status: Success (0)
    URB length [bytes]: 4
    Data length [bytes]: 0
    [Request in: 1193924]
    [Time from request: 0.000032000 seconds]
    [bInterfaceClass: CDC-Data (0x0a)]
    Unused Setup Header
    Interval: 0
    Start frame: 0
    Copy of Transfer Flags: 0x00000004, No transfer DMA map
    Number of ISO descriptors: 0
```

```
7.75.2 -> host Bulk IN

Frame 1193926: 68 bytes on wire (544 bits), 68 bytes captured (544 bits) on interface usbmon7, id 0
USB URB
    [Source: 7.75.2]
    [Destination: host]
    URB id: 0xffff8cd084604300
    URB type: URB_COMPLETE ('C')
    URB transfer type: URB_BULK (0x03)
    Endpoint: 0x82, Direction: IN
    Device: 75
    URB bus id: 7
    Device setup request: not relevant ('-')
    Data: present (0)
    URB sec: 1757712584
    URB usec: 368630
    URB status: Success (0)
    URB length [bytes]: 4
    Data length [bytes]: 4
    [Request in: 1193904]
    [Time from request: 0.000601000 seconds]
    [bInterfaceClass: CDC-Data (0x0a)]
    Unused Setup Header
    Interval: 0
    Start frame: 0
    Copy of Transfer Flags: 0x00000204, No transfer DMA map, Dir IN
    Number of ISO descriptors: 0
Leftover Capture Data: 5b7d300a  <-- "[}0\n"
```

```
host -> 7.75.2 Bulk IN

Frame 1193927: 64 bytes on wire (512 bits), 64 bytes captured (512 bits) on interface usbmon7, id 0
USB URB
    [Source: host]
    [Destination: 7.75.2]
    URB id: 0xffff8cd084604300
    URB type: URB_SUBMIT ('S')
    URB transfer type: URB_BULK (0x03)
    Endpoint: 0x82, Direction: IN
    Device: 75
    URB bus id: 7
    Device setup request: not relevant ('-')
    Data: not present ('<')
    URB sec: 1757712584
    URB usec: 368631
    URB status: Operation now in progress (-EINPROGRESS) (-115)
    URB length [bytes]: 128
    Data length [bytes]: 0
    [Response in: 1194007]
    [bInterfaceClass: CDC-Data (0x0a)]
    Unused Setup Header
    Interval: 0
    Start frame: 0
    Copy of Transfer Flags: 0x00000204, No transfer DMA map, Dir IN
    Number of ISO descriptors: 0
```

- Use filter `usb.capdata` to capture only those packets with 
```
1193924	121.171920	host	7.75.1	USB	68	URB_BULK out  "[}0\n"
1193926	121.171994	7.75.2	host	USB	68	URB_BULK in   "[}0\n"
1193928	121.172036	host	7.75.1	USB	68	URB_BULK out  "\eLua"
1193929	121.172040	host	7.75.1	USB	65	URB_BULK out
...
1193973	121.172555	host	7.75.1	USB	65	URB_BULK out
1193976	121.172588	host	7.75.1	USB	68	URB_BULK out  "_ENV"
1193992	121.173153	7.75.2	host	USB	65	URB_BULK in   "n"
1193994	121.173195	7.75.2	host	USB	65	URB_BULK in   "i"
1193996	121.173197	7.75.2	host	USB	65	URB_BULK in   "l"
1193998	121.173220	7.75.2	host	USB	65	URB_BULK in   "\n"
1194000	121.173222	7.75.2	host	USB	68	URB_BULK in   "[}0\n"
```

#### Debugging `dalua -e`
```
for i in $(seq 1 50000); do echo -n "$i "; if ! dalua -e nil >/dev/null; then break; fi; done
```
