# NUCLEO-F411RE

This directory owns board wiring (`board.dts`), LED/console clock bring-up
(`src/board.cpp`), its Kconfig choice and OpenOCD configuration. Chip registers,
interrupt vectors and memory variants live under `bsp/soc/st/stm32f4/stm32f411`.
GPIO/USART implementations and Cortex-M4 reset/IRQ/fault support are shared.

- MCU: STM32F411RE, LQFP64, 512 KiB flash and 128 KiB SRAM.
- Console: USART2, PA2 TX / PA3 RX, AF7, 115200 baud.
- Heartbeat: active-high PA5 LED.
- Clock: reset HSI at 16 MHz; no PLL setup.
- Build presets: `debug`, `release`.

See [BSP architecture and workflow](../../../README.md).
