### New lock-free LIFO `epicsLifo.h`

The new header-only `epicsLifo.h` provides an intrusive singly linked LIFO
modelled on the Linux kernel's `llist`.
Any number of threads (or ISRs on vxWorks and RTEMS) can push nodes
with `epicsLifoPush()` or `epicsLifoPushBatch()`, which are lock-free,
while consumers detach the whole list with the wait-free `epicsLifoTakeAll()`.
There is deliberately no single-node pop, so the list is not subject to
the ABA problem and nodes can be reused immediately.
The push functions report whether the list was empty,
so producers know when to wake a consumer.
