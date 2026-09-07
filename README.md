# NUCLEO-F411RE bare-metal project

A C/C++23 and assembly project for the STM32F411RE on the NUCLEO-64 board,
using GNU Arm Embedded and CMake/Ninja without a vendor HAL. The current
firmware runs a monitor over USART2 (PA2/PA3) at 115200 baud.

## Prerequisites

On Debian/Ubuntu:

```sh
sudo apt install cmake ninja-build gcc-arm-none-eabi libstdc++-arm-none-eabi-newlib openocd gdb-multiarch python3-venv
code --install-extension marus25.cortex-debug
```

CMake 3.24 or newer is required. Install the other recommended VS Code
extensions when opening the folder. On other platforms, install GNU Arm
Embedded including `arm-none-eabi-gdb`, plus OpenOCD and Ninja, on `PATH`.
Linux launch configurations use `gdb-multiarch` and explicitly select
`arm-none-eabi-objdump`; Cortex-Debug finds `arm-none-eabi-nm` beside it.

Connect a USB data cable to the board's **ST-LINK USB connector**, with both
CN2 jumpers fitted to connect the on-board ST-LINK to the target MCU.

Install the pinned host-side Kconfig dependency once from the project root:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
```

CMake detects this environment automatically. An explicit
`-DPython3_EXECUTABLE=/path/to/python` can select another interpreter with
Kconfiglib installed. Configuration tools run only on the host.

## Build and flash

Run these commands from the project root:

```sh
cmake --preset debug
cmake --build --preset debug
cmake --build --preset debug --target flash
```

Artifacts are in `build/debug/`: ELF with symbols, HEX, BIN and linker map.
Debug builds use `-O0 -g3` for source stepping and macro information. Release
builds are available through the `release` configure/build presets.

The `flash` target builds, programs, verifies and resets the board. It is
available when OpenOCD is found at configure time. Set
`-DOPENOCD_EXECUTABLE=/path/to/openocd` during configuration if needed.
The equivalent direct command is:

```sh
openocd -f openocd.cfg \
  -c 'program {build/debug/nucleo_f411re_baremetal.elf} verify reset exit'
