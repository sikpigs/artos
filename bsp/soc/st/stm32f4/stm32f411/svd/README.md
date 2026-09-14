# STM32F411 peripheral definitions

Unmodified `CMSIS/SVD/STM32F411.svd` from
[Keil STM32F4xx_DFP 3.1.1](https://www.keil.com/pack/Keil.STM32F4xx_DFP.3.1.1.pack).
SVD device version: 1.9. Copyright (c) 2024 STMicroelectronics.
Licensed under Apache-2.0; see `LICENSE-Apache-2.0` and the SVD header.
This third-party file is excluded from the project's MIT license.

SHA-256: `9e1df0b4dd3f0da0bd02ccd2eefec58ef599bae29facc8fa5e9b48c748ff3717`

Both VS Code debug configurations use this local copy, including offline.

## Register generation

This SVD belongs to the SoC and can serve multiple boards. See [BSP architecture](../../../../../README.md).

From the repository root:

```sh
python3 scripts/svd_to_regmap.py --soc stm32f411
python3 scripts/svd_to_regmap.py --soc stm32f411 --check
cmake --build --preset debug --target regmap-checks
```

The script uses Python's standard library. It generates `include/stm32f411/regmap.h`
as an umbrella and one header per inherited SVD `groupName` in
`include/stm32f411/generated/`. All 46 retained SVD peripherals are emitted in
19 groups. TIM8 is explicitly excluded because RM0383 states it is not present
on STM32F411. The vendor SVD remains unmodified.

| Group headers | Layout organization |
| --- | --- |
| `rcc.h`, `gpio.h`, `usart.h`, `i2c.h`, `spi.h`, `dma.h` | Common layout shared across instances after comparing every register and field |
| `crc.h`, `dbg.h`, `exti.h`, `flash.h`, `iwdg.h`, `pwr.h`, `rtc.h`, `sdio.h`, `syscfg.h`, `wwdg.h` | Single-peripheral layout |
| `adc.h`, `tim.h`, `usb_otg_fs.h` | Separate peripheral namespaces and types within the group |

For example:

```cpp
#include <stm32f411/generated/tim.h>

// TIM2 and TIM10 have different layouts and field widths.
stm32f411::tim::TIM2->CCR1.write(0x12345678);
using timer2_registers = stm32f411::tim::tim2::registers_t;
using timer10_registers = stm32f411::tim::tim10::registers_t;
```

`regmap.h` includes every group. Drivers can include just the group they need.
SVD names are preserved for register members, and lowercased for namespaces and
fields. C++ keywords get a trailing underscore: TIM2's `OR` member has field
namespace `tim::tim2::or_`. The SVD `DBG` group is named `dbg`, not the old
handwritten `dbgmcu`. Timer input/output CCMR views and USB host/device views
are anonymous unions at their declared offsets, with distinct field tags.
USB endpoint 0 retains its own control/transfer-size fields.

Ordinary firmware builds check freshness without rewriting source files. The
`regenerate-regmap` CMake target explicitly regenerates the complete header set.
`--soc` chooses the SoC profile and namespace. `--soc-dir` accepts an explicit
SoC directory; `--svd` and `--profile` accept alternative input files. `--output` is now a
**directory** containing `regmap.h` and `generated/`, not a single output file.
Defaults do not depend on the working directory. Regeneration removes obsolete
group headers only when they carry this generator's ownership marker; `--check`
reports those files and never writes anything. The old `generated/rcc_gpio.h`
is replaced by `generated/rcc.h` and `generated/gpio.h`.

### Corrections and compatibility

`regmap.json` pins the input SHA-256 and records corrections with sources:
RCC DCKCFGR/TIMPRE, SPI4/SPI5 reset bits, SPI5 enable/low-power enable bits,
PLLI2SM, LSEMOD, I2C1/2/3 FLTR, and USB GRXSTSP read-and-pop views. Additions fail
if their target already exists, so a vendor update requires review. The separate
`aliases` section preserves existing RCC and USART driver names and combines
split fields after checking that they are contiguous and have matching access.
TIM2/TIM5 compare aliases cover all 32 bits. Generated definitions retain the
source's ST copyright and Apache-2.0 attribution.

GPIO pin-field templates remain handwritten in `gpio.h`, with compile-time checks
against every corresponding generated field. Including only `regmap.h` exposes
raw fields such as `gpio::moder::moder5`; include `gpio.h` for
`gpio::moder::mode<5>`. Existing GPIO and USART driver APIs remain available.
Other former handwritten convenience layouts are replaced by the SVD names:
DMA streams are explicit `S0CR`, `S1CR`, etc.; RTC backup registers are individual
members; USB blocks and endpoints use the explicit names in the SVD. There is no
longer a universal timer register type exposing unsupported registers.

This is complete coverage of the retained SVD registers plus the listed
corrections, not a claim that the SVD describes every hardware feature correctly.
Declarations formerly hand-added without SVD backing are not automatically copied
into the generated output (for example USB FIFO access windows). Additional
hardware definitions should enter through a reviewed correction or a handwritten
adapter, with a reference-manual source. Interrupt dispatch and IRQ enumeration
remain in the existing BSP IRQ API; this generator handles register declarations.

### Supported input and limits

Supported input is flat, aligned 32-bit registers, bitOffset/bitWidth fields,
inherited device/peripheral/register properties, whole-peripheral `derivedFrom`,
and explicit `alternateRegister` views sharing the same offset. Register/field
inheritance, partial derived overrides, arrays, clusters, other widths,
unexplained overlaps, and special read/write semantics are rejected. Group names
are inherited from the base peripheral. Reset values and masks remain per
instance, e.g. `gpio::metadata::gpioa::moder::reset_value`; for a group with
separate types, e.g. `tim::tim2::metadata::tim2::cr1::reset_value`.

Read-only/write-only register access selects the corresponding MMIO wrapper.
When register access is absent, fully specified field access determines whether
the register can be read, written, or both. Field access is retained in generated
comments but is **not enforced by `field_t`**. No claim of safe read-modify-write
semantics is made for status/command fields. In particular, GRXSTSP reads consume
queue entries. Reset metadata is inherited from the SVD unless corrected, not
independently audited hardware defaults. Unprefixed numbers, including those with
leading zeros, are interpreted as decimal. Field-level access and side-effect
policies remain a separate MMIO follow-up.
