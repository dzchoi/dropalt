#### Bootloader
`make flash`

#### Firmware
`make riotboot/slot0`
`make riotboot/flash-slot0`
`make PROGRAMMER=edgb riotboot/flash-slot0`

#### Binary size
```
$ size `f slot0.elf`
   text    data     bss     dec     hex filename
  31348     452   22816   54616    d558 /home/stem/projects/atsamd51/dropalt/.build/board-dropalt/riotboot_files/slot0.elf
```

* Tow firmware binaries are created (using FEATURES_REQUIRED += riotboot). `slot0.XXXX.bin` adds the slot header to `slot0.bin` at the beginning.
* .build/board-dropalt/riotboot_files/slot0.bin has file size 31800 (= .text + .data).
* .build/board-dropalt/riotboot_files/slot0.96.bin has file size 32824 (= .text + .data + RIOTBOOT_HDR_LEN)

#### Flash images
``dfu-util --alt 0 --download `f slot0.96.bin` ``

#### dfu-util
See [DFU 1.1](https://www.usb.org/sites/default/files/DFU_1.1.pdf).

`make riotboot/falsh-slot0` internally uses dfu-util via riot/makefiles/tools/dfu-util.inc.mk, and flashes slot0.XXXX.bin into the slot 0.

`dfu-util` allows us to select the target slot number using the `-a` or `--alt` option. However, it only recognizes the slot number itself, without any knowledge of the slot's start address or size. These details are managed solely by the target device.

`dfu-util` recognizes the optional DFU suffix, which is added at the end of the firmware image to verify that the firmware matches the device. This suffix includes information such as the Vendor ID, Product ID, Device ID, and more. It can be appended to the image file using the `dfu-suffix` tool. However, Riot does not incorporate the DFU suffix when generating firmware images.

> dfu-util: Invalid DFU suffix signature  
> dfu-util: A valid DFU suffix will be required in a future dfu-util release!!!
