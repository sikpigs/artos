# Buffered system console

DTS `/chosen/artos,console` selects the UART. Generated `board::devices::console`
and `console_config` provide hardware configuration. Startup calls
`artos::console::system_console().init()` once before kernel startup and clients.
The static console registers deferred RX work, traps on registration failure,
configures hardware, installs its ISR and enables the UART IRQ.

`CONFIG_CONSOLE_RX_BUFFER_SIZE` controls RX array bytes (default 256, range
2–65536). The ring has N−1 usable slots, one ISR producer and one thread consumer.
The console owns its buffer and drop/overrun counters. Disabling the monitor does
not remove the console. Current console operation requires `CONFIG_ARTOS=y`.

## Reads and deferred wakeups

`try_read(byte)` is nonblocking. `read_byte()` retries it and blocks if empty:
`block_if_empty()` rechecks the buffer under a kernel critical section before
registering the current thread on `rx_wait_queue` and arranging scheduling.
This closes the condition-check/registration gap.

UART RX stores a byte, publishes the write index and pends the console's deferred
work. It never changes scheduler state. SysTick dispatches the callback before
scheduling; the callback checks for buffered data under protection and wakes one
reader. The reader retries the buffer after resuming. Requests coalesce because
the ring retains the actual bytes. See [kernel contracts](../../kernel/README.md).

TX still polls the transmit-empty flag. Line editing, echo and command parsing
belong to the monitor. Multiple readers and serialized multi-writer output are
not supported. There is no flow control, so input can overflow while the reader
is busy producing output. `rx_dropped_bytes` counts rejected ring-buffer bytes;
`rx_overrun_events` counts hardware overrun events, not exact lost-byte totals.

## Hardware smoke test

With no input, inspect that the monitor is blocked and its waiter points at the
console queue; idle or another thread should execute. Send one character and
check it echoes, then that the monitor blocks again. Paste this modest read-only
burst using CR or CRLF line endings:

```text
08000000.4
08000010.4
08000020.4
08000030.4
08000040.4
08000050.4
08000060.4
08000070.4
```

Check successive dump addresses and compare drop/overrun counters before and
after. Wait for output to drain, then send another command to exercise wakeup
again. Large repeated pastes can exceed capacity because each dump produces more
output than input. Build checks alone do not verify this runtime behavior.
