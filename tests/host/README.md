# Test suite

The test build registers native C++ executables and Python integration tests with
CTest. It uses the host compiler independently of the ARM firmware build. From
the repository root, run:

```sh
./artos.py test
```

The same stages are available through CMake and CTest presets:

```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset all
```

Run `ctest --list-presets` for the current filtered test presets. In VS Code,
select the **Host Tests** configure preset before choosing a test preset, or use
a **Tests:** task to configure, build, and run tests without changing the active
CMake Tools preset.

Run `ctest --test-dir build/host-tests --show-only` to list tests, use `-R` to
select by name, or use `-L native`, `-L python`, or `-L hooks` to select a label.
`build/host-tests/tests/host/intrusive_list_tests` can also be run directly for
per-case results.

The list contract under test assumes `insert(location, node)` inserts **before**
location, added nodes are detached, erased nodes belong to the list, and nodes
outlive their membership. Inserting an already-linked node, erasing a foreign
node, copying a live list, and concurrent mutation are outside these tests.

## Expected compatibility implementation

`include/expected.h` implements an allocation-free `artos::expected<T, E>` for
integral/enum payloads, using a union and a boolean discriminator. Default
construction is success with `T{}`. `unexpected` distinguishes error construction
when T and E are the same type. Callers must check state before dereferencing or
calling `error()`; throwing `value()`, `void`, owning payloads and monadic operations
are outside this implementation's contract.

Firmware consumers include `<expected_compat.h>`, which selects standard types
when `__cpp_lib_expected >= 202202L` after conditionally including `<expected>`,
and the local fallback otherwise. Nothing is added to namespace `std`.

- `expected_tests` always exercises the fallback.
- `expected_auto_tests` exercises automatic selection.
- `expected_reference_tests` is available when the host supplies `std::expected`.

```sh
ctest --test-dir build/host-tests -R '^expected' --output-on-failure
```

The Python CTest entries cover configuration generation, DTS bindings, SVD
generation, the serial terminal, and staged-tree hooks. DTS tests require `dtc`
and `fdtget`; hook tests use temporary repositories. Firmware compile/link checks
remain separate; see the
[configuration guide](../../docs/configuration.md#verification).
