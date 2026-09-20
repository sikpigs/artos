# Test suite

The test build registers native C++ executables and Python integration tests with
CTest. It uses the host compiler independently of the ARM firmware build. From
the repository root, run:

```sh
./artos.py test
```

The same configure, build, and test stages are available as presets:

```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset all
ctest --preset native
ctest --preset python
ctest --preset hooks
```

In VS Code, first run **CMake: Select Configure Preset** and choose **Host Tests**.
CMake Tools hides test presets that do not belong to the active configure preset;
after selecting Host Tests, **CMake: Select Test Preset** lists **All Tests**,
**Native C++ Tests**, **Python Tests**, and **Pre-commit Hook Tests**. Alternatively,
use **Tasks: Run Test Task** and select the corresponding **Tests:** task; those
tasks configure and build Host Tests automatically without changing the active
CMake Tools preset.

Run `ctest --test-dir build/host-tests --show-only` to list tests, use `-R` to
select by name, or use `-L native`, `-L python`, or `-L hooks` to select a label.
`build/host-tests/tests/host/intrusive_list_tests` can also be run directly for
per-case results.
The checks remain active in Release builds and return failure for incorrect
behavior; current implementation bugs are not marked as expected passes.

The list contract under test assumes `insert(location, node)` inserts **before**
location, added nodes are detached, erased nodes belong to the list, and nodes
outlive their membership. Inserting an already-linked node, erasing a foreign
node, copying a live list, and concurrent mutation are outside these tests.

The push tests start from an empty list and use only public operations. To test
erase and insert with a controlled initial layout, a fixture establishes the
endpoints through `push_back` and seeds the public hook links for three nodes.
Tests check order, both link directions, termination, removal, reuse and transfer
between lists. The append-after-insert case also exercises the private tail's
observable behavior. Traversals are bounded so a cycle cannot hang the suite.

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

All four registered native tests (intrusive list, fallback, automatic selection,
and standard reference) passed during the documentation review. Checks remain
active with `NDEBUG`. These tests do not exercise scheduler or IRQ interleavings.

The Python CTest entries cover configuration generation, DTS bindings, SVD
generation, the serial terminal, and staged-tree hooks. DTS tests require `dtc`
and `fdtget`; hook tests use temporary repositories. Firmware compile/link checks
remain separate; see the
[configuration guide](../../docs/configuration.md#verification).
