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
cmake --preset debug
cmake --build --preset debug --target menuconfig
cmake --build --preset debug
cmake --build --preset debug --target savedefconfig
```

The `debug`, `release`, and `min-size` presets select only optimization/debugging
behavior. Each build directory's `.config` selects its board and is authoritative
once created. `KCONFIG_DEFCONFIG` can seed a new configuration explicitly;
`savedefconfig` writes a minimal seed to that build's `defconfig` for retention
under `configs/`. The old `-DBSP` selector is retired.

Kconfiglib rejects unknown settings and out-of-range values. CMake tracks the
schemas, generator, seed and saved configuration; outputs are rewritten only when
content changes. No Kconfig processing occurs on the MCU.

| Setting | Default / range | Meaning |
| --- | --- | --- |
| `CONFIG_ARTOS_TICK_HZ` | 1000 / 1–1000000 | Tick frequency; hardware divisibility/reload checks also apply |
| `CONFIG_ARTOS_MAX_THREADS` | 8 / 2–64 | Registry slots including idle; TCBs and stacks are separately supplied |
| `CONFIG_ARTOS_PRIORITY_LEVELS` | 16 / 1–256 | Smaller numeric values have higher scheduling priority |
| `CONFIG_ARTOS_MAX_DEFERRED_WORK_ITEMS` | 8 / 1–64 | Registered callbacks, independent of thread count |
| `CONFIG_CONSOLE_RX_BUFFER_SIZE` | 256 / 2–65536 | RX array bytes; usable capacity is N−1 |

Typed values appear in `build_config::artos` and `build_config::console`.
`CONFIG_MONITOR` depends on `CONFIG_CONSOLE`. Disabling the monitor leaves the
console and its buffer available. Disabling the console also disables the monitor.
Rename legacy `CONFIG_MONITOR_RX_BUFFER_SIZE` to `CONFIG_CONSOLE_RX_BUFFER_SIZE` in
saved configurations. The former monitor workspace setting no longer exists.

`CONFIG_CONSOLE` depends on `CONFIG_ARTOS`, and `CONFIG_MONITOR` depends on
`CONFIG_CONSOLE`. Disabling ARTOS therefore disables both console and monitor,
even if an input configuration requests them. Disabled-kernel typed defaults
support compile-only declarations; they do not supply kernel implementations.

## Startup and source layout

`app/main.cpp` initializes board hardware, then the static system console, then
ARTOS. The console registers deferred RX work before UART IRQ enable and before
the kernel starts SysTick. Registration failure traps. Kernel initialization
resets the thread registry and tick count, creates idle, initializes the port,
and starts the tick. Main then creates monitor/blinky threads when configured
and starts the first thread through PendSV.

| Location | Responsibility |
| --- | --- |
| `runtime/` | C/C++ initialization, guards, abort and newlib hooks |
| `arch/arm/cortex_m4/` | Reset, IRQ dispatch, faults, interrupt masking, SysTick and context switching |
| `kernel/kernel.cpp` | Kernel initialization, startup and tick orchestration |
| `kernel/thread.cpp` | Thread creation and sleep |
| `kernel/scheduler.cpp` | Registry, current/next thread, selection and switch bookkeeping |
| `kernel/blocking.cpp` | Wait registration and blocked-to-ready transitions |
| `kernel/wait.cpp` | Queue membership only |
| `kernel/deferred.cpp` | Fixed callback registrations, pending flags and dispatch |
| `kernel/internal/` | Private TCB and cross-file declarations |
| `modules/console/` | DTS-bound buffered transport and blocking byte reads |
| `modules/monitor/` | Line editing and memory commands |

The port's initial context is 68 bytes, including EXC_RETURN; that is not a usable
thread stack budget. `thread::minimum_stack_bytes` is 1024 bytes including saved
context. Idle and blinky use this minimum; the monitor has 4096 bytes. Main has
an independent 4096-byte stack reservation. PendSV preserves integer state and
conditionally saves/restores the high floating-point registers. Fault capture
uses a separate emergency stack. See [runtime constraints](../runtime/README.md).

The scheduler scans a fixed thread registry, chooses the highest-priority runnable
application thread, and rotates equal-priority ties relative to the current slot.
Slot zero is idle fallback. Declared ready-list storage is not yet used for
selection. Thread IDs pack a slot index and generation; deletion/reuse lifecycle
support is not implemented. See [wait and deferred contracts](../kernel/README.md).

## Verification

```sh
cmake --build --preset debug --target bsp-checks configuration-checks kconfig-checks devicetree-checks regmap-checks
```

Compile-only and linked-image checks do not execute firmware. Hardware checks
should verify blocking/resumption, RX drop/overrun counters and tick timing.
See [native tests](../tests/host/README.md) and the [console smoke test](../modules/console/README.md#hardware-smoke-test).
