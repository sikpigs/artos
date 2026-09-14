# Native tests

These tests use the host C++ compiler, independently of the ARM firmware build:

```sh
cmake -S tests/host -B build/host-tests -G Ninja
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Run `build/host-tests/intrusive_list_tests` directly for per-case results.
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

## Python checks

From the repository root, with requirements installed:

```sh
.venv/bin/python -m unittest discover -s tests -p 'test_*.py'
```

This covers configuration generation, DTS bindings, SVD generation, the serial
terminal and staged-tree hooks. DTS tests require `dtc` and `fdtget`; hook tests
use temporary repositories. Firmware compile/link checks are separate; see the
[configuration guide](../../docs/configuration.md#verification).
