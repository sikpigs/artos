# NUCLEO-F411RE board support package

This in-tree CMake package contains the board description and STM32F411 support:

- `include/board.h`: hierarchical nodes, labels, and chosen devices.
- `include/board_devices.h`: adapters to the current peripheral drivers.
- `include/stm32f411/regmap.h`: chip registers and drivers, namespace `stm32f411`.
- `include/irq.h`, `src/irq.cpp`: IRQ API and this target's USART2 dispatch.
- `src/board.cpp`: board initialization and separate console clock initialization.
- `startup/`: reset handler and interrupt vector table.
- `linker/`: flash/RAM layout template; CMake generates the script with the
  configured monitor reservation (zero when disabled).
- `openocd.cfg`: ST-LINK and reset settings.

Firmware links `bsp::board`, which supplies include paths, board sources, startup,
and linker settings. Compile-only checks link `bsp::headers`. Public includes
remain `<board.h>`, `<board_devices.h>`, and `<irq.h>`; CMake selects their provider.
Chip-specific consumers include `<stm32f411/regmap.h>`.

The package depends on the root `firmware_headers` target for generic MMIO
operations. It is an in-tree package, not a standalone installable SDK. ARTOS,
its Cortex-M port, software configuration, and generic C++ runtime support live
outside the BSP. CPU compiler flags remain in the Arm toolchain file; adding a
different CPU will also require selecting matching flags and an ARTOS port.

Configure with `cmake --preset debug`, then use the `menuconfig` build target
to select the board. Its choice entry is defined in this package's `Kconfig`. The root `openocd.cfg`
forwards to this BSP for existing command-line and VS Code entry points. CMake's
flash and debug-server targets use the selected BSP's configuration directly.

The board and chip headers are still handwritten scaffolding for future SVD/DTS generation.
Software configuration is generated from Kconfig in the build directory.
See [configuration boundaries](../../docs/configuration.md) for current limits.
