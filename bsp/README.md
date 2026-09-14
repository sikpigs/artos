# Board, SoC and driver support

The selected board composes an MCU variant, its SoC register definitions, shared
peripheral drivers, and CPU support. It does not own a private copy of them.

```text
bsp/
  boards/st/nucleo_f411re/    # Wiring, console, LED and debugger settings
  boards/st/nucleo_f439zi/
  soc/st/stm32f4/
    stm32f411/              # SVD/profile, generated headers, vector table
      variants/             # STM32F411RE memory/package selection
    stm32f439/
      variants/             # STM32F439ZI memory/package selection
  drivers/include/stm32/    # GPIO and USART implementations
  linker/cortex_m.ld.in     # Common sections with variant memory parameters
arch/arm/cortex_m4/
  CMakeLists.txt            # Core sources and optional scheduler support
  cpu.cmake                 # CPU/ABI flags, separate from tool discovery
  include/arch/             # Selected core's scheduler traits and register map
  src/                      # Reset, IRQ, faults, CPU and scheduler operations
```

| Board | SoC / variant | Console | Heartbeat | Default memory allocation |
| --- | --- | --- | --- | --- |
| NUCLEO-F411RE | STM32F411 / RE, LQFP64 | USART2, PA2 TX / PA3 RX | PA5 | 512 KiB flash, 128 KiB system SRAM |
| NUCLEO-F439ZI | STM32F439 / ZI, LQFP144 | USART3, PD8 TX / PD9 RX | PB0 (LD1) | 2 MiB flash, 192 KiB system SRAM |

Both boards start on reset HSI at 16 MHz, with APB/AHB dividers of one, and use
115200 baud. F439's separate 64 KiB CCM SRAM is described in `mcu.h` for address
validation but is not allocated to the main heap, task stacks or globals. It is
not a contiguous extension of the system SRAM. Backup SRAM is not treated as
ordinary initialized RAM. Package identity is recorded; complete package-pin
validation is future board-description work.

```sh
cmake --preset debug
cmake --build --preset debug --target all bsp-checks regmap-checks kconfig-checks
cmake --preset f439-debug
cmake --build --preset f439-debug --target all bsp-checks
cmake --preset f439-release
cmake --build --preset f439-release
```

`debug`/`release` seed the F411 board; `f439-debug`/`f439-release` seed the F439.
Saved `.config` files remain authoritative. Use separate build directories for
each board, or change the board through `menuconfig`. Firmware filenames follow
the selected board, e.g. `nucleo_f439zi_baremetal.elf`. The `flash` and
`debug-server` targets select the board's OpenOCD file. The root `openocd.cfg`
remains a compatibility entry point for F411. VS Code has separate launch/attach
configurations and build tasks for the two boards.

## Register generation

```sh
python3 scripts/svd_to_regmap.py --soc stm32f411 --check
python3 scripts/svd_to_regmap.py --soc stm32f439 --check
```

Omit `--check` to regenerate. Generation and freshness checks run clang-format
with the root `.clang-format` before comparing or writing headers. Install
clang-format (LLVM 18 is the current baseline); use `CLANG_FORMAT` to select
an executable consistently with the formatting hook. `--soc-dir` can point to another SoC directory;
its `svd/regmap.json` specifies the device, C++ namespace and pinned SVD hash.
The generator has no board-specific paths. Generated layouts are deliberately
chip-specific; matching SVD group names do not imply hardware compatibility.
GPIO/USART implementation reuse is through explicit register-type template
parameters, with layout checks and board-contract tests. F411's existing driver
names remain thin compatibility adapters; the board bindings use the shared API.

## Adding another board or chip

A new board for an existing part needs its Kconfig choice, board description,
bring-up and OpenOCD configuration under `boards/<vendor>/<board>`, plus a
software defconfig. Select an existing SoC and MCU variant. Reuse generated
headers and drivers without copying them.

A new part needs a SoC directory with a licensed SVD and correction profile,
generated register groups, a reviewed interrupt vector table and matching
`include/soc/irq_numbers.h`. Add its exact memory/package variant and Kconfig
selection. Peripheral implementations can be shared only after verifying their
layout and behavior. A different CPU also needs a CPU build profile and port.
The current build supports Cortex-M4F; this refactor does not claim arbitrary
architecture support.

