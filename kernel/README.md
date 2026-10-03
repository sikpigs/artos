# Kernel wait and deferred work

This is a single-core Cortex-M4 implementation with fixed thread and deferred
registration capacities. Wait queues and deferred registration need no dynamic
allocation; the overload of `thread::create` without caller-provided storage
allocates its TCB and stack. Timed object waits and thread sleeps share the
scheduler's deadline queue. Queue closure uses the `cancelled` wake reason.
Threads can exit and are reaped by the idle thread; terminating
another thread, mutexes and general multi-object waits are not implemented.

## Wait ownership and protection

Each stationary TCB embeds one waiter whose `thread` pointer refers to that TCB.
A console, queue or other waiting object owns its wait queue. A thread has at
most one active object-wait registration; the TCB and queue must outlive it.
Wait-queue operations only maintain links and `waiter.queue`; they do not
schedule or provide synchronization.

`kernel::locked::block_current(queue, deadline)` requires kernel protection
already held. The caller must check its condition under that same protection.
The overloads taking a `critical_section_t` transfer that protection into the
blocking call and return a `wake_reason_t`. Blocking clears the old wake result,
links the waiter, marks the thread blocked and requests scheduling. A timed
wait also joins the deadline queue. Releasing the critical section permits the
context switch. Call only from a running non-idle thread, with no outer mask or
scheduler lock preventing the handoff; these preconditions are not all runtime
enforced.

On timeout, the scheduler removes the waiter from both queues, records
`timeout`, and makes the thread ready. A signal removes any deadline entry,
records `signaled`, and makes the thread ready. The `immediately` deadline
returns `timeout` without registering a waiter. `<queue.h>` uses these timed
waits for bounded push and pop operations. Queue operations return
`queue_result_t::success`, `timeout`, or `closed`; a failed pop leaves its
output unchanged.

`kernel::locked::wake_one(queue)` chooses the highest-priority waiter, retaining
queue order among equal priorities. `wake_all(queue, reason)` wakes every waiter
with the requested reason (defaulting to `signaled`).
Both return whether a thread with higher priority than the current thread was
woken; an empty queue returns false. Neither call requests scheduling. The
console's deferred callback relies on the subsequent `sched()` call in the tick
handler. Other callers must provide their own scheduling decision. A wake is
notification, not resource ownership; readers retry their condition.

## Queue closure and lifetime

`queue_t::close()` permanently marks the queue closed under kernel protection,
then cancels all blocked readers and writers. It is idempotent and does not wait
for those calls to return. Every later push or pop reports `closed`, including
pops when buffered items remain; close discards access to those items. A wait
that is cancelled by close reports `closed`, whereas an elapsed deadline reports
`timeout`.

`queue_t::close_and_wait()` calls `close()` and waits until all operations that
entered while the queue was open have reached their exit path. It uses an active
operation count and a wait queue; it must run in a normal non-idle thread when
other operations are active. No semaphore or fixed participant count is needed.
Do not call it from a queue operation or while holding a mask or scheduler lock
that prevents the other threads from running.

A queue cannot detect a thread that merely holds a pointer or reference and
will call it later. The owner must first stop new users, then call
`close_and_wait()` and join or otherwise receive completion from threads that
may still hold such references before destroying the queue and its storage.
The count covers calls inside the queue, not the caller's work after a method
returns. Destruction does not implicitly close or wait.

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

Kernel critical sections save/raise/restore BASEPRI; the global interrupt
guard uses PRIMASK. SysTick and PendSV share preemption priority and do not
interrupt each other. Higher-priority UART interrupts may capture bytes and
pend atomic deferred flags, but must not modify
scheduler or wait-queue state. The console deferred callback uses a short critical
section around condition inspection and wake completion. This retains fast UART
capture during the scheduler scan. NMI/fault handlers do not participate in normal
wait operations. Local interrupt masking does not provide multicore exclusion.
