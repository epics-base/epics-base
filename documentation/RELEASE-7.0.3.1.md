# Release Notes

This document describes changes that were included in the release of
EPICS listed below.
Notes from earlier EPICS releases are now provided in a separate document for
each version in the EPICS 7 series to date.
Release documents are also included for the older Base 3.15 and 3.16 series.

## EPICS Release 7.0.3.1

**IMPORTANT NOTE:** *Some record types in this release will not be compatible
with device support binaries compiled against earlier versions of those record
types, because importing the record documentation from the EPICS Wiki
[as described below](#imported-record-reference-documentation-from-wiki)
also modified the order of some of the fields in the record definitions.*
As long as all support modules and IOCs are rebuilt from source after updating
them to use this release of EPICS Base, these changes should not have any
affect.


### logClient reliability

On supported targets (Linux, Mac, Windows) logClient will attempt to avoid dropping
undelivered log messages when the connection to the log server is closed/reset.

### Timers and delays use monotonic clock

Many internal timers and delay calculations use a monotonic clock
epicsTimeGetMonotonic() instead of the realtime epicsTimeGetCurrent(). This is
intended to make IOCs less susceptible to jumps in system time.

### Iocsh `on error ...`

A new statement is added to enable IOC shell commands to signal error
conditions, and for scripts to respond. This first is through the new function

```C
    int iocshSetError(int err);
```

A script may be prefixed with eg. "on error break" to stop at the failed
command.

```sh
    on error continue | break | wait [value] | halt
```

A suggested form for IOC shell commands is:

```C
    static void doSomethingCallFunc(const iocshArgBuf *args)
    {
        iocshSetError(doSomething(...)); /* return 0 == success */
    }
```

### Relocatable Builds

Allows built trees to be copied or moved without invalidating RPATH entires.

The `LINKER_USE_RPATH` Makefile variable (see `configure/CONFIG_SITE`) may be
set to `YES`, `NO`, and a new third option `ORIGIN`.  This is limited to
targets using the ELF executable format (eg. Linux).

When `LINKER_USE_RPATH=ORIGIN`, the variable `LINKER_ORIGIN_ROOT` is set to
one of the parents of the build directory.  Any libraries being linked
to which are found under this root will have a relative RPATH entry.
Other libraries continue to result in absolute RPATH entries.

An effect of this might change a support library from being linked with
`-Wl,-rpath /build/epics-base/lib/linux-x86`
to being linked with
`-Wl,-rpath \$ORIGIN/../../../epics-base/lib/linux-x86`
if the support module directory is `/build/mymodule`
and `LINKER_ORIGIN_ROOT=/build`.

The API functions `epicsGetExecDir()` and `epicsGetExecName()` are also
added to `osiFileName.h` to provide runtime access to the directory or
filename of the executable with which the process was started.

### Decouple `LINKER_USE_RPATH` and `STATIC_BUILD`

Previously, setting `STATIC_BUILD=NO` implied `LINKER_USE_RPATH=NO`.
This is no longer the case.  Setting `LINKER_USE_RPATH=YES` will
always emit RPATH entries.  This was found to be helpful when linking
against some 3rd party libraries which are only available as shared objects.

### Channel Access Security: Check Hostname Against DNS

Host names given in a `HAG` entry of an IOC's Access Security Configuration
File (ACF) have to date been compared against the hostname provided by the CA
client at connection time, which may or may not be the actual name of that
client. This allows rogue clients to pretend to be a different host, and the
IOC would believe them.

An option is now available to cause an IOC to ask its operating system to look
up the IP address of any hostnames listed in its ACF (which will normally be
done using the DNS or the `/etc/hosts` file). The IOC will then compare the
resulting IP address against the client's actual IP address when checking
access permissions at connection time. This name resolution is performed at
ACF file load time, which has a few consequences:

  1. If the DNS is slow when the names are resolved this will delay the process
of loading the ACF file.

  2. If a host name cannot be resolved the IOC will proceed, but this host name
will never be matched.

  3. Any changes in the hostname to IP address mapping will not be picked up by
the IOC unless and until the ACF file gets reloaded.

Optionally, IP addresses may be added instead of, or in addition to, host
names in the ACF file.

This feature can be enabled before `iocInit` with

```
    var("asCheckClientIP",1)
```

or with the VxWorks target shell use

```C
    asCheckClientIP = 1
```

### New and modified epicsThread APIs

#### `epicsThreadCreateOpt()`

A new routine `epicsThreadCreateOpt()` is an alternative to
`epicsThreadCreate()` which takes some arguments via a structure (`struct
epicsThreadOpts`) to allow for future extensions.

```C
    typedef struct epicsThreadOpts {
        unsigned int priority;
        unsigned int stackSize;
        unsigned int joinable;
    } epicsThreadOpts;
    #define EPICS_THREAD_OPTS_INIT { \
        epicsThreadPriorityLow, epicsThreadStackMedium, 0}
     epicsThreadId epicsThreadCreateOpt(const char * name,
        EPICSTHREADFUNC funptr, void * parm, const epicsThreadOpts *opts);
```

The final `opts` parameter may be `NULL` to use the default values of thread
priority (low) and stack size (medium). Callers wishing to provide alternative
settings for these thread options or to create a joinable thread (see below)
should create and pass in an `epicsThreadOpts` structure as shown below.
Always initialize one of these structures using the `EPICS_THREAD_OPTS_INIT`
macro to ensure that any additional fields that get added in the future are
set to their default values.

```C
    void startitup(void) {
        epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
        epicsThreadId tid;

        opts.priority = epicsThreadPriorityMedium;
        tid = epicsThreadCreateOpt("my thread", &threadMain, NULL, &opts);
    }
```

C or C++ Code that also needs to build on earlier versions of Base can use
`#ifdef EPICS_THREAD_OPTS_INIT` to determine whether the
`epicsThreadCreateOpt()` API is available on this Base version.

#### Thread stack sizes

The `stackSize` member of the `epicsThreadOpts` structure and the equivalent
parameters to the `epicsThreadCreate()` and `epicsThreadMustCreate()` routines
can now be passed either one of the `epicsThreadStackSizeClass` enum values or
a value returned from the `epicsThreadGetStackSize()` routine.

#### `epicsThreadMustJoin()`

If the new `joinable` flag of an `epicsThreadOpts` structure is non-zero (the
default value is zero), the new API routine `epicsThreadMustJoin()` *must* be
called with the thread's `epicsThreadId` when/after the thread exits, to free
up thread resources. This function will block until the thread's main function
has returned, allowing the parent to wait for its child thread. The child's
`epicsThreadId` will no longer be valid and should not be used after the
`epicsThreadMustJoin()` routine returns.

A thread that was originally created with its joinable flag set may itself
call `epicsThreadMustJoin()`, passing in its own epicsThreadId. This marks the
thread as no longer being joinable, so it will then free the thread resources
itself when its main function returns. The `epicsThreadId` of a thread that is
not joinable gets invalidated as soon as its main function returns.

### Non-VME RTEMS targets now define pdevLibVME

Previously IOC executables that made calls to devLib routines would fail to
link when built for some non-VME based RTEMS targets, which would have to be
explicitly filtered out by sites that build Base for those targets. [This
fix](https://bugs.launchpad.net/epics-base/+bug/1841692) makes that no longer
necessary, all RTEMS targets should now link although the IOC won't be able to
be used with the VME I/O on those systems (that we don't have VMEbus I/O
support for in libCom).
