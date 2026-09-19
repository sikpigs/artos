# NUCLEO-F439ZI

- MCU: STM32F439ZI, LQFP144, 2 MiB flash and 192 KiB system SRAM.
- Additional 64 KiB CCM SRAM is described but not allocated by the linker.
- Console: USART3, PD8 TX / PD9 RX, AF7, 115200 baud through ST-LINK VCP.
- Heartbeat: active-high LD1 on PB0, assuming the standard SB120 ON / SB119 OFF wiring.
- Clock: reset HSI at 16 MHz, APB/AHB divide by one; no external clock or PLL setup.

Wiring source: [ST UM1974](https://www.st.com/resource/en/user_manual/dm00244518-stm32-nucleo-144-boards-mb1137-stmicroelectronics.pdf),
LED and USART communication sections. VCP uses the board's standard solder-bridge
configuration. Memory source: [STM32F439ZI datasheet](https://www.st.com/resource/en/datasheet/stm32f439zi.pdf).

```sh
cmake --preset debug
cmake --build --preset debug --target menuconfig # select NUCLEO-F439ZI
cmake --build --preset debug --target all bsp-checks
# After connecting this board:
cmake --build --preset debug --target flash
```

Use the F439-specific VS Code launch/attach configurations. The F411 root
`openocd.cfg` is not the F439 entry point; CMake's `flash` and `debug-server`
targets select the correct board configuration.

Build/link checks do not verify hardware behavior. Ethernet,
USB, external memory, high-speed clock setup and device-specific peripheral
behavior beyond the shared GPIO/USART path remain outside this initial port.
See [BSP architecture](../../../README.md).
