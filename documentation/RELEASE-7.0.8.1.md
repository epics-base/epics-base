## EPICS Release 7.0.8.1

### Limit to `_FORTIFY_SOURCE=2`

GCC versions 12 and beyond and glibc have added some aggressive runtime
checks for buffer overflows in libc functions at runtime, and the
[Ubuntu 2024.04](https://wiki.ubuntu.com/ToolChain/CompilerFlags) release
increased their default gcc fortification level from 2 to 3.
This has started causing EPICS Base builds to fail on that version, and
other OS releases may make that configuration change with similar results.
This release detects a compiler configured with `_FORTIFY_SOURCE=3` and
overrides it to 2.
Later releases of Base will adjust the code, providing information to the
compiler to avoid triggering these incorrect protections.

### Fix issue with compress record

In Base 7.0.8, an update to the compress record was added to allow for certain
algorithms to use partially filled buffers in their computations. Unfortunately,
this broke the behaviour of the records in certain cases. This has been fixed.

### Various minor changes

These included fixing minor memory leaks and documentation corrections. The
`SIZV` field of lsi, lso and printf record VAL fields now can't exceed 32767
characters, to match an internal limit.

### `epicsSocketAccept()` now returns `SOCKET`, not `int`

This might have some effect on downstream modules still using `int`, but the
OS-specific osdSock.h headers which osiSock.h includes have all declared
`SOCKET` (in most casese as a typedef for `int`) for many releases.
This change removes a compiler warning on WIN32.
Further details and the discussion about this change can be found
[here](https://github.com/epics-base/epics-base/pull/458).

### `dbLoadRecords` allows macros with default values

Previously the parser assumed that files containing macro substitutions were
bad if no macro definitions were provided; that assumption was made incorrect
once macro substitutions were allowed to provide a default value.

### Hostname length limit in CA removed

Before this release, the CA client library only handled hostnames in address
list environment variables up to 255 characters long.
This limit has been removed.
