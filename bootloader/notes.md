## Riot bootloader
We use `riotboot_dfu`, which supports USB Device Firmware Update (DFU) in addition to the slot selection booting. See https://api.riot-os.org/group__bootloader__riotboot.html

It offers two slots (using `bAltenateSetting` in the USB Interface Descriptor) and implements the DFU protocol to allow flashing binaries into either slot with `dfu-util`.

#### Booting sequence
A power reset or system reset will immediately boot from slot 0. Any other reset, such as an external reset or watchdog reset, will enter the bootloader (DFU mode). However, a reset with the RIOTBOOT_MAGIC_NUMBER set at the end of RAM will enter the bootloader.

Executing reboot_to_bootloader() from firmware will jump to the bootloader.

#### Reset button
The Drop Alt device appears to connect its Reset button to the ATSAMD51's RESETN pin instead of a dedicated GPIO pin (BTN0_PIN). See
qmk_firmware/keyboards/massdrop/alt/config.h. An external reset can be triggered either by pressing this button or by running an application (`edbg -bt samd51 -x 10`).

This also aligns with the Itsybitsy-m4 configuration. See
https://learn.adafruit.com/introducing-adafruit-itsybitsy-m4/pinouts#other-pins-2991183.

#### Memory layout
```
|------------------------------- FLASH -------------------------------------|
|----- RIOTBOOT_LEN ----|----------- RIOTBOOT_SLOT_SIZE (slot 0) -----------|
                        |----- RIOTBOOT_HDR_LEN ------|
 ---------------------------------------------------------------------------
|        riotboot       | riotboot_hdr_t + filler (0) |   RIOT firmware     |
 ---------------------------------------------------------------------------
```

* From riot/cpu/cortexm_common/Makefile.include (for Cortex-m4f):  
  RIOTBOOT_LEN = 16K (0x4000)  
  RIOTBOOT_HDR_LEN = 1024 (0x400)

* From riot/sys/riotboot/Makefile.include:  
  SLOT0_LEN = 120K = (ROM_LEN - RIOTBOOT_LEN) / 2  
  SLOT1_LEN = SLOT0_LEN  
  SLOT0_OFFSET = RIOTBOOT_LEN  
  SLOT1_OFFSET = SLOT0_LEN + SLOT0_OFFSET

* From riot/cpu/sam0_common/Makefile.include:  
  ROM_LEN = 0x00040000  
  RAM_LEN = 0x00020000  
  ROM_START_ADDR = 0x00000000  
  RAM_START_ADDR = 0x20000000

* USB_VID/PID is set from CONFIG_USB_VID/PID in board.h.

#### riotboot_hdr_t
```
typedef struct {
  uint32_t magic_number;      /**< Header magic number (always "RIOT")              */
  uint32_t version;           /**< Integer representing the partition version       */
  uint32_t start_addr;        /**< Address after the allocated space for the header */
  uint32_t chksum;            /**< Checksum of riotboot_hdr                         */
} riotboot_hdr_t;
```

* magic_number = 0x544f4952  /* "RIOT" */
* version = APP_VER from riot/makefiles/boot/riotboot.mk  
  APP_VER should be an uint32_t number, which is `date +%s` by default (the current time expressed as the number of seconds since 1/1/1970 UTC.)
* The chksum is computed over magic_number, version and start_addr in riotboot_hdr_t, using fletcher32().

#### Serial number
The factory serial number was originally encoded within the bootloader (mdloader) as e.g.
`uint16_t[10] = u"1551771897"` at the address `*(uint32_t*)0x3ffc`.

```
00002480: 0001 0000 3100 3500 3500 3100 3700 3700  ....1.5.5.1.7.7.
00002490: 3100 3800 3900 3700 2000 2000 2000 2000  1.8.9.7. . . . .

00003ff0: ffff ffff ffff ffff ffff ffff 8424 0000
```

However, the product serial number is now encoded as `uint16_t[15] = u"..HMM.*"` in the USER page of the device at address `0x804020`. This serial number is visible in the iSerial field of the USB device descriptor.
