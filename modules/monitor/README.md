# Serial monitor module

`monitor::run()` edits command lines and processes memory commands using the
system console initialized by startup. It owns no UART ISR or RX buffer. Input
uses blocking `read_byte()`; output still polls through the console.

`CONFIG_MONITOR` depends on `CONFIG_CONSOLE`. The console depends on
ARTOS, enforced by Kconfig; see [configuration guidance](../../docs/configuration.md).
Disabling the monitor leaves the console available to other clients. The monitor
is the sole RX consumer and uses a 4096-byte thread stack in the application.

## Commands

All numbers are hexadecimal without a `0x` prefix. Addresses must be aligned to
four bytes; counts are numbers of 32-bit words, not bytes or end addresses.

| Input | Operation |
| --- | --- |
| `08000000` | Read one word |
| `08000000.4` | Read four words |
| `address: value value ...` | Write successive 32-bit words |
| `snapshot` | Measure 32 thread-snapshot calls with the hardware stopwatch |

The `snapshot` command reports the number of registered threads, min/average/max
call cycles, maximum microseconds, and maximum as a percentage of one kernel tick.
The latter two values round up to avoid understating latency.
Enable `CONFIG_ARTOS_THREAD_STATS=y` to use it. The reported call time includes
timer reads and any higher-priority interrupt that preempts the call, so it is a
conservative bound on the kernel-interrupt masked time. Output is sent only
after all samples. The measurement uses the currently registered thread count
and configured history size; repeat it with the expected production load.

The monitor accepts CR or CRLF line endings; LF alone is ignored. Backspace and
Delete erase input. Lines longer than 128 characters are discarded through CR.
A backslash prompt follows an empty or invalid command. There is no run command.

Memory reads/writes are direct and volatile. The parser checks syntax, alignment
and address-space overflow, not whether a region is mapped or writable. Invalid
addresses can fault; peripheral reads can have side effects. Use the flash-read
examples for RX testing rather than writing to arbitrary RAM.

See [console buffering and smoke tests](../console/README.md). The current C++ monitor is independently written and covered by the root
[MIT license](../../LICENSE). The earlier ARM adaptation of the Woz monitor was
removed; this license statement does not relicense that historical implementation.
