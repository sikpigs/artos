# ARTOS on STM32 Nucleo

A C/C++23 and assembly project for NUCLEO-F411RE and NUCLEO-F439ZI, using GNU Arm
Embedded and CMake/Ninja without a vendor HAL. The monitor uses USART2 (PA2/PA3)
on F411 or USART3 (PD8/PD9) on F439, at 115200 baud. Both start at 16 MHz HSI.
See [board/SoC architecture and F439 setup](bsp/README.md). The commands below
default to F411; use the `f439-debug` / `f439-release` presets for F439.

C/C++ startup and library hooks live in [runtime/](runtime/README.md).
The [kernel](kernel/README.md) provides priority scheduling, blocking waits and
tick-serviced deferred callbacks. The [buffered console](modules/console/README.md)
uses these to suspend the monitor while waiting for RX input.

## Prerequisites

On Debian/Ubuntu:

```sh
sudo apt install cmake ninja-build gcc-arm-none-eabi libstdc++-arm-none-eabi-newlib openocd gdb-multiarch python3-venv device-tree-compiler
code --install-extension marus25.cortex-debug
```

CMake 3.24 or newer is required. Install the other recommended VS Code
extensions when opening the folder. On other platforms, install GNU Arm
Embedded including `arm-none-eabi-gdb`, plus OpenOCD and Ninja, on `PATH`.
Linux launch configurations use `gdb-multiarch` and explicitly select
`arm-none-eabi-objdump`; Cortex-Debug finds `arm-none-eabi-nm` beside it.

Connect a USB data cable to the board's **ST-LINK USB connector**, with the board’s ST-LINK-to-target connections in their default configuration.

