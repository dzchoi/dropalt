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

* .build/board-dropalt/riotboot_files/slot0.bin has file size 31800 (= .text + .data).
* .build/board-dropalt/riotboot_files/slot0.96.bin has file size 32824 (= .text + .data + RIOTBOOT_HDR_LEN)

#### Flash images
``dfu-util --alt 0 --download `f slot0.96.bin` ``
