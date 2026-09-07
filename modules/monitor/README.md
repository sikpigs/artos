# Serial monitor module

Optional USART console and ARM Thumb adaptation of the Woz monitor.

- `include/monitor/monitor.h`: public `monitor::run()` entry point.
- `src/monitor.cpp`: console driver integration, receive buffer, workspace, callbacks.
- `src/wozmon.s`: command parsing, memory access, and program execution.
- `Kconfig`: enable switch, receive-buffer size, and workspace reservation.
- `CMakeLists.txt`: `monitor::monitor` target.
- `PROVENANCE.md`: source ancestry and unresolved licensing status.

The application includes `<monitor/monitor.h>` and calls `monitor::run()` after
`board::init()`. The module initializes the BSP console through
`board::init_console()` and uses `board::devices::console`. This adapter currently
depends on the STM32 USART driver; the module is not yet board-independent.
It does not depend on ARTOS.

CMake adds this directory only when `CONFIG_MONITOR=y`. Its interface target
supplies sources directly to the firmware, inheriting the firmware's compilation
settings; it is not a separately installed or precompiled library. Disabled builds
receive neither its sources nor its public include path.

The module places its workspace in `.monitor_workspace`. The BSP linker template
owns physical RAM placement and reserves `CONFIG_MONITOR_WORKSPACE_SIZE` bytes
(default 4096, minimum 160). The receive buffer is separate. Disabling the module
sets both sizes to zero, leaving no monitor reservation. The generated configuration
is supplied by the root Kconfig integration.

Moving these files does not change their licensing status. See the provenance
notes before including this module under any project-wide license declaration.
