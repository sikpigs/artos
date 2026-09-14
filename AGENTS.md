# Project conventions

- Prefer angle brackets for C and C++ includes, including project headers:
  `#include <artos.h>` rather than `#include "artos.h"`.
- When changing an include to angle brackets, ensure its path resolves through
  the target's include directories. Keep kernel-private include paths private;
  do not expose internal headers through public interfaces to satisfy this rule.
- Use snake_case for data members without leading or trailing underscores.
  For example, use `rx_buffer`, not `rx_buffer_` or `_rx_buffer`.
- Preserve hardware register names in MMIO layouts (for example, `CTRL`). Use
  narrowly scoped `NOLINTBEGIN/END(readability-identifier-naming)` annotations
  around handwritten register layouts instead of weakening general member rules.
  Update generators rather than manually editing generated register headers.
- Re-read relevant files before proposing or making changes; the user may be
  editing them between turns.

The `.clangd` `Style.AngledHeaders` setting controls newly inserted includes.
Clang-format 18 does not convert include delimiters, and the current clang-tidy
configuration does not enforce this preference. Follow it when writing code;
do not perform unrelated bulk include rewrites.
