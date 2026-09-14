# F439 vector ordering

Derived from STMicroelectronics `cmsis-device-f4/Source/Templates/gcc/startup_stm32f439xx.s`:
[upstream source](https://github.com/STMicroelectronics/cmsis-device-f4/blob/master/Source/Templates/gcc/startup_stm32f439xx.s).
Retrieved 2026-09-12. Copyright (c) 2017 STMicroelectronics, Apache-2.0;
see `LICENSE-Apache-2.0`. The order/names are retained; reset handling is shared
in `arch/arm/cortex_m4/src/reset.s` and weak external handlers branch to the shared
dispatcher. This third-party-derived file is outside the project's MIT grant.

Upstream source SHA-256: `b7939c39334f1ebbf51fa2580613db4f246de069d1d9d0bc7898d7ddac1b3e01`.
