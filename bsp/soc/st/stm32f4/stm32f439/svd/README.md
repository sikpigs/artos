# STM32F439 peripheral definitions

Unmodified `CMSIS/SVD/STM32F439.svd` from
[Keil STM32F4xx_DFP 3.1.1](https://www.keil.com/pack/Keil.STM32F4xx_DFP.3.1.1.pack).
SVD device version 1.3, Copyright (c) 2024 STMicroelectronics. Apache-2.0;
see `LICENSE-Apache-2.0`. The vendor file and generated definitions retain their
third-party license and are outside the project's MIT grant.

SHA-256: `c3de4e42c9fa2e0196f364efa25d163e4a9813b6c6b2fd0f5a8ffb64158851c9`.

`regmap.json` pins this SVD and selects namespace `stm32f439`. It currently has
no correction patches or excluded peripherals: all 84 SVD peripheral entries
are generated. This is source coverage, not a complete reference-manual audit.
F411-specific corrections are deliberately not applied to this chip.

```sh
python3 scripts/svd_to_regmap.py --soc stm32f439
python3 scripts/svd_to_regmap.py --soc stm32f439 --check
```

Output is `bsp/soc/st/stm32f4/stm32f439/include/stm32f439/`, grouped by SVD groupName.
UART/USART variants retain separate namespaces where their layouts differ;
the board binds the shared driver to `stm32f439::usart::usart3::registers_t`.
See the [F411 generator notes](../../stm32f411/svd/README.md) for supported input
constructs and the current MMIO field-access/side-effect limitations.
