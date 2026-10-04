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

## Mutex tests

`mutex` and `mutex_aging` run the same scenarios with priority aging disabled
and enabled. They cover acquisition, owner-only release, scheduling after release,
timed and indefinite contention, deadline rollover and retries, priority
inheritance and restoration, several owned mutexes, and dependency chains.
Regression cases check donation removal through a chain and preservation of
live donations when an owning thread's own wait times out. `mutex_aging` also
checks both orderings: aging before donation, and donation before aging. The
latter checks restoration after aging resets and the donation ends through
either release or timeout.
Additional regressions check that blocked, sleeping, and finished threads do
not age, that scheduling preserves the donation from a mutex waiter, and that
a new mutex owner immediately inherits from any remaining waiters.
They also specify desired runtime rejection of ISR use, self-locking, and a
cycle between two mutexes; the deadlock checks require an explicit error result
rather than allowing the thread to block.

```sh
ctest --test-dir build/host-tests -R '^mutex(_aging)?$' --output-on-failure
```

The tests link the production mutex, blocking, wait queue, deadline scanner, and
scheduler code. The host port simulates a context switch when a blocking call
releases its interrupt mask, then runs each scenario's other-thread actions.
Each scenario runs in a separate child process so a failed assertion cannot
hide the remaining results. Assertions and signals count as test failures.
These tests do not exercise ARM exclusive instructions, actual exception entry,
or assembly context switching; those require hardware validation.
The Kconfig suite also compiles ARM TCB layout checks with aging enabled and
disabled, four thread name lengths, and optional TCB extensions enabled and
disabled. Native builds also enforce their own TCB size assertions. ARM checks
skip if `arm-none-eabi-g++` is unavailable.

## Semaphore tests

`semaphore` and `semaphore_aging` link the production semaphore, blocking,
deadline, wait queue, and scheduler implementations. The host port simulates
context switches and an intervening close at a critical-section boundary.
`initial_count` denotes permits already in use: acquire increments occupancy,
release decrements it, and try_acquire returns full at maximum occupancy.
Releasing an unused semaphore is tested as a successful no-op.

Cases cover initial occupancy, capacity limits, zero/timed/indefinite waits,
ISR acquisition and release, accumulated wake flags, deadline rollover and
barging retries, lack of priority donation, close rejection and cancellation,
and close racing with acquisition. Additional regressions check atomic permit
acquisition and scheduling inside the release critical section. Invalid constructor counts are expected to
assert; other assertions and signals count as failures. Each case runs in an
isolated child process. These tests do not exercise ARM exception entry or
arbitrary instruction-level preemption. Separate host execution stacks exercise
close_and_wait and deferred close_and_delete with one or multiple active
acquisitions, including timed and indefinite waiters. Deferred deletion must
occur inside the final acquisition's guard destructor; the worker has not yet
returned from acquire at that point. Deletion is also tested when idle, already
closed with an active acquisition, and requested at the final operation's
unmask boundary. A watched allocation is quarantined until the scenario
finishes, allowing deterministic checks for premature or repeated deallocation
and continued kernel access after deletion. The default trivial destructor does
not let these runtime tests observe an explicit extra destructor call.

`python.semaphore_api` separately compiles calls to the public release and lifecycle APIs.

```sh
ctest --test-dir build/host-tests -R '^(semaphore(_aging)?|python.semaphore_api)$' --output-on-failure
```

## Queue deletion lifecycle

`queue_lifecycle` and `queue_lifecycle_aging` prepare the contract for deferred
`queue_t::close_and_delete()`. They link the production queue, blocking, wait
queue, and scheduler code, with separate host stacks for active operations.

The contract is: an idle queue deletes immediately; an active queue closes and
returns without waiting, then its last operation guard destroys and deallocates
it exactly once. Deletion also works after plain close. Queue storage belongs
to the caller and remains intact and reusable after deletion. Callers must stop
new users and arrange exclusive responsibility for destruction; these tests do
not combine a deletion request with concurrent close_and_wait callers.

The 26 cases cover ordinary operations without deletion, active close_and_wait,
empty/full and already-closed deletion, all four push/pop ends with timed and
indefinite waits, multiple readers/writers, a signaled reader alongside a blocked
writer, derived destruction through the virtual core destructor, and deletion
at each operation's final unmask boundary. The watched allocation is quarantined
until the scenario ends, allowing deterministic deallocation checks. This does
not simulate ARM exception entry or arbitrary instruction-level preemption.

Until the public queue_t wrapper exists, five baseline cases run and 21 deletion
cases explicitly report PENDING. The executable returns 77 and CTest marks the
suite skipped, unless a baseline case fails. Once the public API exists, all
cases activate automatically; its core implementation must then link normally.

```sh
cmake --build build/host-tests --target queue_lifecycle_tests queue_lifecycle_aging_tests
ctest --test-dir build/host-tests -R '^queue_lifecycle(_aging)?$' -V
```

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
