## Monolithic Firmware

The monolithic firmware includes both the bootloader and application logic in a single image. Below are the steps to build and flash the firmware.

### Build and Flash with EDBG

Requires a CMSIS-DAP compatible debugger connected to the target device.

```bash
# Navigate to the `dropalt` directory
$ cd dropalt

# Build the firmware
$ make

# Flash the firmware using EDBG
$ riot/dist/tools/edbg/edbg -b -t samd51 -f .build/board-dropalt/dropalt-fw.bin -p -v

# Alternatively, build and flash using Makefile integration
$ make PROGRAMMER=edbg flash
```

### Update via DFU (after initial flash)

Once the firmware has been flashed via EDBG, subsequent updates can be performed using dfu-util.

```bash
# Update the firmware using dfu-util
$ dfu-util -a0 -D .build/board-dropalt/dropalt-fw.bin -R

# Build and update both firmware and keymap module using a helper script
$ ./daflash
```

## Keymap module

The Lua-based keymap module can be downloaded independently without reflashing the firmware.

```
# Navigate to the `dropalt` directory
$ cd dropalt

# Build and download the Lua bytecode
$ ./dadownload
```
