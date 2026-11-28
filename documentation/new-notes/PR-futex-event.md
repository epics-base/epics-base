### `epicsEvent` on Linux is now built on a futex

On Linux, `epicsEvent` no longer uses a mutex and a condition variable
but a futex and atomic operations.
Neither `epicsEventTrigger()` nor a waiter that is being woken takes a lock,
so a high-priority thread never has to wait for a preempted
lower-priority thread inside these functions.
A trigger that finds no waiting thread makes no system call,
the kernel wakes the highest-priority waiter first,
and `epicsEventWaitWithTimeout()` now measures timeouts with
`CLOCK_MONOTONIC`, so they are not affected by changes of the system time.

With many threads triggering the same event,
the old implementation suffered from lock contention;
in a test with four triggering and four waiting threads the new one
handled 8 million triggers about 150 times faster.
