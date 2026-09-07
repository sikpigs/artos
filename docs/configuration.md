# Configuration and port boundaries

The board/chip headers are handwritten examples of future generator output.
Kconfiglib now resolves board selection and software policy at build time; there
is no DTS parser, DTB, or SVD generator yet.

| File | Responsibility | Future producer |
| --- | --- | --- |
| `include/mmio.h` | Generic typed register and field operations | Handwritten |
| `bsp/nucleo_f411re/include/stm32f411/regmap.h` | Existing STM32 register descriptions and peripheral drivers | SVD for declarations; drivers remain handwritten |
| `bsp/nucleo_f411re/include/board.h` | Hardware nodes, clocks, pins, labels, chosen devices | DTS generator |
| `build/<preset>/generated/build_config.h` | ARTOS/monitor enable, tick rate, priority levels, monitor buffer/workspace sizes | Kconfig generator |
| `bsp/nucleo_f411re/include/board_devices.h` | Bind board nodes to peripheral drivers | Handwritten |
| `bsp/nucleo_f411re/src/board.cpp` | Board initialization and explicit console clock enables | Handwritten bring-up |
| `modules/monitor/src/monitor.cpp`, `modules/monitor/src/wozmon.s` | Optional console and monitor | Handwritten |
| `include/artos_port.h` | ARTOS port interface and selected CPU traits | Handwritten |
| `src/ports/cortex_m4.cpp` | SysTick access, ISR bridge, initial thread stack frame | Handwritten CPU port |
| `src/artos.cpp` | Tick accounting and thread metadata | Handwritten kernel |

## Description contract

Nodes are nested types. Properties are typed `static constexpr` members.
References, labels, and chosen roles are type aliases to the same node, preserving
identity. Clock dependencies are explicit references rather than inferred from
node containment. Future overlays must be resolved before generating this header.
There is no runtime tree, lookup table, or static initialization of devices.

`board::chosen::console` selects the UART node. `board::devices::console` binds
that node to the current USART driver with its clock as a template argument.
`board::chosen::kernel_tick` selects the timer description used by the CPU port.
The current adapter and bring-up support this board's USART2 and Cortex-M SysTick;
choosing another peripheral requires its corresponding adapter and bring-up.
Pin compatibility and resource conflict validation are not implemented yet.

Hardware facts belong in the board/chip description. Software policy belongs in
`build_config`. A tick frequency, for example, lives only in `build_config`, and
the port combines it with the board's timer clock. The port rejects inexact tick
divisors and reloads outside the usable SysTick range at compile time.

## Startup and current scope

`main()` calls `board::init()` before constructing the console and, when enabled,
ARTOS. Disabling `CONFIG_ARTOS` excludes its implementation and CPU port from
the firmware and leaves the monitor operating directly through IRQs.
Monitor startup calls `board::init_console()` to enable GPIOA and USART2 and
read back their enable registers before peripheral access. `board::init()`
retains the reset clock tree: 16 MHz HSI,
AHB /1, APB1 /1. Changing a frequency constant alone does not program a PLL;
clock-tree changes need matching bring-up code.

The tick reload is now 15,999 for the configured 1 kHz tick. Previously the
hardcoded reload of 99,999 assumed a 100 MHz clock that startup did not configure.

The CPU port owns stack word size, eight-byte alignment, and the basic 64-byte
initial context. Thread storage rejects undersized or misaligned byte counts,
and aligns the opaque TCB storage. TCB size remains a kernel implementation
detail, checked against the internal structure. The public creation wrapper now
forwards the stack byte count to the internal function.

Thread scheduling and context switching remain unfinished, as before. The initial
frame covers the basic integer context; floating-point context switching is not
implemented. The existing placeholder thread ID behavior is unchanged.

The chip register namespace is `stm32f411`, with declarations in the BSP header
`stm32f411/regmap.h`. Splitting its peripheral drivers from SVD-generated
declarations is a later step. The BSP owns startup, IRQ support, memory layout,
and debugger settings; the generic MMIO helpers and ARTOS CPU port stay outside it.

## Verification

```sh
cmake --preset debug
cmake --build --preset debug
cmake --build --preset debug --target configuration-checks kconfig-checks
cmake --preset release
cmake --build --preset release
cmake --build --preset release --target configuration-checks
```

`configuration-checks` is a compile-only target. It checks node identity, valid
and invalid stack sizes/priorities for the selected configuration, and instantiates the
public thread creation wrapper. It does not execute or flash firmware. Hardware
verification should check the console and measure SysTick timing on the board.

## Kconfig flow

`Kconfig` includes the board choice entry from `bsp/nucleo_f411re/Kconfig`
and the monitor options from `modules/monitor/Kconfig`.
`scripts/configure.py` uses Kconfiglib to resolve the build directory's `.config`
(or the defconfig seed on first configure). It emits CMake board/source selections,
a conventional `autoconf.h`, and the typed `build_config.h`. Outputs are rewritten
only when their contents change. Unknown symbols and invalid ranges fail the
configuration step rather than silently accepting fallback values.

`cmake/kconfig.cmake` tracks the saved configuration, Kconfig files, and generator
so editing options triggers regeneration before compilation. `menuconfig` edits
that build's `.config`; `savedefconfig` exports a minimal seed into that build's
`defconfig`. A seed selected with `KCONFIG_DEFCONFIG` is used only when `.config`
is absent. Defaults do not override saved user choices.

The only board currently supported is NUCLEO-F411RE. Adding another board requires
a Kconfig choice entry, a generator mapping to its package, and matching BSP/CPU
support. The existing Arm toolchain still supplies Cortex-M4F compiler flags.

When ARTOS is disabled, its typed configuration retains inert defaults for public
header declarations; the implementation is not linked. Kconfig's own generated
`autoconf.h` omits the disabled subsystem values. Firmware code should use
`CONFIG_ARTOS` or `build_config::artos::enabled` to test availability.

`kconfig-checks` runs host-side generator tests for saved choices, stable outputs,
disabled ARTOS, and invalid inputs. No Kconfig code runs on the MCU.

## Optional monitor and memory

`CONFIG_MONITOR` controls compilation of `modules/monitor/src/monitor.cpp` and `modules/monitor/src/wozmon.s`.
When off, there is no monitor call, receive buffer, workspace object, or console
clock initialization. Main enters a `wfi` idle loop after optional ARTOS startup.
The BSP's IRQ support remains available for future application consumers.

`CONFIG_MONITOR_WORKSPACE_SIZE` controls the reserved workspace (default 4096
bytes, minimum 160). This includes the command buffer and monitor state; the
receive ring buffer is separate. The generated BSP linker script receives this
size from the same Kconfig resolution as the C++ header. With the monitor off,
both generated buffer/workspace sizes are zero and the workspace section reserves
no bytes. Linker and C++ assertions check that the workspace object fits when on.
The independent main stack remains 4096 bytes in every configuration.

Verified all four ARTOS/monitor combinations in Release, including symbol absence
for disabled monitor code/data and zero workspace reservation. With ARTOS enabled,
the current linked RAM usage outside the main stack drops from 4480 to 12 bytes
when the monitor is disabled. A configuration-only change to a 160-byte workspace
also regenerated and relinked successfully.

The monitor is packaged under `modules/monitor` and linked through the
`monitor::monitor` CMake target only when enabled. Its public header is
`<monitor/monitor.h>`. Kconfig dependency tracking includes module schemas, so
changes to its options regenerate the build. The BSP retains the physical linker
layout; the module owns the workspace objects and option definitions. Provenance
and unresolved upstream licensing are recorded in the module's `PROVENANCE.md`.
