### epicsAtomic: exchange and loads/stores with explicit memory ordering

The `epicsAtomic.h` API has new functions for `int`, `size_t` and pointer
targets:

- `epicsAtomicExchange{IntT,SizeT,PtrT}()` atomically replace a value
  and return the previous one (a full barrier, like the other
  read-modify-write operations).
- `epicsAtomicGet{IntT,SizeT,PtrT}Relaxed()` and
  `epicsAtomicSet{IntT,SizeT,PtrT}Relaxed()` are atomic accesses
  that don't order any other memory accesses.
- `epicsAtomicGet{IntT,SizeT,PtrT}Acquire()` and
  `epicsAtomicSet{IntT,SizeT,PtrT}Release()` provide C11/C++11 acquire
  and release semantics, the usual way to publish data to another thread.

The C++ API in namespace `epics::atomic` has the overloads `exchange()`,
`getRelaxed()`, `getAcquire()`, `setRelaxed()` and `setRelease()`.

With GCC 4.7 or later and with Clang these use the `__atomic` builtins,
so on x86 all loads and stores compile to plain moves,
and ARM and POWER need only lightweight barriers or none at all.
With MSVC on x86 and x64 they compile to plain moves with compiler barriers.
Other targets use volatile accesses with the existing read and write barriers.
