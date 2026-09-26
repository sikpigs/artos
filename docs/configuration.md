# Configuration and port boundaries

## Sources of truth

| Source | Owns | Generated output / consumer |
| --- | --- | --- |
| `Kconfig`, board/SoC/module Kconfig files, `configs/*_defconfig` | Board selection and software resource limits | `scripts/configure.py` produces `autoconf.h`, `build_config.h`, and CMake selections |
| `bsp/boards/st/<board>/board.dts` and SoC DTSI | Selected console, pins, baud rate, initial clocks and device descriptions | dtc produces a DTB; `scripts/dtb_to_board.py` produces board headers |
| SoC `dts/bindings.json` | Reviewed mappings from hardware to C++ types | Validates the selected devices and pin/AF combinations |
| SoC SVD and `svd/regmap.json` | Register descriptions and reviewed corrections | `scripts/svd_to_regmap.py` produces checked-in register headers |
| MCU variant CMake file and `bsp/linker/cortex_m.ld.in` | Memory allocation and linker layout | Generated linker script and `mcu.h` |

Build-generated configuration and board headers live under `build/<preset>/generated/`.
Do not edit them. The DTB is a build intermediate, not a runtime device tree.
Generated node types expose constexpr properties; chosen roles select driver
bindings through `board::devices`. See [BSP generation and validation](../bsp/README.md#board-header-generation).

Both supported boards retain reset HSI at 16 MHz with unity bus dividers.
Changing a DTS clock value does not program a PLL. The Cortex-M port checks tick
clock divisibility and SysTick reload range. At 1 kHz the reload is 15,999.

## Kconfig workflow

```sh
./artos.py menuconfig
./artos.py build
./artos.py save-defconfig
```

The `debug`, `release`, and `min-size` presets select optimization/debugging
behavior and seed different SysTick scheduling cutoffs (2500, 500, and 800
cycles, respectively). Set `CONFIG_ARTOS_SCHEDULER_MIN_CYCLES_REMAINING` to
override a cutoff; zero disables it. A scheduling request below the cutoff
switches to idle and defers selection of a runnable thread until the next tick.
Each build directory's `.config` selects its board and is authoritative
once created. `KCONFIG_DEFCONFIG` can seed a new configuration explicitly;
`savedefconfig` writes a minimal seed to that build's `defconfig` for retention
under `configs/`.

Kconfiglib rejects unknown settings and out-of-range values. CMake tracks the
schemas, generator, seed and saved configuration; outputs are rewritten only when
content changes. No Kconfig processing occurs on the MCU.

The active [Kconfig](../Kconfig) files define option names, defaults, ranges,
dependencies, and help text. They are the reference for current resource limits
and scheduler options. Smaller numeric priority values mean higher scheduling
priority; the console RX buffer reserves one slot to distinguish full from empty.

Typed values appear in `build_config::artos` and `build_config::console`.
`CONFIG_CONSOLE` depends on `CONFIG_ARTOS`, and `CONFIG_MONITOR` depends on
`CONFIG_CONSOLE`. Disabling ARTOS therefore disables both console and monitor.
Disabling the monitor alone leaves the console available. Disabled-kernel typed
defaults support compile-only declarations; they do not supply kernel
implementations.

## Startup and source layout

`app/main.cpp` initializes board hardware, then the static system console, then
ARTOS. The console registers deferred RX work before UART IRQ enable and before
the kernel starts SysTick. Registration failure traps. Kernel initialization
resets the thread registry and tick count, creates idle, initializes the port,
and starts the tick. Main then creates monitor/blinky threads when configured
and starts the first thread through PendSV.

The main boundaries are `runtime/` for language and library startup,
`arch/arm/cortex_m4/` for processor support, `kernel/` for scheduling and
thread services, and `modules/` for the console and monitor. The source lists in
their CMake files are the current inventory of translation units.

The port's initial context is 68 bytes, including EXC_RETURN; that is not a usable
thread stack budget. `thread::minimum_stack_bytes` is 1024 bytes including saved
context. Idle and blinky use this minimum; the monitor has 4096 bytes. Main has
an independent 4096-byte stack reservation. PendSV preserves integer state and
conditionally saves/restores the high floating-point registers. Fault capture
uses a separate emergency stack. See [runtime constraints](../runtime/README.md).

The scheduler chooses the highest-priority ready application thread and rotates
equal-priority ties. Slot zero is the idle fallback. Thread IDs pack a slot index
and generation; deletion/reuse lifecycle support is not implemented. See
[wait and deferred contracts](../kernel/README.md).

## Verification

```sh
./artos.py check
./artos.py test
```

Compile-only and linked-image checks do not execute firmware. Hardware checks
should verify blocking/resumption, RX drop/overrun counters and tick timing.
See [native tests](../tests/host/README.md) and the [console smoke test](../modules/console/README.md#hardware-smoke-test).
