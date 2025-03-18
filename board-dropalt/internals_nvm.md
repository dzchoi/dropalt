## NVM (flash memory)

#### Reference
  - NVMCTRL in SAM D5x/E5x Family Data Sheet
  - FlashStorage: https://github.com/cmaglie/FlashStorage
  - FlashStorage_SAMD library: https://github.com/khoih-prog/FlashStorage_SAMD
  - SmartEEPROM example:
    https://microchip-mplab-harmony.github.io/csp_apps_sam_d5x_e5x/apps/nvmctrl/nvmctrl_smarteeprom/readme.html
  - SmartEEPROM library: https://github.com/deezums/SmartEEPROM

#### Memory space
* Memory space is divided in two:
  - NVM main address space (0x0, 256KB, Read-Write) where 2 physical NVM banks (BANKA and BANKB) are mapped.
    - Size: PARAM.NVMP pages (1 page is 512 bytes.)
    - Region: The main address space is divided into 32 equally sized regions.

      Each region can be protected against write or erase operation. The 32-bit RUNLOCK register reflects the protection of each region. This register is automatically updated during power-up with information from User Page (NVM LOCKS field).
    - Block: Each bank is organized into blocks, where each block contains 16 pages (=8KB). (So, A region corresponds to a block in 256KB flash memory.)

  - Auxiliary space which contains:
    - The Calibration Page (CB) (0x00800000, 1 page, Read-only)
    - Factory and Signature pages (FS) (0x00806000, 4 pages, Read-only)
    - The USER Page (0x00804000, 1 page = 512 bytes, Read-Write)  
      -> The first word is reserved, and used during the NVMCTRL start-up to automatically configure the device.
    - Writing to the auxiliary space will take effect after the next reset. Therefore, a boot of the device is needed for changes in the lock/unlock setting to take effect.

  Note: a page, block, or region begins at its aligned address.

* The lower blocks in the NVM main address space can be allocated as a boot loader section using the BOOTPROT[3:0] fuse. It is protected against write or erase operations, except if STATUS.BPDIS is set. The BOOTPROT section is not erased during a Chip-Erase operation even if STATUS.BPDIS is set. The size of the protected section is (15 - STATUS.BOOTPROT) * 8KB. Note that Writing to the auxiliary space will take effect after the next reset.

STATUS.BPDIS can be set by issuing the Set BOOTPROT Disable command (SBPDIS). It
is cleared by issuing the Clear BOOTPROT Disable command (CBPDIS).

* The upper blocks can be allocated to SmartEEPROM. SmartEEPROM raw data is mapped at the end of the main address space. SmartEEPROM allocated space in the main address space is not accessible from AHB0/1. Any AHB access throws a hardfault exception.

#### NVMCTRL->ADDR
NVMCTRL->ADDR is a 24-bit register that represents a byte address, allowing it to access up to 16MB of flash memory.

#### NVM Read
Reading from the NVM is carried out through the AHB bus by directly addressing the NVM main address space or auxiliary address space, e.g. using the dereference operator in C.

* AHB0 - BANKA, AHB1 - BANKB, AHB2 - SmartEEPROM

#### Erase
Before a page can be written, it must be erased. Erasing the block/page sets all bits to ‘1’.

The erase granularity depend on the address space (block or page). The Erase Block/Page command can be used to erase the desired block or page in the NVM main address space. The Erase Page command can be issued on the USER page in the auxiliary space.

|       | Erase Granularity | Write Granularity |
|-------|-------------------|-------------------|
| BANKA | Block             | Quad-Word or Page |
| BANKB | Block             | Quad-Word or Page |
| AUX   | Page              | Quad-Word         |

If the block/page resides in a region that is locked, the erase will not be performed and the Lock Error bit in the INTFLAG register (INTFLAG.LOCKE) will be set. INTFLAG.PROGE will also be set since the command didn’t complete.

1. Write the address of the block/page to erase to ADDR. Any address within the block/page can be used.
2. Issue an Erase Block/Page command.

#### Chip Erase
The entire NVM main address space except the BOOTPROT section (not Auxiliary space) can be erased by a debugger Chip Erase command.

#### Page buffer
When writing data to the NVM main address space or USER page, it passes through the AHB bus and is initially written to an internal buffer called the page buffer. Once the write is complete, you can commit the data to the NVM either automatically or manually. The page buffer is the same size as an NVM page. When the page buffer is written it will be reflected in the address space(?).

An AHB write (i.e. writing to the address space) automatically updates the ADDR register. ADDR is write-locked by NVMCTRL until the page buffer is committed. Writes (i.e. loads) to the page buffer must be 32 bits. 16-bit or 8-bit writes to the page buffer is not allowed, and will cause a PAC error.  
(So, ADDR gets the address of the initial AHB write and should be word-aligned. Subsequent writes will go into the page buffer until the page buffer is committed.)

The page buffer is automatically cleared to all-ones after any page write operation (WP or WQW command).

Internally, only 64 bits are written to the page buffer at a time through the page buffer load register (PBLDATA), which is 64 bits wide and aligned. Crossing the 64-bit boundary will reset the PBLDATA register to all ones instead of reading the existing data from the page buffer. Therefore, caution is needed when rewriting into the page buffer.