Install the pinned host-side Kconfig dependency once from the project root:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
```

CMake detects this environment automatically. An explicit
`-DPython3_EXECUTABLE=/path/to/python` can select another interpreter with
Kconfiglib installed. Configuration tools run only on the host.

## Pre-commit formatting and clang-tidy checks

Install clang-format and clang-tidy and enable the versioned hook once per clone:

```sh
sudo apt install clang-format clang-tidy
git config core.hooksPath .githooks
```

The hook checks the full staged contents of added, copied, renamed, or modified
C/C++ source and header files against the staged `.clang-format` configuration.
An empty formatting diff allows the commit; otherwise it prints the diff and
blocks the commit. Missing tools or formatter errors also block the commit with
a reason. Working files and the Git index are left untouched, including partially
staged changes. Deleted files and other file types are skipped.

Fix reported files with `clang-format -i path/to/file.cpp`, then stage the desired
changes again (use `git add -p` for partial staging). Set `CLANG_FORMAT` to select
a specific executable, such as `clang-format-18`. Python 3 is also required.
After formatting passes, clang-tidy analyzes an isolated copy of the staged tree.
It promotes enabled warnings to errors for commits and blocks on diagnostics,
missing tools, or parsing errors. It never applies fixes. Stage `.clang-tidy`
along with the hook when first adding them to the repository.

Configure the board builds and host tests needed by your staged changes:

```sh
cmake --preset debug
cmake --preset f439-debug
cmake -S tests/host -B build/host-tests -G Ninja
```

The hook reads the active `compile_commands.json` first, then other databases in
`build/*/compile_commands.json` to cover board-specific sources and native tests.
For shared sources it selects the first available command, rather than checking
every board or test variant. Each selected command retains its own build directory,
generated headers and compiler target. Host builds export their database automatically.

`CLANG_TIDY_BUILD_DIR` restricts analysis to one build. Alternatively,
`CLANG_TIDY_BUILD_DIRS` supplies an ordered list separated by the platform path
separator (`:` on Linux), for example `build/f439-debug:build/debug:build/host-tests`.
`CLANG_TIDY` selects the executable. GNU Arm library paths are discovered from
the selected command's compiler. The analysis snapshot omits GCC’s ARM
`-mgeneral-regs-only` flag because Clang 18 rejects it; the firmware build retains
that restriction. Static analysis does not verify generated instruction usage.

Changed sources must appear in at least one selected database. Header changes
(including deletions) and `.clang-tidy` changes check all available staged C/C++
translation units. Unused headers and unconfigured variants are not covered.
The hook copies generated artifacts from local builds, so keep them current with
the staged configuration. It analyzes the staged source tree without applying
fixes or changing staging. Staged symlinks are rejected.

Run both hooks' tests with
`python3 -m unittest discover -s tests -p 'test_pre_commit_clang*.py'`.

## Optional clang-tidy analysis

The root `.clang-tidy` enables selected correctness checks, const-correctness,
`nullptr` modernization, and snake_case member naming without leading/trailing
underscores. Manual runs are advisory (`WarningsAsErrors: ''`);
the pre-commit hook overrides this with `--warnings-as-errors=*`. The header filter includes handwritten
headers in the current include layout and omits generated board/register headers.
Extend the filter when adding new nested include directories. Main source file
diagnostics are always reported.

Install clang-tidy, configure the desired board preset, and validate the settings:

```sh
sudo apt install clang-tidy
cmake --preset debug
clang-tidy --verify-config
```

Clang needs the ARM target and GNU Arm standard-library headers in addition to
the compilation database. This invocation was tested with clang-tidy 18 and
the Ubuntu GNU Arm 13.2.1 installation; adjust the include paths for your system
(inspect `arm-none-eabi-g++ -E -x c++ -v /dev/null`):

```sh
clang-tidy -p build/debug runtime/cxa_guard.cpp \
  --extra-arg=--target=arm-none-eabi \
  --extra-arg=-isystem/usr/lib/arm-none-eabi/include/c++/13.2.1 \
  --extra-arg=-isystem/usr/lib/arm-none-eabi/include/c++/13.2.1/arm-none-eabi \
  --extra-arg=-isystem/usr/lib/arm-none-eabi/include
```

Use `-p build/f439-debug` for the F439 compilation database. Run checks on `.cpp`
translation units to analyze their included headers in context. Omit `--fix`
while reviewing the initial findings.

The installed Clang 18 / GNU Arm 13.2.1 combination does not expose
`std::expected`. `include/expected_compat.h` automatically selects the local
fallback for that combination, allowing `app/main.cpp` to be analyzed. The local
implementation supports integral/enum payloads and is covered by native tests.
See [the expected tests](tests/host/README.md#expected-compatibility-implementation)
for its limited contract. Compiler errors still fail analysis even with advisory
checks.

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

## VS Code formatting and diagnostics

Install the recommended **clangd** extension (`llvm-vs-code-extensions.vscode-clangd`)
and let it install its language server if prompted. Configure a board preset so
the root `compile_commands.json` link points to the desired build. After changing
the server arguments, run **clangd: Restart language server** from the Command
Palette (or reload the window).

C and C++ files use clangd as their default formatter and format on save using
the root `.clang-format`. **Format Document** also works. Clangd uses its bundled
formatter, so keep its LLVM version aligned with the standalone clang-format
used by the pre-commit hook if their formatting results differ.

Clang-tidy diagnostics run while editing using `.clang-tidy`; supported fixes
are available through **Quick Fix**. The `.clangd` configuration allows slower
checks too. Clangd does not run every standalone clang-tidy check, so the
pre-commit hook remains the full check. Formatting runs on save; tidy fixes are
applied when you select them. `.clangd` prefers angle brackets for inserted
includes; clang-format 18 does not convert existing include delimiters. Project
conventions are recorded in [AGENTS.md](AGENTS.md). See the [clangd configuration reference](https://clangd.llvm.org/config#clangtidy).

The configured `--query-driver` allows clangd to query the installed GNU Arm
compiler for the target and system include paths. Update that path in
`.vscode/settings.json` if your toolchain lives outside `/usr/bin`.

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

### Peripheral registers

During either debug session, expand **XPERIPHERALS** in Run and Debug to inspect
named registers and bitfields. Each board’s launch/attach configurations use its bundled
SoC SVD; no download is needed at debug time. Install the recommended
`mcu-debug.peripheral-viewer` extension if this panel is missing.
See the [SVD source and license](bsp/soc/st/stm32f4/stm32f411/svd/README.md).
Peripheral reads can have side effects (for example, reading USART data consumes
received data); avoid expanding data registers while using the serial monitor.

### Integrated serial console

On Linux, run **Tasks: Run Task > Serial console NUCLEO-F411RE**. It opens a
dedicated VS Code terminal at **115200 baud, 8N1, no flow control**, using the
ST-LINK USB virtual COM port. It works alongside F5 debugging; the firmware's
monitor processes input only while the CPU is running. **Ctrl+]** closes the
console; Ctrl+C is sent to the board.

The dependency-free Python script selects a single ST-LINK device from
`/dev/serial/by-id/`. If more than one board is connected, set
`NUCLEO_SERIAL_PORT` in the environment used to start VS Code, or run:

```sh
python3 scripts/serial_console.py --port /dev/serial/by-id/YOUR_STLINK_DEVICE
```

An explicit `/dev/ttyACM0` also works. Close other serial terminals before
opening this one. If access is denied, check the device's permissions and your
serial-device group membership (usually `dialout` on Debian/Ubuntu). The console
does not build, reset, or flash the target. On Windows use a serial terminal
with the same settings and the board's COM port.

### Fault capture

HardFault, MemManage, BusFault and UsageFault now capture diagnostic state in
`fault_record` and stop at a breakpoint when a debugger is connected. Without a
debugger they stay in the fault loop; an attach session can inspect the record.
The handlers use a separate 512-byte emergency stack and avoid floating-point
instructions so capture does not trigger lazy FP stacking.

Add `fault_record` to **Watch**, or enter these in the Cortex-Debug Debug Console:

```text
-exec p/x fault_record
-exec info line *fault_record.frame.pc
-exec x/8i fault_record.frame.pc
```

Check `complete == 1` first. `exception` identifies HardFault (3), MemManage (4),
BusFault (5), or UsageFault (6). `cfsr` and `hfsr` describe the cause; `mmfar` and
`bfar` are meaningful only when CFSR's MMARVALID (bit 7) and BFARVALID (bit 15)
are set, respectively. `exc_return`, `msp` and `psp` preserve the entry context.
`frame` contains the stacked core registers, including PC and LR; use it only
when `frame_valid == 1`. Invalid SRAM pointers and stacking errors suppress the
frame read. The PC is not necessarily the offending instruction for imprecise
bus faults. See [ST's programming manual, fault handling and status registers](https://www.st.com/resource/en/programming_manual/pm0214-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf).

If an existing debugger exception catch stops before capture has run, continue
to the handler's breakpoint. Reset to recover after inspecting the fault;
continuing stays in the fault loop. **Reset, relaunch and flashing clear the
record**; use Attach with a matching ELF to inspect an existing crash. Configurable
fault enable bits retain their existing settings, so disabled fault classes
still escalate to HardFault.

Host checks (no board needed):

```sh
python3 tests/test_serial_console.py
g++ -std=c++23 -Wall -Wextra -Werror -fsyntax-only \
  -Iinclude -Ibuild/debug/generated tests/fault_capture.cpp
```

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

CMake and VS Code use each board's OpenOCD configuration; root `openocd.cfg`
is the F411 compatibility entry point. They use OpenOCD's Nucleo-F4 board
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

Supported targets are NUCLEO-F411RE (512 KiB flash, 128 KiB system SRAM) and
NUCLEO-F439ZI (2 MiB flash, 192 KiB system SRAM). Both reserve 4 KiB for the main
stack. F439 CCM is described but not allocated by the linker. Other variants need
reviewed CPU, vector, memory and pin bindings; see [board support](bsp/README.md).

## Configuration and ARTOS port

Kconfig selects the board and software settings. To edit the Debug configuration:

```sh
cmake --preset debug
cmake --build --preset debug --target menuconfig
cmake --build --preset debug
```

The menu offers board selection, ARTOS settings, console enable and RX buffer
size, and monitor enable. `CONFIG_CONSOLE_RX_BUFFER_SIZE` defaults to 256 bytes
with 255 usable ring slots. Disabling `CONFIG_MONITOR` removes command processing
but leaves the system console available. `CONFIG_CONSOLE` controls the transport
and RX storage. Kconfig enforces the console’s ARTOS dependency; disabling ARTOS also disables
the console and monitor. See [configuration guidance](docs/configuration.md).
The main stack reservation remains 4 KiB.

Pins and device selection live in each board’s `board.dts`; CMake generates
`board.h` and `board_devices.h` through dtc/DTB. See [board generation](bsp/README.md#board-header-generation).

Each build directory has its own `.config`; Debug and Release selections are
independent. The first configure uses the selected preset’s F411 or F439 defconfig.
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
and the [NUCLEO-F411RE BSP](bsp/boards/st/nucleo_f411re/README.md) for details.

The optional [monitor module](modules/monitor/README.md) contains its implementation,
Kconfig options, and supported command syntax.

## License

The main project code and documentation are licensed under the [MIT License](LICENSE),
copyright (c) 2026 Rob Lake.

The independently written C++ monitor in `modules/monitor/` is included in this grant.
Third-party dependencies retain their respective licenses.