```

## VS Code debugging

1. Open this folder and select **Debug NUCLEO-F411RE (ST-LINK/OpenOCD)**
   in Run and Debug.
2. Press **F5**. This configures and builds the Debug preset, flashes the ELF,
   resets the board and stops at `main`, regardless of the preset selected
   in CMake Tools.
3. Set source breakpoints, use **F10** to step over, **F11** to step into,
   and **F5** to continue. Cortex-Debug provides registers and memory views.

**Attach NUCLEO-F411RE (ST-LINK/OpenOCD)** connects and halts without building
or flashing. Its `build/debug/` ELF must match the firmware already on the
board. To debug startup, change `runToEntryPoint` to `Reset_Handler` in
`.vscode/launch.json`.

**Tasks: Run Task > Flash NUCLEO-F411RE** builds/flashes the Debug preset.
The regular **CMake: build** task still follows the selected CMake preset.

If tools are outside `PATH`, set `cortex-debug.openocdPath` and
`cortex-debug.armToolchainPath` in VS Code user settings. For a different
Linux GDB or binutils installation, update the `linux.gdbPath` and
`linux.objdumpPath` launch overrides. Set `TOOLCHAIN_PREFIX` through a local
`CMakeUserPresets.json` for a custom compiler toolchain.

## Command-line debugging

In one terminal, start OpenOCD:

```sh
cmake --build --preset debug --target debug-server
# Or: openocd -f openocd.cfg
```

In another terminal:

```sh
gdb-multiarch build/debug/nucleo_f411re_baremetal.elf
```

Use `arm-none-eabi-gdb` instead on platforms with GNU Arm's GDB. In GDB:

```gdb
target extended-remote localhost:3333
monitor reset halt
load
monitor reset halt
thbreak main
continue
next
info registers
```

`load` programs the board; omit it when attaching to matching firmware.
To finish and leave firmware running:

```gdb
monitor reset run
detach
quit
```

Stop OpenOCD with Ctrl-C. Only one OpenOCD/debug session can own the probe at
a time; stop the terminal server before starting an F5 session.

## OpenOCD configuration and troubleshooting

All entry points share `openocd.cfg`. It uses OpenOCD's Nucleo-F4 board
configuration (ST-LINK, SWD, hardware reset), binds to localhost, and uses
950 kHz, supported by the connected ST-LINK/V2.1. Reset hooks keep this speed
and leave clock initialization to firmware instead of applying the stock
STM32F4 script's PLL changes.

- **`objdump-multiarch` / `nm-multiarch` ENOENT:** use the explicit
  `arm-none-eabi-objdump` override in both Linux launch configurations.
  Verify `arm-none-eabi-objdump` and `arm-none-eabi-nm` are on `PATH`.
- **GDB missing:** install `gdb-multiarch` on Ubuntu; the native `gdb` may
  support only the host architecture.
- **OpenOCD `open failed`:** check the cable, ST-LINK connector and CN2
  jumpers, then stop any other debug servers. On Linux, ensure the OpenOCD
  package's ST-LINK udev rules are installed and replug the board after
  reloading rules. Run the debugger as your normal user.
- **Missing newlib source:** packaged libraries contain references to their
  original build paths. This warning does not indicate a connection failure;
  continue to `main` or step over library calls. An attach session also needs
  an ELF matching the flashed image to show reliable source locations.
- **Multiple probes:** add `adapter serial YOUR_STLINK_SERIAL` to
  `openocd.cfg` to select one.

Configuration references: [OpenOCD reset configuration](https://openocd.org/doc/html/Reset-Configuration.html)
and [Cortex-Debug launch attributes](https://github.com/Marus/cortex-debug/blob/master/debug_attributes.md).

## Hardware assumptions

This targets the **NUCLEO-F411RE**, with 512 KiB flash and 128 KiB SRAM
(including 4 KiB reserved for the main stack). Other NUCLEO-64 variants need
matching CPU flags, interrupt vectors, memory sizes and pin assignments.

## Configuration and ARTOS port

Kconfig selects the board and software settings. To edit the Debug configuration:

```sh
cmake --preset debug
cmake --build --preset debug --target menuconfig
cmake --build --preset debug
```

The menu offers the NUCLEO-F411RE board (currently the only board), ARTOS
on/off, kernel tick frequency, priority levels, monitor on/off, receive-buffer
size, and reserved monitor workspace size.
Disabling `CONFIG_MONITOR` removes the monitor code, receive buffer, and workspace
reservation. With no monitor, main idles after board and optional ARTOS startup.
`CONFIG_MONITOR_WORKSPACE_SIZE` defaults to 4096 bytes and can be reduced to
160 bytes for the current workspace; the receive buffer is separate. The main
stack reservation remains 4 KiB.

Pins and hardware clocks remain in `bsp/nucleo_f411re/include/board.h`.

Each build directory has its own `.config`; Debug and Release selections are
independent. The first configure seeds it from `configs/nucleo_f411re_defconfig`.
Subsequent builds use the saved `.config` and automatically regenerate the
CMake selections and `generated/build_config.h` when it changes.

```sh
cmake --build --preset debug --target savedefconfig
```

This exports a minimal configuration to `build/debug/defconfig`. Copy it into
`configs/` to keep it, then pass `-DKCONFIG_DEFCONFIG=/path/to/defconfig` when
configuring a fresh build directory. The seed does not overwrite an existing
`.config`. Board selection replaces the old `-DBSP` option.

ARTOS accesses CPU-specific timer and stack operations through
`include/artos_port.h`. See [configuration boundaries](docs/configuration.md)
and the [NUCLEO-F411RE BSP](bsp/nucleo_f411re/README.md) for details.

The optional [monitor module](modules/monitor/README.md) contains its implementation,
Kconfig options, and source provenance notes.

## License

The main project code and documentation are licensed under the [MIT License](LICENSE),
copyright (c) 2026 Rob Lake.

The entire `modules/monitor/` directory is excluded from this grant while its
licensing remains unresolved; see its [provenance notes](modules/monitor/PROVENANCE.md).
Third-party dependencies retain their respective licenses.