Generic IRQ code uses one handler/context slot per SoC external interrupt slot.
It validates IRQ numbers, preserves PRIMASK while updating callbacks, and routes
weak external handlers through the active IPSR number. Strong application ISR
symbols can still override individual vectors. This costs 688 bytes of callback
slots on F411 and 728 bytes on F439 (two 32-bit pointers per slot). IRQ identifiers
and vector order belong to the SoC, while NVIC operations belong to the CPU.

Fault capture uses the variant's RAM ranges rather than an F411-specific limit.
The shared reset still initializes `.data`/`.bss`, enables the FPU and calls C++
constructors. Board initialization enables/configures the heartbeat LED; console
clocks are enabled explicitly by `init_console()`.

Build and linked-vector checks run without a board. Hardware validation should
check reset, ST-LINK console, LED and scheduler timing on the selected board.
The first port intentionally does not configure Ethernet, USB or a 180 MHz PLL.

## Board header generation

Install `device-tree-compiler` (provides both `dtc` and `fdtget`). Each build runs:

```text
boards/st/<board>/board.dts + soc/.../<soc>/dts/<soc>.dtsi
    -> dtc -> build/<preset>/generated/board/board.dtb
    -> scripts/dtb_to_board.py + soc/.../<soc>/dts/bindings.json
    -> board.h + board_devices.h
```

The DTB is a build intermediate; it is not linked into the firmware. CMake tracks
DTS includes with dtc's dependency file and regenerates on source, binding or
template changes. `board-headers` builds just these outputs; `devicetree-checks`
runs generator integration tests and compiles the existing board contracts.

DTS owns board wiring, baud rate and `/chosen` device selection. SoC DTSI owns
the initial peripheral inventory, register ranges, IRQs and reset HSI clock.
The code-owned `bindings.json` maps supported hardware to SVD-generated C++
types and validates console pin/AF routing. There are no C++ expressions in DTS.
Register-address and IRQ static assertions also cross-check the bindings against
the selected SoC headers. Memory/linker selection remains in the MCU variant.

`board.h` preserves the current `board::root`, `labels` and `chosen` API.
This first version emits a fixed role hierarchy for the selected devices, rather
than exporting arbitrary tree nodes or DTS labels. `board_devices.h` instantiates
the shared USART/GPIO drivers and emits RCC enables derived from the chosen pins
and UART. Handwritten `board.cpp` calls those helpers and initializes the LED.

### Initial binding contract

These are small Artos bindings, not Linux or Zephyr STM32 bindings. DTS include,
node override and phandle syntax is handled by dtc; libfdt's `fdtget` reads the
compiled data. The initial supported device graph is:

- `/chosen`: `artos,console`, `artos,kernel-tick`, `artos,heartbeat`, each one phandle.
- USART: `artos,stm32f4-usart`, `reg`, `interrupt-parent`, `interrupts`, `clocks`,
  `current-speed`, `artos,tx-pin`, `artos,rx-pin`, `artos,alternate-function`.
  Pin properties contain `<&gpio pin flags>`. Only the reviewed USART2 PA2/PA3
  mapping on F411 and USART3 PD8/PD9 on F439, both AF7, are currently bound.
- GPIO: `artos,stm32f4-gpio`, `reg`, `gpio-controller`, `#gpio-cells = <2>`.
  Initially the inventory includes GPIOA on F411 and GPIOB/GPIOD on F439.
- Heartbeat: a child of `gpio-leds` with `gpios = <&gpio pin 0>`.
  Only active-high pins are supported; conflicting console/LED pins are rejected.
- SysTick: `artos,cortex-m-systick`, its register range and one `clocks` phandle.
- Clock: `fixed-clock`, zero clock cells, 16 MHz. Startup retains reset HSI and
  unity bus dividers; changing the DTS frequency cannot configure a PLL.
- Interrupt parent: the SoC NVIC with one external IRQ-number cell.

Selected devices and their dependencies must be enabled (`okay`, `ok`, or absent
status). Invalid references, unsupported addresses, IRQs, clock settings, GPIO
flags, baud divisors, pin mappings and extra properties on selected device nodes
are errors. Unselected devices are not instantiated or comprehensively validated.
Address translation, active-low adapters, pinctrl states, general clock trees,
additional serial ports and other driver classes remain future extensions.
Package-wide pin availability is not yet checked.

For example, change `current-speed = <57600>;` in a board DTS to generate a
57600-baud console. Adding another peripheral requires both a DTS node and a
reviewed driver binding; an SVD register definition alone does not provide one.
