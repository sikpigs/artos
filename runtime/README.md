# C/C++ runtime support

These freestanding runtime hooks are linked into every firmware build, including
builds with ARTOS disabled:

- `init.cpp` implements `__libc_init_array`, called by architecture reset code to
  run preinitializers, the optional `_init` hook, and global constructors.
- `abort.cpp` implements C `abort()` through the CPU fatal-trap interface.
- `syscalls.cpp` supplies placeholder newlib file/console hooks (`_close`,
  `_fstat`, `_isatty`, `_lseek`, `_read`, and `_write`). They report failure with
  `errno = ENOSYS`; `_isatty` returns false. Failed assertions still call
  `abort()`, but libc diagnostics have no output backend yet.
- `cxa_guard.cpp` implements the ARM C++ ABI guards for function-local statics.
  It uses a 32-bit guard and masks interrupts throughout initialization on the
  single core. Constructors must not block or yield.

CPU instructions and exception entry code belong in `arch/`; these files provide
language and library entry points using `cpu_port.h` where needed. The guard
implementation assumes the ARM ABI and must be reviewed before adding another
architecture.
