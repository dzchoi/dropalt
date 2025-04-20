#### Bootloader
```
make
make flash
```

#### Firmware
```
make riotboot/slot0
make riotboot/flash-slot0
dfu-util --alt 0 --download `f slot0.96.bin`

make PROGRAMMER=edbg riotboot/flash-slot0
```
Use `make riotboot/slot0` rather than `make`, as using `make` can result in a build error.

#### Firmware size
```
$ size `f slot0.elf`
   text    data     bss     dec     hex filename
  31348     452   22816   54616    d558 /home/stem/projects/atsamd51/dropalt/.build/board-dropalt/riotboot_files/slot0.elf
```

* Two firmware binaries are created (using FEATURES_REQUIRED += riotboot). `slot0.XXXX.bin` adds the slot header to `slot0.bin` at the beginning.
* .build/board-dropalt/riotboot_files/slot0.bin has file size 31800 (= .text + .data).
* .build/board-dropalt/riotboot_files/slot0.96.bin has file size 32824 (= .text + .data + RIOTBOOT_HDR_LEN)

#### Keymap module
```
daluac keymap.lua >keymap.bin
dfu-util -a1 -D keymap.bin
```
