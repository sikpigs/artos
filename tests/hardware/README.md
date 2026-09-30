# Thread stack canary hardware tests

`test_stack_canary.py` flashes a canary-enabled Debug ELF, runs it twice under
OpenOCD/GDB, and tests both context-switch checks. It changes one guard word
through GDB while the CPU is halted, then verifies that firmware enters
`fatal_trap()` and records the expected reason, phase, thread ID, TCB address,
and registry slot. It also checks that the guard and diagnostic record are
clean after reset. Each test resets and resumes the firmware when it passes.

Connect the board through its ST-LINK USB connector. Nothing else may be using
its OpenOCD interface. The test overwrites the board's current firmware.

From the repository root, prepare an isolated F411 Debug build:

```sh
cp configs/nucleo_f411re_defconfig /tmp/artos_canary_defconfig
printf '\nCONFIG_ARTOS_THREAD_STACK_CANARY=y\n' >> /tmp/artos_canary_defconfig
cmake -S . -B build/canary-hw -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DKCONFIG_DEFCONFIG=/tmp/artos_canary_defconfig
cmake --build build/canary-hw
python3 tests/hardware/test_stack_canary.py \
  --board nucleo_f411re --elf build/canary-hw/artos.elf
```

For an F439, use `configs/nucleo_f439zi_defconfig`, a separate build directory,
and `--board nucleo_f439zi`. The runner requires `openocd`, `gdb-multiarch`,
and `arm-none-eabi-nm` on `PATH`; use `--openocd`, `--gdb`, or `--nm` to specify
other executable paths. It refuses an ELF without the canary record or with
a stack top that does not match the selected board.

This is a deliberate guard-corruption test. It verifies detection and the
postmortem record, without forcing a real stack overflow into adjacent data.