If NVMCTRL is busy processing a write command (STATUS.READY=0) then the AHB bus is stalled on an AHB write until the ongoing command is completed. Writing the page buffer is allowed during a block erase operation.

The status of the page buffer is given by STATUS.LOAD. This bit indicates that the NVM page buffer has been loaded with one or more words.

#### NVM Commit
Supported erase/write commands depend on the address space (block or page).
|                    | WP  | WQW | EP  | EB  |
|--------------------|:---:|:---:|:---:|:---:|
| Main Address Space |  X  |  X  |     |  X  |
| User Page          |     |  X  |  X  |     |
* QW = 16B, P = 512B, B = 8KB

CTRLA.WMODE configures the write mode:

* Manual (MAN)
  No command is triggered automatically. You should execute the write command (WQW/WP) manually.
  - WP: Write the entire contents of the page buffer into the NVM at the address specified by ADDR.
  - WQW: Write the first quad-word in the page buffer into the NVM at the address specified by ADDR.

  Example
    1. Configure manual write for the NVM using WMODE (NVMCTRL.CTRLA).
    2. Make sure the NVM is ready to accept a new command (NVMCTRL.STATUS).
    3. Clear page buffer (NVMCTRL.CTRLB).
    4. Make sure NVM is ready to accept a new command (NVMCTRL.STATUS).
    5. Clear the DONE Flag (NVMCTRL.INTFLAG).
    6. Write data to page buffer with 32-bit accesses at the needed address.
    7. Perform page write (NVMCTRL.CTRLB).
    8. Make sure NVM is ready to accept a new command (NVMCTRL.STATUS).
    9. Clear the DONE Flag (NVMCTRL.INTFLAG).

  Q. What happens if we have changed more than quad words and we execute WQW?

* Automatic Write With Double Word Granularity (ADW)
  The **WQW** command is triggered at the ADDR address (which should be quad-word aligned) when the second word is written in the page buffer. The adjacent double-word within the page buffer must be all ones.

* Automatic Write With Quad Word Granularity (AQW)
  The WQW command is triggered at the ADDR address (which should be quad-word aligned) when the fourth word is written in the page buffer.

* Automatic Write With Page Granularity (AP)
  The WP command is triggered at the ADDR address (which should be page aligned) when the last word in the page is written.

Note that in automatic write modes, partially written pages can either be written with a manual write or discarded using the PBC command.

STATUS.READY will be 0 until programming is complete, and access through the AHB in the same bank will be stalled. Therefore it is possible to chain write commands without polling STATUS.READY.

#### Write lock
After programming the NVM main array, the region that the page resides in can be locked to prevent spurious write or erase sequences. Locking is performed on a per-region basis, and so locking a region locks all pages inside the region.

#### Serial number
The factory serial number was originally encoded within the bootloader (mdloader) as e.g.
`uint16_t[10] = u"1551771897"` at the address `*(uint32_t*)0x3ffc`.

```
00002480: 0001 0000 3100 3500 3500 3100 3700 3700  ....1.5.5.1.7.7.
00002490: 3100 3800 3900 3700 2000 2000 2000 2000  1.8.9.7. . . . .

00003ff0: ffff ffff ffff ffff ffff ffff 8424 0000
```

However, the product serial number (`..HMM.*`) is now stored as an `uint8_t[16]` array in the USER page of the device at address 0x804020. If the string length is less than 16, it can be null-terminated. This serial number is displayed in the iSerial field of the USB device descriptor.

* Max size
Under the USB protocol, the maximum string size is 126 characters in UTF-16 format, and the string does not require null-termination. However, in `struct Usb2422`, the size is further restricted to 31 characters, defined as `uint16_t SERSTR[31];`.

* Updating the USER page (updated openocd for `atsame5 userpage` required)
  ```
  openocd -f `f openocd.cfg` -c "atsame5 userpage > user_row.bin; exit"
  echo -en "15HMMKAG010321\0" | dd of=user_row.bin bs=1 seek=$((0x20)) conv=notrunc
  openocd -f `f openocd.cfg` -c "atsame5 userpage < user_row.bin; reset; exit"
  ```

  You can additionally modify a binary file using `xxd` in vim:
  ```
  : set noeol binary
  : %!xxd
  <Edit it!>
  : %!xxd -r
  : w
  ```

#### SmartEEPROM
SmartEEPROM은 각 BANKA/B에 각각 존재하는데 각 BANK마다 두 개의 virtual sector (SEES)가 있다.

즉, BANK swapping을 위한 추가 영역 외에도 또 추가 영역이 별도로 있는 셈인데, 이는 bit가 0->1로 바뀔 때 full block erase가 필요한데 반대 편에 있는 (이미 erase된) SEES를 할당하여 사용하게 된다. 이 automatic reallocation를 통해 SmartEEPROM은 erase opration을 신경쓰지 않아도 된다.

#### Backup RAM
From riot/cpu/samd5x/Makefile.include,  
BACKUP_RAM_ADDR = 0x47000000  
BACKUP_RAM_LEN  = 0x2000

From vectors_cortexm.c,  
```
#ifndef CPU_BACKUP_RAM_NOT_RETAINED
#define CPU_BACKUP_RAM_NOT_RETAINED 0
#endif
```
