# Development tooling

## Routine checks

Configure and build firmware, run compile/generation checks, and run the native
and Python tests through the project driver:

```sh
./artos.py build
./artos.py check
./artos.py test
```

The `debug` preset is the default. Put `--preset release` or
`--preset min-size` before the command to select another build mode.
CTest presets named `all`, `native`, `python`, and `hooks` provide filtered test
runs after configuring and building the `host-tests` preset. Matching VS Code
test tasks configure and build automatically.

## Pre-commit hook

Install clang-format and clang-tidy, then enable the checked-in hook once per
clone:

```sh
sudo apt install clang-format clang-tidy
git config core.hooksPath .githooks
```

The formatter script intentionally does not call `git clang-format`.
`git clang-format` operates on changed line ranges and is primarily a rewriting
tool. The hook instead reads complete files and style configuration from the Git
index, sends those bytes directly to clang-format, and prints a patch when the
result differs. This checks exactly what will be committed, including partially
staged files, without changing the worktree or index.

After formatting passes, clang-tidy analyzes an isolated snapshot of the index.
Changed translation units are checked directly. A staged header or `.clang-tidy`
change checks every translation unit covered by the available compilation
databases because the affected include graph cannot be determined reliably from
the index alone. Diagnostics are promoted to errors for commits; neither fixes
nor source changes are applied.

Configure every build whose sources the hook needs to cover. The active root
`compile_commands.json` is preferred, then databases under `build/*` fill gaps.
For example:

```sh
./artos.py build
./artos.py test
```

The first command covers firmware; the second configures `build/host-tests`.
Generated build files are copied into the isolated snapshot so analysis sees the
same generated headers as compilation. A source file must have a command in at
least one selected database.

The hook runs up to four clang-tidy processes concurrently. This primarily
speeds up header and configuration changes; single-source commits still launch
one process. Set `CLANG_TIDY_JOBS=1` to serialize analysis on a memory-constrained
machine, or choose another positive value. Other overrides are:

- `CLANG_FORMAT` and `CLANG_TIDY`: tool executable names or paths.
- `CLANG_TIDY_BUILD_DIR`: use one build directory.
- `CLANG_TIDY_BUILD_DIRS`: ordered, path-separator-delimited build directories,
  such as `build/debug:build/host-tests` on Linux.

GNU Arm system include paths are discovered from the compiler command. The
analysis snapshot removes `-mgeneral-regs-only`, which Clang 18 does not accept;
the real firmware build retains it.

Run the hook integration tests through CTest with:

```sh
ctest --test-dir build/host-tests -L hooks --output-on-failure
```

## Manual clang-tidy use

The root `.clang-tidy` keeps manual diagnostics advisory. The pre-commit hook
adds `--warnings-as-errors=*`. A host translation unit can be inspected directly
after the test build has been configured:

```sh
./artos.py test
clang-tidy -p build/host-tests tests/intrusive_list.cpp
```

The hook is the recommended path for firmware sources because it also supplies
the GNU Arm C++ and newlib include paths that Clang needs. Clangd uses the same
configuration while editing, but does not run every standalone clang-tidy check.
