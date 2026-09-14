# Kernel wait and deferred work

This is a single-core Cortex-M4 implementation with fixed thread and deferred
registration capacities. It requires no dynamic allocation for these mechanisms.
Timeout/cancellation wake reasons are declared but their completion paths are
not implemented. Sleep deadlines are separate from object waits. Multicore,
thread deletion, mutexes and general multi-object waits remain future work.

## Wait ownership and protection

Each stationary TCB embeds one waiter whose `thread` pointer refers to that TCB.
A console or other waiting object owns the queue. A thread has at most one active
queue registration; the TCB and queue must outlive it. Queue operations only
maintain links and `waiter.queue`; they do not schedule or provide synchronization.

`kernel::locked::block_current(queue)` requires kernel protection already held.
The caller must check its condition under that same protection. It clears the old
wake result, links the waiter, marks the thread blocked, selects another runnable
thread and requests PendSV. Restoring interrupts permits the context switch.
Call only from a running non-idle thread, with no outer mask or scheduler lock
preventing the handoff; these preconditions are not all runtime-enforced.

`kernel::locked::wake_one(queue)` removes the FIFO front waiter, records `signaled`,
and makes its thread ready. Empty queues return false. It does not itself request
scheduling. The current console caller runs in the deferred pass, followed by
`sched()` in the tick handler. New callers must provide their scheduling decision.
A wake is notification, not resource ownership; readers retry their condition.

## Deferred work

`<deferred.h>` exposes `register_deferred(callback, context)` and
`pend_deferred(handle)`. The table capacity is
`build_config::artos::max_deferred_work_items`. Null callbacks or a full table
return `invalid_deferred_handle`; clients must check before enabling producers.
Pending uses assertions for handle validity, so invalid handles are not a
recoverable error contract in Release builds.

Register once before dispatch/producers can access the slot. Registration is not
synchronized against concurrent registration or dispatch. There is no unregister
or slot reuse, and callback contexts must remain alive for the system lifetime.
Kernel initialization currently does not clear deferred registrations: the console
registers before `kernel::init()` starts SysTick.

Pending stores `true` with release ordering. `serve_deferred()` visits each slot
once, exchanges pending to false with acquire ordering, and invokes the callback
if requested. Repeated requests coalesce; event payloads must live in a separate
buffer or counter. A request arriving after the exchange remains pending. If its
slot has already been visited, it is handled on a later tick.

Callbacks execute in SysTick interrupt context and must be bounded and nonblocking.
The tick handler advances time, services deferred work, then schedules. There is
no dedicated bottom-half IRQ yet; nominal notification latency is up to one tick,
plus masking, callback and scheduling delays.

## Interrupt contract

Thread-side critical sections save/disable/restore PRIMASK. SysTick and PendSV
share preemption priority and do not interrupt each other. Higher-priority UART
interrupts may capture bytes and pend atomic deferred flags, but must not modify
scheduler or wait-queue state. The console deferred callback uses a short critical
section around condition inspection and wake completion. This retains fast UART
capture during the scheduler scan. NMI/fault handlers do not participate in normal
wait operations. Local interrupt masking does not provide multicore exclusion.
