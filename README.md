## Drop ALT keyboard firmware built on Riot

Development is not yet completed though it is almost in final phase. This document is also being worked on.

### Features
* Multi-threading backed by [Riot](https://doc.riot-os.org/index.html)
* Keymaps implemented using C++ class inheritance
* More use of the built-in USBhub (two hosts can be plugged in, switching them using a hot key or by unplugging)
* ... (To be updated)

### Building and flashing
* Prepare the source code.
  ```console
  git clone --recurse-submodules https://github.com/dzchoi/dropalt.git
  ```

* As being developed as an [application](https://doc.riot-os.org/creating-an-application.html) in Riot source code architecture,
  it can be built using the [Riot building process](https://doc.riot-os.org/getting-started.html#compiling-riot). Note that the
  keyboard's MCU architecture is ARM Cortex M (Cortex-M4f to be exact).

  You can simply execute to build:
  ```console
  make
  ```

* The generated binary is compatible with Microchip's SAM-BA bootloader and can be flashed using the existing
  [mdload](https://github.com/Massdrop/mdloader).
  ```console
  mdloadeer -f -D dropalt/.build/board-dropalt/dropalt-*.bin --restart
  ```
  Alternatively, it can be also flashed using OpenOCD. (To be updated)

* To enter flash mode with this firmware, keep pressing `CapsLock + Left Alt + FN` for one second or press the reset PIN in the back.
  Currently many custom keymaps are applied for my personal and experimental use, which will be back to the official layout on
  finalized version.
  (See `keymaps.cpp` for current keymaps.)

### Defining custom keymaps
  (To be updated)

### Defining custom LED effects
  (To be updated)

### Monitoring log messages
* Log messages can be read through USB CDC-ACM (a.k.a. serial over USB). Any Serial Terminal can be used, though Riot has already
  installed `pyterm` in `riot/dist/tools/pyterm/`.
  ```console
  python3 dropalt/riot/dist/tools/pyterm/pyterm -p /dev/ttyACMx
  ```

### License
I am planning to have MIT license, but RIOT has its own license, LGPL2.1.
