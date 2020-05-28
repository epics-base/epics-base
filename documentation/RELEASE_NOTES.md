# EPICS 7.0 Release Notes

These release notes describe changes that have been made since the previous
release of this series of EPICS Base. **Note that changes which were merged up
from commits to new releases in an older Base series are not described at the
top of this file but have entries that appear lower down, under the series to
which they were originally committed.** Thus it is important to read more than
just the first section to understand everything that has changed in each
release.

The PVA submodules each have their own individual sets of release notes which
should also be read to understand what has changed since earlier releases.

## EPICS Release 7.0.4

### Bug fixes

The following launchpad bugs have fixes included in this release:

- [lp: 1812084](https://bugs.launchpad.net/bugs/1812084), Build failure on
  RTEMS 4.10.2
- [lp: 1829919](https://bugs.launchpad.net/bugs/1829919), IOC segfaults when
  calling dbLoadRecords after iocInit
- [lp: 1838792](https://bugs.launchpad.net/bugs/1838792), epicsCalc bit-wise
  operators on aarch64
- [lp: 1853148](https://bugs.launchpad.net/bugs/1853148), mingw compiler
  problem with printf/scanf formats
- [lp: 1852653](https://bugs.launchpad.net/bugs/1852653), USE_TYPED_DSET
  incompatible with C++
- [lp: 1862328](https://bugs.launchpad.net/bugs/1862328), Race condition on
  IOC start leaves rsrv unresponsive
- [lp: 1866651](https://bugs.launchpad.net/bugs/1866651), thread joinable race
- [lp: 1868486](https://bugs.launchpad.net/bugs/1868486), epicsMessageQueue
  lost messages
- [lp: 1868680](https://bugs.launchpad.net/bugs/1868680), Access Security file
  reload (asInit) fails

### \*_API macros in EPICS headers

Internally, the Com and ca libraries now express dllimport/export (Windows)
and symbol visibility (GCC) using library-specific macros (eg. `LIBCOM_API`)
instead of the macros `epicsShareFunc`, `epicsShareClass`, `epicsShareDef` etc.
that are defined in the `shareLib.h` header.
This change may affect some user code which uses the `epicsShare*` macros
without having explicitly included the `shareLib.h` header themselves.
Such code should be changed to include `shareLib.h` directly.

A new helper script `makeAPIheader.pl` and build rules to generate a
library-specific `*API.h` header file has been added. Run `makeAPIheader.pl -h`
for information on how to use this in your own applications, but note that the
resulting sources will not be able to be compiled using earlier versions of
EPICS Base.

### IOCsh usage messages

At the iocShell prompt `help <cmd>` now prints a descriptive usage message
for many internal IOCsh commands in addition to the command parameters.
Try `help *` to see all commands, or a glob pattern such as `help db*` to see
a subset.

External code may provide usage messages when registering commands using a
new `const char *usage` member of the `iocshFuncDef` structure.
The `iocsh.h` header also now defines a macro `IOCSHFUNCDEF_HAS_USAGE` which
can be used to detect Base versions that support this feature at compile-time.

### Variable names in RELEASE files

`configure/RELEASE` files are parsed by both GNUmake and the `convertRelease.pl`
script. While GNUmake is quite relaxed about what characters may be used in a
RELEASE variable name, the `convertRelease.pl` script parser has only recognized
variable names that match the Perl regular expression `\w+`, i.e. upper and
lower-case letters, digits and underscore characters.

The script has been modified so now RELEASE variable names must start with a
letter or underscore, and be followed by any number of letters, digits,
underscore or hyphen characters, matching the regular expression
`[A-Za-z_][A-Za-z_0-9-]*`. The hyphen character `-` was not previously allowed
and if used would have prevented a build from finding include files and
libraries in any module using that in its RELEASE variable name.

This change does disallow names that start with a digit which used to be
allowed, but hopefully nobody has been relying on that ability. The regular
expression used for names can be found in the file `src/tools/EPICS/Release.pm`
and can be adjusted locally if necessary.

### caRepeater /dev/null

On \*NIX targets caRepeater will now partially daemonize by redirecting
stdin/out/err to /dev/null.  This prevents caRepeater from inheriting
the stdin/out of a process, like caget, which has spawned it in the
background.  This has been known to cause problems in some cases when
caget is itself being run from a shell script.

caRepeater will now understand the `-v` argument to retain stdin/out/err
which may be necessary to see any error messages it may emit.

### `state` record deprecated

IOCs now emit a warning when a database file containing the `state` record is
loaded. This record has been deprecated for a while and will be removed
beginning with EPICS 7.1. Consider using the `stringin` record instead.

### Record types publish dset's

The record types in Base now define their device support entry table (DSET)
structures in the record header file. While still optional, developers of
external support modules are encouraged to start converting their code to use
the record's new definitions instead of the traditional approach of copying the
structure definitions into each source file that needs them. By following the
instructions below it is still possible for the converted code to build and
work with older Base releases.

This would also be a good time to modify the device support to use the type-safe
device support entry tables that were introduced in Base-3.16.2 -- see
[#type-safe-device-and-driver-support-tables](this entry below) for the
description of that change, which is also optional for now.

Look at the aiRecord for example. Near the top of the generated `aiRecord.h`
header file is a new section that declares the `aidset`:

```C
/* Declare Device Support Entry Table */
struct aiRecord;
typedef struct aidset {
    dset common;
    long (*read_ai)(struct aiRecord *prec);
    long (*special_linconv)(struct aiRecord *prec, int after);
} aidset;
#define HAS_aidset
```

Notice that the common members (`number`, `report()`, `init()`, `init_record()`
and `get_ioint_info()` don't appear directly but are included by embedding the
`dset common` member instead. This avoids the need to have separate definitions
of those members in each record dset, but does require those members to be
wrapped inside another set of braces `{}` when initializing the data structure
for the individual device supports. It also requires changes to code that
references those common members, but that code usually only appears inside the
record type implementation and very rarely in device supports.

An aiRecord device support that will only be built against this or later
versions of EPICS can now declare its dset like this:

```C
aidset devAiSoft = {
    { 6, NULL, NULL, init_record, NULL },
    read_ai, NULL
};
epicsExportAddress(dset, devAiSoft);
```

However most device support that is not built into EPICS itself will need to
remain compatible with older EPICS versions, which is why the ai record's header
file also declares the preprocessor macro `HAS_aidset`. This makes it easy to
define the `aidset` in the device support code when it's needed, and not when
it's provided in the header:

```C
#ifndef HAS_aidset
typedef struct aidset {
    dset common;
    long (*read_ai)(aiRecord *prec);
    long (*special_linconv)(aiRecord *prec, int after);
} aidset;
#endif
aidset devAiSoft = {
    { 6, NULL, NULL, init_record, NULL },
    read_ai, NULL
};
epicsExportAddress(dset, devAiSoft);
```

The above `typedef struct` declaration was copied directly from the new
aiRecord.h file and wrapped in the `#ifndef HAS_aidset` conditional.

This same pattern should be followed for all record types except for the lsi,
lso and printf record types, which have published their device support entry
table structures since they were first added to Base but didn't previously embed
the `dset common` member. Device support for these record types therefore can't
use the dset name since the new definitions are different from the originals and
will cause a compile error, so this pattern should be used instead:

```C
#ifndef HAS_lsidset
struct {
    dset common;
    long (*read_string)(lsiRecord *prec);
}
#else
lsidset
#endif
devLsiEtherIP = {
    {5, NULL, lsi_init, lsi_init_record, get_ioint_info},
    lsi_read
};
```

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

### Decouple LINKER_USE_RPATH and STATIC_BUILD

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

## EPICS Release 7.0.3

### `epicsTimeGetCurrent()` optimization

Add a fast path to epicsTimeGetCurrent() and related calls in the common case
where only the default OS current time provider is registered. This path does
not take the global mutex guarding the time providers list, potentially
reducing lock contention.

### dbEvent tweak Queue size

The size of the queue used by dbEvent to push monitor updates has been
slightly increased based on `DBR_TIME_DOUBLE` to better fill an ethernet frame.
This may result in slightly fewer, but larger frames being sent.

### mbbo/mbbiDirect number of bits as precision

Report NOBT as "precision" through the dbAccess API. This is not accessible
through CA, but is planned to be used through QSRV.

## EPICS Release 7.0.2.2

### Build System changes

 * The GNUmake build targets `cvsclean` and `depclean` are now available from
any directory; previously they were only available from application top
directories.

 * The approach that EPICS Base uses for building submodules inside the parent
module looks useful for support modules too. The rules for building submodules
have been modified and extracted into a new `RULES_MODULES` file, so a support
module will be able to use them too without having to copy them into its own
`modules/Makefile`. There are some specific requirements that support modules
and their submodules must follow, which are described as comments in the new
`base/configure/RULES_MODULES` file itself.

### `EPICS_BASE_VERSION` Update Policy change

In the past, a build of EPICS using sources checked out from the repository
branch between official releases would have shown the version number of the
previous release, followed by a -DEV suffix, for example 7.0.2.1-DEV.

The policy that controls when the number gets updated has been changed, and
now immediately after a release has been tagged the version number will be
updated to the next patch release version, plus the -DEV suffix as before.
Thus following 7.0.2.2 the version number will show as 7.0.2.3-DEV. This does
not require the next official release to be numbered 7.0.2.3 though, it could
become 7.0.3 or even 7.1.0 if the changes incorporated into it are more
substantial than bug fixes.

### Drop `CLOCK_MONOTONIC_RAW` from posix/osdMonotonic.c

Turns out this is ~10x slower to query than `CLOCK_MONOTONIC`.

## EPICS Release 7.0.2.1

### Linking shared libraries on macOS

The linker flag `-flat_namespace` has been restored for creating shared
libraries, although not for loadable libraries (bundles). This was required
for building using the latest versions of Apple XCode.

### Fix `DB_LINK` loop breaking

A regression was introduced in 7.0.2 which caused record chains with loops to
be incorrectly broken. Processing should be skipped when a `DB_LINK` with
Process Passive (PP) closes a loop to a synchronous record.

Instead in 7.0.2 the targeted record would be processed if processing began
with a remote action (or some other caller of `dbPutField()`). This would
result in the loop running a second time. The loop would be broken on the
second iteration.

[See lp: #1809570](https://bugs.launchpad.net/epics-base/+bug/1809570)

### Old dbStaticLib APIs removed

Support for some obsolete dbStaticLib Database Configuration Tool (DCT) APIs
was removed some time ago, but vestiges of them still remained. The following
routines and macros and have now finally been removed:

  * `int dbGetFieldType(DBENTRY *pdbentry)`
  * `int dbGetLinkType(DBENTRY *pdbentry)`
  * `DCT_STRING`
  * `DCT_INTEGER`
  * `DCT_REAL`
  * `DCT_MENU`
  * `DCT_MENUFORM`
  * `DCT_INLINK`
  * `DCT_OUTLINK`
  * `DCT_FWDLINK`
  * `DCT_NOACCESS`
  * `DCT_LINK_CONSTANT`
  * `DCT_LINK_FORM`
  * `DCT_LINK_PV`

### Fix for `dbhcr` before `iocInit`

The `dbhcr` command used to work before `iocInit` as well as afterwards. It
displays all records that have hardware addresses (`VME_IO`, `CAMAC_IO`,
`GPIB_IO`, `INST_IO` etc.) but stopped working if run before iocInit due to the
rewrite of the link address parser code in dbStaticLib. This release fixes that
issue, although in some cases the output may be slightly different than it used
to be.

## EPICS Release 7.0.2

### Launchpad Bugs

The list of tracked bugs fixed in this release can be found on the
[Launchpad Milestone page for EPICS Base 7.0.2](https://launchpad.net/epics-base/+milestone/7.0.2).

### Git Branches Recombined

The four separate Git branches `core/master`, `libcom/master`, `ca/master` and
`database/master` have been recombined into one branch called `7.0`. Keeping
these as 4 separate branches in the same repository made it impossible to
create merge requests that contained changes in more than one of these
modules. The layout of the source files has not changed at all however, so the
source code for libcom, ca and the database are still found separately under
the module subdirectory.

## EPICS Release 7.0.1.1

### Changed SIML failure behavior

A failure when fetching the simulation mode through `SIML` will not put the
record into INVALID alarm state anymore. Instead, as long as the record's
current alarm severity (`SEVR`)is `NO_ALARM`, its alarm status (`STAT`) will be
set to `LINK_ALARM` without increasing the severity. This allows clients to get
some notification of a failing or bad `SIML` link without otherwise affecting
record processing.

### `dbVerify()` has been restored to dbStaticLib

This routine was removed in Base-3.16.1 but has been reimplemented in this
release by special request. Note that the error message strings that it
returns when verification fails have changed, but are still designed for
display to the user.

### Simulation mode improvements

Records that support simulation mode have two new fields, `SSCN` (Simulation
Scan Mode) and `SDLY` (Simulation Delay). `SSCN` is a menu field that provides
an alternate value for the `SCAN` field to be used while the record is in
simulation mode. This is especially useful for I/O scanned records, for which
simulation mode was not working at all. Setting `SDLY` to a positive value
makes the record process asynchronously in simulation mode, with the second
stage processing happening after the specified time (in seconds).

### Extend the dbServer API with init/run/pause/stop methods

This change permits IOCs to be built that omit the CA server (RSRV) by
removing its registrar entry which is now provided in the new `rsrv.dbd` file.
Other server layers can be built into the IOC (alongside RSRV or in place of
it) by registering them in a similar manner. The dbServer API is documented
with Doxygen comments in the header file.

Specific IOC server layers can be disabled at runtime by adding their name to
the environment variable `EPICS_IOC_IGNORE_SERVERS` (separated by spaces if more
than one should be ignored).

### Grand source-code reorganization

EPICS 7.0.1 contains the IOC Database, RSRV server and the Channel Access
client code from EPICS Base 3.16.1 along with all the original record types
and soft device support, but GDD and the Portable Channel Access Server have
been unbundled and are now available separately. In their place we have
brought in the more recently written EPICS V4 C++ libraries (collectively
referred to as the PVA modules). The directory tree for EPICS is somewhat
larger as a result, and the original structure of the Base directories has
been split into 4 separate Git repositories. External modules should build
against this new structure with little or no changes needed, except that some
allowance may be needed for the merging of the V4 modules.

There should be rather more description and documantation of these changes
than is currently available, but as developers we generally much prefer to
write code than documentation. Send questions to the tech-talk mailing list
and we'll be happy to try and answer them!

## Changes between 3.16.1 and 3.16.2

The list of tracked bugs fixed in this release can be found on the
[Launchpad Milestone page for EPICS Base 3.16.2](https://launchpad.net/epics-base/+milestone/3.16.2).

### Status reporting for the callback and scanOnce task queues

Two new iocsh commands and some associated underlying APIs have been added to
show the state of the queues that feed the three callback tasks and the
scanOnce task, including a high-water mark which can optionally be reset. The
new iocsh commands are `callbackQueueShow` and `scanOnceQueueShow`; both take
an optional integer argument which must be non-zero to reset the high-water
mark.

### Support for event codes greater than or equal to `NUM_TIME_EVENTS`

Event numbers greater than or equal to `NUM_TIME_EVENTS` are now allowed if
supported by the registered event time provider, which must provide its own
advancing timestamp validation for such events.

Time events numbered 0 through `(NUM_TIME_EVENTS-1)` are still validated by code
in epicsGeneralTime.c that checks for advancing timestamps and enforces that
restriction.

### Type-safe Device and Driver Support Tables

Type-safe versions of the device and driver support structures `dset` and
`drvet` have been added to the devSup.h and drvSup.h headers respectively. The
original structure definitions have not been changed so existing support
modules will still build normally, but older modules can be modified and new
code written to be compatible with both.

The old structure definitions will be replaced by the new ones if the macros
`USE_TYPED_DSET` and/or `USE_TYPED_DRVET` are defined when the appropriate
header is included. The best place to define these is in the Makefile, as with
the `USE_TYPED_RSET` macro that was introduced in Base-3.16.1 and described
below. See the comments in devSup.h for a brief usage example, or look at
[this commit](https://github.com/epics-modules/ipac/commit/a7e0ff4089b9aa39108bc8569e95ba7fcf07cee9)
to the ipac module to see a module conversion.

A helper function `DBLINK* dbGetDevLink(dbCommon *prec)` has also been added
to devSup.h which fetches a pointer to the INP or OUT field of the record.

### RTEMS build configuration update, running tests under QEMU

This release includes the ability to run the EPICS unit tests built for a
special version of the RTEMS-pc386 target architecture on systems that have an
appropriate QEMU emulator installed (`qemu-system-i386`). It is also now
possible to create sub-architectures of RTEMS targets, whereas previously the
EPICS target architecture name had to be `RTEMS-$(RTEMS_BSP)`.

The new target `RTEMS-pc386-qemu` builds binaries that can be run in the
`qemu-system-i386` PC System emulator. This target is a derivative of the
original `RTEMS-pc386` target but with additional software to build an in-
memory file-system, and some minor modifications to allow the unit tests to
work properly under QEMU. When this target is enabled, building any of the
make targets that cause the built-in self-tests to be run (such as `make
runtests`) will also run the tests for RTEMS using QEMU.

To allow the new 3-component RTEMS target name, the EPICS build system for
RTEMS was modified to allow a `configure/os/CONFIG.Common.<arch>` file to set
the `RTEMS_BSP` variable to inform the build what RTEMS BSP to use. Previously
this was inferred from the value of the `T_A` make variable, but that prevents
having multiple EPICS targets that build against the same BSP. All the
included RTEMS target configuration files have been updated; build
configuration files for out-of-tree RTEMS targets will continue to work as the
original rules are used to set `RTEMS_BSP` if it hasn't been set when needed.

### Link type enhancements

This release adds three new link types: "state", "debug" and "trace". The
"state" link type gets and puts boolean values from/to the dbState library
that was added in the 3.15.1 release. The "debug" link type sets the
`jlink::debug` flag in its child link, while the "trace" link type also causes
the arguments and return values for all calls to the child link's jlif and
lset routines to be printed on stdout. The debug flag can no longer be set
using an info tag. The addition of the "trace" link type has allowed over 200
lines of conditional diagnostic printf() calls to be removed from the other
link types.

The "calc" link type can now be used for output links as well as input links.
This allows modification of the output value and even combining it with values
from other input links. See the separate JSON Link types document for details.

A new `start_child()` method was added to the end of the jlif interface table.

The `lset` methods have now been properly documented in the dbLink.h header
file using Doxygen annotations, although we do not run Doxygen on the source
tree yet to generate API documentation.

Link types that utilize child links must now indicate whether the child will
be used for input, output or forward linking by the return value from its
`parse_start_map()` method. The `jlif_key_result` enum now contains 3 values
`jlif_key_child_inlink`, `jlif_key_child_outlink` and `jlif_key_child_fwdlink`
instead of the single `jlif_key_child_link` that was previously used for this.

### GNUmake targets for debugging

Some additional build rules have been added to help debug configuration
problems with the build system. Run `make show-makefiles` to get a sorted list
of all the files that the build system includes when building in the current
directory.

A new pattern rule for `PRINT.%` can be used to show the value of any GNUmake
variable for the current build directory (make sure you are in the right
directory though, many variables are only set when inside the `O.<arch>` build
directory). For example `make PRINT.T_A` will display the build target
architecture name from inside a `O.<arch>` directory but the variable will be
empty from an application top or src directory. `make PRINT.EPICS_BASE` will
show the path to Base from any EPICS application directory though.

### Propagate PUTF across Asynchronous record processing

The IOC contains a mechanism involving the PUTF and RPRO fields of each record
to ensure that if a record is busy when it receives a put to one of its
fields, the record will be processed again to ensure that the new field value
has been correctly acted on. Until now that mechanism only worked if the put
was to the asynchronous record itself, so puts that were chained from some
other record via a DB link did not cause reprocessing.

In this release the mechanism has been extended to propagate the PUTF state
across DB links until all downstream records have been reprocessed. Some
additional information about the record state can be shown by setting the TPRO
field of an upstream record, and even more trace data is displayed if the
debugging variable `dbAccessDebugPUTF` is set in addition to TPRO.

### Finding info fields

A new iocsh command `dbli` lists the info fields defined in the database, and
can take a glob pattern to limit output to specific info names. The newly
added dbStaticLib function `dbNextMatchingInfo()` iterates through the info
fields defined in the current record, and is used to implement the new
command.

### Output from `dbpr` command enhanced

The "DataBase Print Record" command `dbpr` now generates slightly better
output, with more field types having their own display methods. This release
also includes additional protection against buffer overflows while printing
long links in `dbpr`, and corrects the output of long strings from the `dbgf`
command.

### Record types mbbiDirect and mbboDirect upgraded to 32 bit

The VAL fields and related fields of these records are now `DBF_LONG`. (Not
`DBF_ULONG` in order to prevent Channel Access from promoting them to
`DBF_DOUBLE`.) Additional bit fields `B10`...`B1F` have been added.

Device support that accesses `VAL` or the bit fields directly (most don't) and
aims for compatibility with old and new versions of these records should use
at least 32 bit integer types to avoid bit loss. The number of bit fields can
be calculated using `8 * sizeof(prec->val)` which is correct in both versions.

### Restore use of ledlib for VxWorks command editing

The epicsReadline refactoring work described below unfortunately disabled the
VxWorks implementation of the osdReadline.c API that uses ledlib for command
editing and history. This functionality has now been restored, see Launchpad
[bug #1741578](https://bugs.launchpad.net/bugs/1741578).

### Constant link types

Constant links can now hold 64-bit integer values, either as scalars or
arrays. Only base 10 is supported by the JSON parser though, the JSON standard
doesn't allow for hexadecimal numbers.

### Upgraded the YAJL JSON Library

The third-party YAJL library that has been included in libCom for several
years has been upgraded to version 2.1.0 and several bugs fixed. This has an
updated API, requiring any code that uses it to parse its own JSON files to be
modified to match. The changes are mainly that it uses `size_t` instead
`unsigned int` for string lengths, but it also uses `long long` instead of
`long` for JSON integer values, which was the main motivation for the upgrade.

The self-tests that YAJL comes with have been imported and are now run as an
EPICS Unit Test program, and the JSON syntax accepted by the parser was
extended to permit trailing commas in both arrays and maps. The difference
between the old and new YAJL APIs can be detected at compile time by looking
for the macro `EPICS_YAJL_VERSION` which is defined in the `yajl_common.h`
header file along with a brief description of the API changes.

### Timestamp support for the calc link type

A new optional parameter can be given when specifying a calc JSON link. The
`time` parameter is a string containing a single letter `A..L` that selects
one of the input links to be used for the timestamp of calculation if
requested. The timestamp will be fetched atomically with the value from the
chosen input link (providing that input link type supports the readLocked()
method).

### Silence errors from puts to constant link types

A soft channel output record with the OUT link unset uses the CONSTANT link
type. The new link type code was causing some soft channel device supports to
return an error status from the write method of that link type, which would
cause a `ca_put()` operation to such a record to generate an exception. This has
been silenced by giving the constant link types a dummy putValue method. A new
test program has been added to prevent regressions of this behaviour.

### RSRV expanding large buffer causes crash

In the 3.16.1 release a crash can occur in the IOC's RSRV server when a large
array is made even larger; the previous array buffer was not being released
correctly. See Launchpad
[bug #1706703](https://bugs.launchpad.net/epics-base/+bug/1706703).

## Changes made between 3.16.0.1 and 3.16.1

### IOC Database Support for 64-bit integers

The IOC now supports the 64-bit integer field types `DBF_INT64` and
`DBF_UINT64`, and there are new record types `int64in` and `int64out` derived
from the `longin` and `longout` types respectively that use the `DBF_INT64`
data type for their VAL and related fields. The usual range of Soft Channel
device support are included for these new record types.

All internal IOC APIs such as dbAccess can handle the new field types and
their associated request values `DBR_INT64` and `DBR_UINT64`, which are
implemented using the `epicsInt64` and `epicsUInt64` typedef's from the
`epicsTypes.h` header.

The waveform record type has been updated to support these new field types.
**All waveform device support layers must be updated to recognize the new type
enumeration values**, which had to be inserted before the `FLOAT` value in the
enum `dbfType` and in `menuFtype`. C or C++ code can detect at compile-time
whether this version of base provides 64-bit support by checking for the
presence of the `DBR_INT64` macro as follows (Note that `DBF_INT64` is an
enum tag and not a preprocessor macro):

```
    #ifdef DBR_INT64
        /* Code where Base has INT64 support */
    #else
        /* Code for older versions */
    #endif
```

If the code uses the old `db_access.h` types (probably because it's calling
Channel Access APIs) then it will have to test against the EPICS version
number instead, like this:

```
    #include <epicsVersion.h>

    #ifndef VERSION_INT
    #  define VERSION_INT(V,R,M,P) ( ((V)<<24) | ((R)<<16) | ((M)<<8) | (P))
    #endif
    #ifndef EPICS_VERSION_INT
    #  define EPICS_VERSION_INT VERSION_INT(EPICS_VERSION, EPICS_REVISION, EPICS_MODIFICATION, EPICS_PATCH_LEVEL)
    #endif

    #if EPICS_VERSION_INT >= VERSION_INT(3,16,1,0)
        /* Code where Base has INT64 support */
    #else
        /* Code for older versions */
    #endif
```

Channel Access does not (and probably never will) directly support 64-bit
integer types, so the new field types are presented to the CA server as
`DBF_DOUBLE` values. This means that field values larger than 2^52
(0x10_0000_0000_0000 = 4503599627370496) cannot be transported over Channel
Access without their least significant bits being truncated. The EPICS V4
pvAccess network protocol _can_ transport 64-bit data types however, and a
future release of the pvaSrv module will connect this ability to the fields of
the IOC.

Additional 64-bit support will be provided in later release. For instance the
JSON parser for the new Link Support feature only handles integers up to 32
bits wide, so constant array initializer values cannot hold larger values in
this release.

### Add `EPICS_CA_MCAST_TTL`

A new environment parameter `EPICS_CA_MCAST_TTL` is used to set the Time To Live
(TTL) value of any IP multi-cast CA search or beacon packets sent.

### `EPICS_CA_MAX_ARRAY_BYTES` is optional

A new environment parameter `EPICS_CA_AUTO_ARRAY_BYTES` is now used by libca and
RSRV (CA clients and the IOC CA server). The default is equivalent to setting
`EPICS_CA_AUTO_ARRAY_BYTES=YES` which removes the need to set
`EPICS_CA_MAX_ARRAY_BYTES` and always attempts to allocate sufficiently large
network buffers to transfer large arrays properly over the network. In this case
the value of the `EPICS_CA_MAX_ARRAY_BYTES` parameter is ignored.

Explicitly setting `EPICS_CA_AUTO_ARRAY_BYTES=NO` will continue to honor the
buffer setting in `EPICS_CA_AUTO_ARRAY_BYTES` as in previous releases.

The default setting for `EPICS_CA_AUTO_ARRAY_BYTES` can be changed by adding the
line

```makefile
    EPICS_CA_AUTO_ARRAY_BYTES=NO
```

to the `configure/CONFIG_SITE_ENV` file before building Base. Sites that wish to
override this only for specific IOC architectures can create new files for each
architecture named `configure/os/CONFIG_SITE_ENV.<target-arch>` with the above
setting in before building Base. The configuration can also be explicitly
changed by setting the environment variable in the IOC's startup script,
anywhere above the `iocInit` line.

The PCAS server (used by the PV Gateway and other CA servers) now always behaves
as if `EPICS_CA_AUTO_ARRAY_BYTES` is set to `YES` (it ignores the configuration
parameter and environment variable).

### Channel Access "modernization"

Drop support for CA clients advertising protocol versions less than 4.

This effects clients from Base older than 3.12.0-beta1. Newer clients will
continue to be able to connect to older servers. Older clients will be ignored
by newer servers.

This allows removal of UDP echo and similar protocol features which are not
compatible with secure protocol design practice.

### Lookup-tables using the subArrray record

The subArray record can now be used as a lookup-table from a constant array
specified in its INP field. For example:

```
    record(subArray, "powers-of-2") {
      field(FTVL, "LONG")
      field(MALM, 12)
      field(INP, [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048])
      field(INDX, 0)
      field(NELM, 1)
    }
```

The INDX field selects which power of 2 to set the VAL field to. In previous
releases the INP field would have to have been pointed to a separate waveform
record that was initialized with the array values somehow at initialization
time.

### Synchronized Timestamps with TSEL=-2

Most Soft Channel input device support routines have supported fetching the
timestamp through the INP link along with the input data. However before now
there was no guarantee that the timestamp provided by a CA link came from the
same update as the data, since the two were read from the CA input buffer at
separate times without maintaining a lock on that buffer in between. This
shortcoming could be fixed as a result of the new link support code, which
allows code using a link to pass a subroutine to the link type which will be
run with the link locked. The subroutine may make multiple requests for
metadata from the link, but must not block.

### Extensible Link Types

A major new feature introduced with this release of EPICS Base is an
Extensible Link Type mechanism, also known as Link Support or JSON Link Types.
This addition permits new kinds of link I/O to be added to an IOC in a similar
manner to the other extension points already supported (e.g. record, device
and driver support).

A new link type must implement two related APIs, one for parsing the JSON
string which provides the link address and the other which implements the link
operations that get called at run-time to perform I/O. The link type is built
into the IOC by providing a new `link` entry in a DBD file.

#### New Link Types Added

This release contains two new JSON link types, `const` and `calc`:

 * The `const` link type is almost equivalent to the old CONSTANT link type
with the updates described below to accept arrays and strings, except that
there is no need to wrap a scalar string constant inside array brackets since
a constant string will never be confused with a PV name.

 * The `calc` link type allows CALC expressions to be used to combine
values from other JSON links to produce its value. Until additional JSON link
types are created though, the `calc` link type has little practical utility as
it can currently only fetch inputs from other `calc` links or from `const`
links.

```
    field(INP, {calc:{expr:"A+B+1",
                      args:[5,         # A
                            {const:6}] # B
                     }
               }
           )
```

The new link types are documented in a separate document that gets generated at build time and installed as `html/links.html`.

#### Device Support Addressing using `JSON_LINK`

The API to allow device support to use JSON addresses is currently
incomplete; developers are advised not to try creating device support that
specifies a `JSON_LINK` address type.

#### Support Routine Modifications for Extensible Link Types

For link fields in external record types and soft device support to be able
to use the new link types properly, various changes are required to utilize
the new Link Support API as defined in the dbLink.h header file and outlined
below. The existing built-in Database and Channel Access link types have been
altered to implement the link APIs, so will work properly after these
conversions:

 * Make all calls to `recGblInitConstantLink()` unconditional on the link
type, i.e. change this code:

```C
        if (prec->siml.type == CONSTANT) {
            recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);
        }
```

  into this:

```C
        recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);
```

  Note that `recGblInitConstantLink()` still returns TRUE if the field was
  successfully initialized from the link (implying the link is constant).
  This change will work properly with all Base releases currently in use.

 * Code that needs to identify a constant link should be modified to use
the new routine `dbLinkIsConstant()` instead, which returns TRUE for constant
or undefined links, FALSE for links whose `dbGetLink()` routine may return
different values on different calls. For example this:

```C
        if (prec->dol.type != CONSTANT)
```

  should become this:

```C
        if (!dbLinkIsConstant(&prec->dol))
```

  When the converted software is also required to build against older versions
  of Base, this macro definition may be useful:

```C
        #define dbLinkIsConstant(lnk) ((lnk)->type == CONSTANT)
```

 * Any code that calls dbCa routines directly, or that explicitly checks if
a link has been resolved as a CA link using code such as

```C
        if (prec->inp.type == CA_LINK)
```

  will still compile and run, but will only work properly with the old CA link
  type. To operate with the new extensible link types such code must be
  modified to use the new generic routines defined in dbLink.h and should
  never attempt to examine or modify data inside the link. After conversion
  the above line would probably become:

```C
        if (dbLinkIsVolatile(&prec->inp))
```

  A volatile link is one like a Channel Access link which may disconnect and
  reconnect without notice at runtime. Database links and constant links are
  not volatile; unless their link address is changed they will always remain
  in the same state they started in. For compatibility when building against
  older versions of Base, this macro definition may be useful:

```C
        #define dbLinkIsVolatile(lnk) ((lnk)->type == CA_LINK)
```

 * The current connection state of a volatile link can be found using the
routine `dbIsLinkConnected()` which will only return TRUE for a volatile link
that is currently connected. Code using the older dbCa API returning this
information used to look like this:

```C
        stat = dbCaIsLinkConnected(plink);
```

  which should become:

```C
        stat = dbIsLinkConnected(plink);
```

  Similar changes should be made for calls to the other dbCa routines.

 * A full example can be found by looking at the changes to the calcout
record type, which has been modified in this release to use the new dbLink
generic API.

### Constant Link Values

Previously a constant link (i.e. a link that did not point to another PV,
either locally or over Channel Access) was only able to provide a single
numeric value to a record initialization; any string given in a link field
that was not recognized as a number was treated as a PV name. In this release,
constant links can be expressed using JSON array syntax and may provide array
initialization of values containing integers, doubles or strings. An array
containing a single string value can also be used to initialize scalar
strings, so the stringin, stringout, lsi (long string input), lso (long string
output), printf, waveform, subArray and aai (analog array input) record types
and/or their soft device supports have been modified to support this.

Some examples of constant array and string initialized records are:

```
    record(stringin, "const:string") {
        field(INP, ["Not-a-PV-name"])
    }
    record(waveform, "const:longs") {
        field(FTVL, LONG)
        field(NELM, 10)
        field(INP, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
    }
    record(aai, "const:doubles") {
        field(FTVL, DOUBLE)
        field(NELM, 10)
        field(INP, [0, 1, 1.6e-19, 2.718, 3.141593])
    }
    record(aSub, "select") {
        field(FTA, STRING)
        field(NOA, 4)
        field(INPA, ["Zero", "One", "Two", "Three"])
        field(FTB, SHORT)
        field(NOB, 1)
        field(FTVA, STRING)
        field(NOVA, 1)
        field(SNAM, "select_asub")
    }
```

Reminder: Link initialization with constant values normally only occurs at
record initialization time. The calcout and printf record types are the only
exceptions in the Base record types to this rule, so it is generally not
useful to change a const link value after iocInit.

### Database Parsing of "Relaxed JSON" Values

A database file can now provide a "relaxed JSON" value for a database field
value or an info tag. Only a few field types can currently accept such values,
but the capability is now available for use in other places in the future.
When writing to a JSON-capable field at run-time however, only strictly
compliant JSON may be used (the dbStaticLib parser rewrites relaxed JSON
values into strict JSON before passing them to the datase for interpretation,
where the strict rules must be followed).

"Relaxed JSON" was developed to maximize compatibility with the previous
database parser rules and reduce the number of double-quotes that would be
needed for strict JSON syntax. The parser does accept strict JSON too though,
which should be used when machine-generating database files. The differences
are:

  * Strings containing only the characters `a-z A-Z 0-9 _ - + .` do not have to
be enclosed in double-quote characters.

  * The above rule applies to map keys as well as to regular string values.

  * The JSON keywords `null`, `true` and `false` (all lower-case) will be
recognized as keywords, so they must be quoted to use any of these single words
as a string.

  * Comments may be used, introduced as usual by the `#` character and extending
to the end of the line.

A JSON field or info value is only enclosed in quotes when the value being
provided is a single string, and even here the quotes can be omitted in some
cases as described above. The following shows both correct and incorrect
excerpts from a database file:

```
    record(ai, math:pi) {
        field(INP, {const: 3.14159265358979})   # Correct
        field(SIOL, "{const: 3.142857}")        # Wrong

        info(autosave, {            # White-space and comments are allowed
            fields:[DESC, SIMM],
            pass0:[VAL]
        })                          # Correct
    }
```

Note that the record, field and info-tag names do *not* accept JSON values, so
they follows the older bareword rules for quoting where the colon `:` and
several additional characters are legal in a bareword string. Only the value
(after the comma) is parsed as JSON. The autosave module has not been modified
to accept JSON syntax, the above is only an example of how JSON might be used.

### Echoless comments in iocsh

The way comments are parsed by the iocsh interpreter has changed. The
interpreter can be selectively disabled from echoing comments coming from a
script by starting those lines with `#-` rather than just `#`.

### Typed record support methods

The table of record support functions (rset methods for short) no longer has
entries of type `RECSUPFUN` (which says: any number and type of arguments).
Instead, rset methods are now typed by default. The `RECSUPFUN` typedef has
been deprecated and casts to it as well as using the untyped `struct rset`
will create compilation warnings.

Existing code (e.g. external record supports) will generate such warnings when
compiled against this version of Base, but it will work without changes.

For a conversion period, the new typed rset definitions are activated by
defining `USE_TYPED_RSET`, preferably by setting `USR_CPPFLAGS +=
-DUSE_TYPED_RSET` inside a Makefile. After activating the new typed rset in
this way and making the following changes, the result should still compile and
work properly against older versions of Base.

The first parameter of `init_record` and `process` has been changed to `struct
dbCommon *`. Record types that use `void*` here should be changed to use
`struct dbCommon*`, and cast the argument to their own `xxxRecord *`.

When compiled against this release, compiler warnings about incompatible types
for the method pointers should be taken seriously. When compiled against older
versions of base, such warnings are unavoidable.

Record types written in C++ need to take more drastic measures because of the
stricter type checking in C++. To remain compatible with older versions of
base you will need to use something like:

```
    #include "epicsVersion.h"
    #ifdef VERSION_INT
    #  if EPICS_VERSION_INT < VERSION_INT(3,16,0,2)
    #    define RECSUPFUN_CAST (RECSUPFUN)
    #  else
    #    define RECSUPFUN_CAST
    #  endif
    #else
    #  define RECSUPFUN_CAST (RECSUPFUN)
    #endif
```

and then replace `(RECSUPFUN)` with `RECSUPFUN_CAST` when initializing the
rset. Further changes might also be needed, e.g. to adapt `const`-ness of
method parameters.


## Changes made between 3.15.3 and 3.16.0.1

### Build support for CapFast and dbst removed

The build rules associated with the CapFast-related tools `sch2edif` and
`e2db` and the database optimization tool `dbst` have been removed, along with
the `DB_OPT` build configuration variable.

### compressRecord buffering order

The compressRecord has a new field `BALG` which can select between FIFO
(append) and LIFO (prepend) ordering for insertion of new elements. FIFO
ordering is the default, matching the behviour of previous versions.

### Valgrind Instrumentation

Valgrind is a software debugging suite provided by many Linux distributions.
The header valgrind/valgrind.h is now included in, and installed by, Base.
When included by a C or C++ source file this header defines some macros which
expand to provide hints to the Valgrind runtime. These have no effect on
normal operation of the software, but when run using the valgrind tool they
can help to find memory leaks and buffer overflows. Suitable hints have been
added to several free-lists within libCom, including freeListLib, allowing
valgrind to provide more accurate information about the source of potential
leaks.

valgrind.h automatically disables itself when the build target is not
supported by the valgrind tool. It can also explicitly be disabled by defining
the macro `NVALGRIND`. See `src/libCom/Makefile` for a commented-out example.

As a matter of policy valgrind.h will never be included by any header file
installed by Base, so its use will remain purely an implementation detail
hidden from application software. Support modules which choose to use
valgrind.h are advised to do likewise.

### Database Multi-locking

The IOC record locking code has been re-written with an expanded API; global
locks are no longer required by the IOC database implementation.

The new API functions center around `dbScanLockMany()`, which behaves like
`dbScanLock()` applied to an arbitrary group of records. `dbLockerAlloc()` is
used to prepare a list or record pointers, then `dbScanLockMany()` is called.
When it returns, all of the records listed may be accessed (in any order) until
`dbScanUnlockMany()` is called.

The Application Developer's Guide has been updated to describe the API and
implementation is more detail.

Previously a global mutex `lockSetModifyLock` was locked and unlocked during
`dbScanLock()`, acting as a sequencing point for otherwise unrelated calls. The
new dbLock.c implementation does not include any global mutex in `dbScanLock()`
or `dbScanLockMany()`. Locking and unlocking of unrelated lock sets is now
completely concurrent.

### Generate Version Header

A Perl script and Makefile rules have been added to allow modules to generate
a C header file with a macro defined with an automatically updated identifier.
This is a VCS revision ID (Darcs, Git, Mercurial, Subversion, and Bazaar are
supported) or the date/time of the build if no VCS system is in use.

The makeBaseApp example template has been updated with a new device support
which makes this identifier visible via a lsi (long string input) record.

### epicsTime API return status

The epicsTime routines that used to return epicsTimeERROR now return a
specific `S_time_` status value, allowing the caller to discover the reason for
any failure. The identifier `epicsTimeERROR` is no longer defined, so any
references to it in source code will no longer compile. The identifier
epicsTimeOK still exists and has the value 0 as before, so most code that uses
these APIs can be changed in a way that is backwards-compatible with the
previous return status.

Time providers that have to return a status value and still need to be built
with earlier versions of Base can define the necessary status symbols like
this:

```
    #include "epicsTime.h"

    #ifndef M_time
      /* S_time_... status values were not provided before Base 3.16 */
      #define S_time_unsynchronized epicsTimeERROR
      #define S_time_...whatever... epicsTimeERROR
    #endif
```

### Refactoring of epicsReadline

The epicsReadline code has been reorganized to allow the commandline history
editor to be disabled at runtime. The `EPICS_COMMANDLINE_LIBRARY` build setting
still selects the preferred editor, but the new `IOCSH_HISTEDIT_DISABLE`
environment variable can be set at runtime to disable history editing and make
the IOC or other program use the basic editor instead. This is useful when
starting and controlling an IOC from another program through its stdin and
stdout streams since history editors often insert invisible escape codes into
the stdout stream, making it hard to parse.

### Callback subsystem API

Added a new macro `callbackGetPriority(prio, callback)` to the callback.h
header and removed the need for dbScan.c to reach into the internals of its
`CALLBACK` objects.


# Changes incorporated from the 3.15 branch


## Changes made between 3.15.7 and 3.15.8

### Bug fixes

The following launchpad bugs have fixes included in this release:

- [lp: 1812084](https://bugs.launchpad.net/epics-base/+bug/1812084), Build
  failure on RTEMS 4.10.2
- [lp: 1829770](https://bugs.launchpad.net/epics-base/+bug/1829770), event
  record device support broken with constant INP
- [lp: 1829919](https://bugs.launchpad.net/epics-base/+bug/1829919), IOC
  segfaults when calling dbLoadRecords after iocInit
- [lp: 1838792](https://bugs.launchpad.net/epics-base/+bug/1838792), epicsCalc
  bit-wise operators on aarch64
- [lp: 1841608](https://bugs.launchpad.net/epics-base/+bug/1841608), logClient
  falsely sends error logs on all connections
- [lp: 1853168](https://bugs.launchpad.net/epics-base/+bug/1853168), undefined
  reference to `clock_gettime()`
- [lp: 1862328](https://bugs.launchpad.net/epics-base/+bug/1862328), Race
  condition on IOC start leaves rsrv unresponsive
- [lp: 1868486](https://bugs.launchpad.net/epics-base/+bug/1868486),
  epicsMessageQueue lost messages


### Improvements to the self-test build targets

This release contains changes that make it possible to integrate another test
running and reporting system (such as Google's gtest) into the EPICS build
system. The built-in test-runner and reporting system will continue to be used
by the test programs inside Base however.

These GNUmake `tapfiles` and `test-results` build targets now collect a list of
the directories that experienced test failures and display those at the end of
running and/or reporting all of the tests. The GNUmake process will also only
exit with an error status after running and/or reporting all of the test
results; previously the `-k` flag to make was needed and even that didn't always
work.

Continuous Integration systems are recommended to run `make tapfiles` (or if
they can read junittest output instead of TAP `make junitests`) followed by
`make -s test-results` to display the results of the tests. If multiple CPUs are
available the `-j` flag can be used to run tests in parallel, giving the maximum
jobs that should be allowed so `make -j4 tapfiles` for a system with 4 CPUs say.
Running many more jobs than you have CPUs is likely to be slower and is not
recommended.

### Calc Engine Fixes and Enhancements

The code that implements bit operations for Calc expressions has been reworked
to better handle some CPU architectures and compilers. As part of this work a
new operator has been added: `>>>` performs a logical right-shift, inserting
zero bits into the most significant bits (the operator `>>` is an arithmetic
right-shift which copies the sign bit as it shifts the value rightwards).

### IOC logClient Changes

The IOC's error logging system has been updated significantly to fix a number
of issues including:

  - Only send errlog messages to iocLogClient listeners
  - Try to minimize lost messages while the log server is down:
    + Detect disconnects sooner
    + Don't discard the buffer on disconnect
    + Flush the buffer immediately after a server reconnects

### epicsThread: Main thread defaults to allow blocking I/O

VxWorks IOCs (and potentially RTEMS IOCs running GeSys) have had problems with
garbled error messages from dbStaticLib routines for some time &mdash; messages
printed before `iocInit` were being queued through the errlog thread instead of
being output immediately. This has been fixed by initializing the main thread
with its `OkToBlock` flag set instead of cleared. IOCs running on other
operating systems that use iocsh to execute the startup script previously had
that set anyway in iocsh so were not affected, but this change might cause other
programs that don't use iocsh to change their behavior slightly if they use
`errlogPrintf()`, `epicsPrintf()` or `errPrintf()`.

### catools: Handle data type changes in camonitor

The camonitor program didn't properly cope if subscribed to a channel whose data
type changed when its IOC was rebooted without restarting the camonitor program.
This has now been fixed.

### More Record Reference Documentation

The remaining record types have had their reference pages moved from the Wiki,
and some new reference pages have been written to cover the analog array and
long string input and output record types plus the printf record type, none of
which were previously documented. The wiki reference pages covering the fields
common to all, input, and output record types have also been added, thanks to
Rolf Keitel. The POD conversion scripts have also been improved and they now
properly support linking to subsections in a different document, although the
POD changes to add the cross-links that appeared in the original wiki pages
still needs to be done in most cases.

### Fix build issues with newer MinGW versions

The `clock_gettime()` routine is no longer used under MinGW since newer versions
don't provide it any more.

### Fix race for port in RSRV when multiple IOCs start simultaneously

If multiple IOCs were started at the same time, by systemd say, they could race
to obtain the Channel Access TCP port number 5064. This issue has been fixed.


## Changes made between 3.15.6 and 3.15.7

### GNU Readline detection on Linux

Most Linux architectures should now configure themselves automatically to use
the GNU Readline library if its main header file can be found in the expected
place, and not try to use Readline if the header file isn't present. For older
Linux architectures where libncurses or libcurses must also be linked with, the
manual configuration of the `COMMANDLINE_LIBRARY` variable in the appropriate
`configure/os/CONFIG_SITE.Common.<arch>` file will still be necessary.

### Replace `EPICS_TIMEZONE` with `EPICS_TZ`

The `EPICS_TIMEZONE` environment parameter provided time-zone information for
the IOC's locale in the old ANSI format expected by VxWorks for its `TIMEZONE`
environment variable, and can also used by RTEMS to set its `TZ` environment
variable. However the `TIMEZONE` value has to be updated every year since it
contains the exact dates of the daylight-savings time changes. The Posix TZ
format that RTEMS uses contains rules that for calculating those dates, thus its
value would only need updating if the rules (or the locale) are changed.

This release contains changes that replace the `EPICS_TIMEZONE` environment
parameter with one called `EPICS_TZ` and a routine for VxWorks that calculates
the `TIMEZONE` environment variable from the current `TZ` value. This routine
will be run once at start-up, when the EPICS clock has synchronized to its NTP
server. The calculations it contains were worked out and donated to EPICS by
Larry Hoff in 2009; it is unforunate that it has taken 10 years for them to be
integrated into Base.

The default value for the `EPICS_TZ` environment parameter is set in the Base
`configure/CONFIG_SITE_ENV` file, which contains example settings for most EPICS
sites that use VxWorks, and a link to a page describing the Posix TZ format for
any locations that I missed.

If a VxWorks IOC runs continuously without being rebooted from December 31st to
the start of daylight savings time the following year, its `TIMEZONE` value will
be wrong as it was calculated for the previous year. This only affects times
that are converted to a string on the IOC however and is easily fixed; just run
the command `tz2timezone()` on the VxWorks shell and the calculation will be
redone for the current year. IOCs that get rebooted at least once before the
start of summer time will not need this to be done.

### Added new decimation channel filter

A new server-side filter has been added to the IOC for reducing the number
and frequency of monitor updates from a channel by a client-specified factor.
The filter's behaviour is quite simplistic, it passes the first monitor event it
sees to the client and then drops the next N-1 events before passing another
event. For example to sample a 60Hz channel at 1Hz, a 10Hz channel every 6
seconds, or a 1Hz channel once every minute:

```
    Hal$ camonitor 'test:channel.{"dec":{"n":60}}'
    ...
```

More information is included in the filters documentation, which can be found
in the `html/filters.html` document that is generated during the build.

### Imported Record Reference Documentation from Wiki

The remaining record types that had 3.14 reference documentation in the EPICS
Wiki have had that documentation converted and imported into their DBD files.
The preferred form for future updates to the record type descriptions is now an
emailed patch file, a Pull Request through GitHub, or a Merge Request through
Launchpad. Note that in some cases the behavior of a record type in a 7.0.x
release may differ from that of the same record type in a 3.15 release, although
this would be unusual, so it may be important to indicate the branch that your
changes apply to.

**NOTE:** *These documentation changes have modified the order of the fields in
some record definitions. As a result this release is not compatible with record
or device support binaries that were compiled against earlier releases.*

### `make test-results` for Windows

The make target `test-results` should now work properly on Windows. Some Perl
installations used versions of `prove.bat` that would only display the results of
up to 3 tests or didn't return an error status in the event of tests failing. The
build system now calls its own perl script to summarize the results instead of
passing a list of TAP filenames to `prove`.

### Add option to avoid CALLBACK conflict

If a macro `EPICS_NO_CALLBACK` is defined, then callback.h will no longer
(re)define CALLBACK. The name `CALLBACK` is used by the WIN32 API, and
redefinition in callback.h cause errors if some windows headers are later
included.

Code which defines `EPICS_NO_CALLBACK`, but still wishes to use callbacks,
should use the alternate name `epicsCallback` introduced in 3.15.6, 3.16.2, and
7.0.2. It is also possible, though not encouraged, to use `struct callbackPvt`
which has been present since the callback API was introduced.

### Cleaning up with Multiple CA contexts in a Process

Bruno Martins reported a problem with the CA client library at shutdown in a
process that uses multiple CA client contexts. The first context that triggers
the CA client exit handler prevents any others from being able to clean up
because it resets the ID of an internal epicsThreadPrivate variable which is
shared by all clients. This action has been removed from the client library,
which makes cleanup of clients like this possible.

### Perl CA bindings fixed for macOS Mojave

Apple removed some Perl header files from macOS Mojave that were available
in their SDK, requiring a change to the include paths used when compiling the
CA bindings. The new version should build on new and older macOS versions, and
these changes may also help other targets that have an incomplete installation
of Perl (the build will continue after printing a warning that the Perl CA
bindings could not be built).

### Routine `epicsTempName()` removed from libCom

This routine was a simple wrapper around the C89 function `tmpnam()`
which is now seen as unsafe and causes warning messages to be generated by
most modern compilers. The two internal uses of this function have been
modified to call `epicsTempFile()` instead. We were unable to find any
published code that used this function, so it was removed immediately instead
of being deprecated.

### DBD Parsing of Record Types

The Perl DBD file parser has been made slightly more liberal; the order in
which DBD files must be parsed is now more flexible, so that a record type
definition can now be parsed after a device support that referred to that
record type. A warning message will be displayed when the device support is
seen, but the subsequent loading of the record type will be accepted without
triggering an error. See
[Launchpad bug 1801145](https://bugs.launchpad.net/epics-base/+bug/1801145).

### menuScan and several record types documented with POD

The EPICS Wiki pages describing a number of standard record types has been
converted into the Perl POD documentation format and added to the DBD files,
so at build-time an HTML version of these documents is generated and installed
into the htmls directory. Thanks to Tony Pietryla.

### CA client tools learned `-V` option

This displays the version numbers of EPICS Base and the CA protocol.

## Changes made between 3.15.5 and 3.15.6

### Unsetting environment variables

The new command `epicsEnvUnset varname` can be used to
unset an environment variable.

### Warning indicators in msi (and macLib) output

The libCom macro expansion library has been modified so that when the
`SUPPRESS_WARNINGS` flag is set it will no longer include any `,undefined`
or `,recursive` indicators in its output when undefined or recursive
macros are encountered. These indicators were harmless when the output was fed
into an IOC along with a definition for the macro, but when the `msi`
tool was used to generate other kinds of files they caused problems. If the
`msi -V` flag is used the markers will still be present in the output
whenever the appropriate condition is seen.

### Improvements to msi

In addition to fixing its response to discovering parsing errors in its
substitution input file (reported as Launchpad
[bug 1503661](https://bugs.launchpad.net/epics-base/+bug/1503661))
so it now deletes the incomplete output file, the msi program has been cleaned
up a little bit internally.

### All array records now post monitors on their array-length fields

The waveform record has been posting monitors on its NORD field since Base
3.15.0.1; we finally got around to doing the equivalent in all the other
built-in record types, which even required modifying device support in some
cases. This fixes
[Launchpad bug 1730727](https://bugs.launchpad.net/epics-base/+bug/1730727).

### HOWTO: Converting Wiki Record Reference to POD

Some documentation has been added to the `dbdToHtml.pl` script
explaining how Perl POD (Plain Old Documentation) markup can be added to
`.dbd` files to generate HTML documentation for the record types. To see
these instructions, run `perl bin/<host>/dbdToHtml.pl -H`
or `perldoc bin/<host>/dbdToHtml.pl`.

### Fix problem with numeric soft events

Changing from numeric to named soft events introduced an incompatibility
when a numeric event 1-255 is converted from a DOUBLE, e.g. from a calc record.
The `post_event()` API is not marked deprecated any more.

Also `scanpel` has been modified to accept a glob pattern for
event name filtering and to show events with no connected records as well.

### Add `osiSockOptMcastLoop_t` and osiSockTest

Added a new OS-independent typedef for multicast socket options, and a test
file to check their correct operation.

### Support for `CONFIG_SITE.local` in Base

This feature is mostly meant for use by developers; configuration
settings that would normally appear in `base/configure/CONFIG_SITE` can now
be put in a locally created `base/configure/CONFIG_SITE.local` file instead
of having go modify or replace the original. A new `.gitignore` pattern
tells git to ignore all `configure/*.local` files.

### Fix broken `EPICS_IOC_LOG_FILE_LIMIT=0` setting

The Application Developers' Guide says this is allowed and disables the
limit on the log-file, but it hasn't actually worked for some time (if ever).
Note that the iocLogServer will be removed from newer Base release sometime
soon as its functionality can be implemented by other dedicated log servers
such as logstash or syslog-ng.

Fixes [lp:1786858](https://bugs.launchpad.net/bugs/1786858)
and part of [lp:1786966](https://bugs.launchpad.net/bugs/1786966).

### Cleanup of startup directory

The files in the startup directory have not been maintained in recent years
and have grown crufty (technical term). This release includes the following
updates to these files:

  - The Perl `EpicsHostArch.pl` script has been rewritten, and support
    for a few previously missing host architectures has been added to it.
  - The `EpicsHostArch.pl` script has also been moved into the standard
    `src/tools` directory, from where it will be installed into
    `lib/perl`. In this new location it is no longer executable, so it must
    be run by the `perl` executable.
  - The build system has been adjusted to look for `EpicsHostArch.pl` in
    both places if the `EPICS_HOST_ARCH` environment variable has not been
    set at build-time.
  - Sites that used the original Perl script to set `EPICS_HOST_ARCH` as part of
    their standard environment will need to adjust their scripts when they
    upgrade to this release.
  - The `EpicsHostArch` shell script has been replaced with a wrapper
    routine that calls the Perl `EpicsHostArch.pl` script. Sites that rely on
    this script to set `EPICS_HOST_ARCH` should consider switching to the
    Perl script instead.
  - The `Site.cshrc` and `Site.profile` files have been renamed to
    `unix.csh` and `unix.sh`, respectively.
  - The existing `win32.bat` file has been cleaned up and a new
    `windows.bat` file added for 64-bit targets. The contents of these files
    should be seen as examples, don't uncomment or install parts for software
    that you don't explicitly know that you need.

### Recent Apple XCode Build Issues

The latest version of XCode will not compile calls to `system()` or
`clock_settime()` for iOS targets. There were several places in Base
where these were being compiled, although there were probably never called. The
code has now been modified to permit iOS builds to complete again.

### Prevent illegal alarm severities

A check has been added to `recGblResetAlarms()` that prevents records
from getting an alarm severity higher than `INVALID_ALARM`. It is still possible
for a field like HSV to get set to a value that is not a legal alarm severity,
but the core IOC code should never copy such a value into a record's SEVR or
ACKS fields. With this fix the record's alarm severity will be limited to
`INVALID_ALARM`.

### Fixes for Launchpad bugs

The following launchpad bugs have fixes included:

  - [lp: 1786320](https://bugs.launchpad.net/epics-base/+bug/1786320), dbCa
    subscribes twice to ENUM
  - [lp: 541221](https://bugs.launchpad.net/epics-base/+bug/541221),
    `assert (pca->pgetNative)` failed in ../dbCa.c
  - [lp: 1747091](https://bugs.launchpad.net/epics-base/+bug/1747091),
    epicsTimeGetEvent() / generalTime bug
  - [lp: 1743076](https://bugs.launchpad.net/epics-base/+bug/1743076), Segfault
    in `ca_attach_context()` during exits
  - [lp: 1751380](https://bugs.launchpad.net/epics-base/+bug/1751380), Deadlock
    in `ca_clear_subscription()`
  - [lp: 1597809](https://bugs.launchpad.net/epics-base/+bug/1597809), Setting
    NAME field in DB file may break IOC
  - [lp: 1770292](https://bugs.launchpad.net/epics-base/+bug/1770292),
    `get_alarm_double()` inconsistent across record types
  - [lp: 1771298](https://bugs.launchpad.net/epics-base/+bug/1771298),
    Conversion of NaN to integer relies on undefined behavior

### Updated VxWorks Timezone settings

Removed the settings for 2017; fixed the hour of the change for MET.

### Fixed camonitor server side relative timestamps bug

Initialize the first time-stamp from the first monitor, not the client-side
current time in this configuration.

### Build changes for MSVC

Windows builds using Visual Studio 2015 and later now use the `-FS`
compiler option to allow parallel builds to work properly.

We now give the `-FC` option to tell the compiler to print absolute
paths for source files in diagnostic messages.

### Extend maximum Posix epicsEventWaitWithTimeout() delay

The Posix implementation of epicsEventWaitWithTimeout() was limiting the
timeout delay to at most 60 minutes (3600.0 seconds). This has been changed to
10 years; significantly longer maximum delays cause problems on systems where
`time_t` is still a signed 32-bit integer so cannot represent absolute
time-stamps after 2038-01-19. Our assumption is that such 32-bit systems will
have been retired before the year 2028, but some additional tests have been
added to the epicsTimeTest program to detect and fail if this assumption is
violated.

### New test-related make targets

This release adds several new make targets intended for use by developers
and Continuous Integration systems which simplify the task of running the
built-in self-test programs and viewing the results. Since these targets are
intended for limited use they can have requirements for the build host which
go beyond the standard minimum set needed to build and run Base.

#### `test-results` - Summarize test results

The new make target `test-results` will run the self-tests if
necessary to generate a TAP file for each test, then summarizes the TAP output
files in each test directory in turn, displaying the details of any failures.
This step uses the program `prove` which comes with Perl, but also needs
`cat` to be provided in the default search path so will not work on most
Windows systems.

#### `junitfiles` - Convert test results to JUnit XML Format

The new make target `junitfiles` will run the self-tests if necessary
and then convert the TAP output files into the more commonly-supported JUnit
XML format. The program that performs this conversion needs the Perl module
`XML::Generator` to have been installed.

#### `clean-tests` - Delete test result files

The new make target `clean-tests` removes any test result files from
previous test runs. It cleans both TAP and JUnit XML files.

### Fix DNS related crash on exit

The attempt to fix DNS related delays for short lived CLI programs (eg. caget)
in [lp:1527636](https://bugs.launchpad.net/epics-base/+bug/1527636) introduced a
bug which cased these short lived clients to crash on exit. This bug should now
be fixed.

### Server bind issue on Windows

When a National Instruments network variables CA server is already running on
a Windows system and an IOC or PCAS server is started, the IOC's attempt to
bind a TCP socket to the CA server port number fails, but Windows returns a
different error status value than the IOC is expecting in that circumstance
(because the National Instruments code requests exclusive use of that port,
unlike the EPICS code) so the IOC fails to start properly. The relevent EPICS
bind() checks have now been updated so the IOC will request that a dynamic port
number be allocated for this TCP socket instead when this happens.

### Checking Periodic Scan Rates

Code has been added to the IOC startup to better protect it against bad
periodic scan rates, including against locales where `.` is not
accepted as a decimal separator character. If the scan period in a menuScan
choice string cannot be parsed, the associated periodic scan thread will no
longer be started by the IOC and a warning message will be displayed at iocInit
time. The `scanppl` command will also flag the faulty menuScan value.

## Changes made between 3.15.4 and 3.15.5

### dbStatic Library Speedup and Cleanup

Loading of database files has been optimized to avoid over-proportionally
long loading times for large databases. As a part of this, the alphabetical
ordering of records instances (within a record type) has been dropped. In the
unexpected case that applications were relying on the alphabetic order, setting
`dbRecordsAbcSorted = 1` before loading the databases will retain the
old behavior.

The routine `dbRenameRecord()` has been removed, as it was intended
to be used by database configuration tools linked against a host side version
of the dbStatic library that is not being built anymore.

### Launchpad Bug-fixes

In addition to the more detailed change descriptions below, the following
Launchpad bugs have also been fixed in this release:

  - [lp:1440186](https://bugs.launchpad.net/epics-base/+bug/1440186) Crash due
    to a too small buffer being provided in `dbContextReadNotifyCache()`
  - [lp:1479316](https://bugs.launchpad.net/epics-base/+bug/1479316) Some data
    races found using Helgrind
  - [lp:1495833](https://bugs.launchpad.net/epics-base/+bug/1495833) biRecord
    prompt groups are nonsensical
  - [lp:1606848](https://bugs.launchpad.net/epics-base/+bug/1606848) WSAIoctl
    `SIO_GET_INTERFACE_LIST` failed in Windows

### Whole-Program Optimization for MS Visual Studio Targets

When using the Microsoft compilers a new build system variable is provided that
controls whether whole program optimization is used or not. For static builds
using Visual Studio 2010 this optimization must be disabled. This is controlled
in the files `configure/os/CONFIG_SITE.Common.windows-x64-static` and
`configure/os/CONFIG_SITE.Common.win32-x86-static` by setting the variable
`OPT_WHOLE_PROGRAM=NO` to override the default value `YES` that would otherwise
be used.

Note that enabling this optimization slows down the build process. It is not
possible to selectively disable this optimization, when building a particular
module say; Microsoft's linker will restart itself automatically with the
`-LTCG` flag set and display a warning if it is asked to link any object
files that were compiled with the `-GL` flag.

### Add dynamic (variable length) array support to PCAS

Dynamic array sizing support was added to the IOC server (RSRV) in the
Base-3.14.12 release, but has not until now been supported in the Portable
Channel Access Server (PCAS). Channel Access server applications using the
PCAS may not need to be modified at all; if they already push monitors with
different gdd array lengths, those variable sizes will be forwarded to any CA
clients who have requested variable length updates. The example CAS server
application has been modified to demonstrate this feature.

In implementing the above, the gdd method `gdd::put(const gdd *)` now
copies the full-sized array from the source gdd if the destination gdd is of
type array, has no allocated memory and a boundary size of 0.

### Additional epicsTime conversion

The EPICS timestamp library (epicsTime) inside libCom's OSI layer has
been extended by routines that convert from `struct tm` to the EPICS
internal `epicsTime` type, assuming UTC - i.e. without going through
the timezone mechanism. This solves issues with converting from the structured
type to the EPICS timestamp at driver level from multiple threads at a high
repetition rate, where the timezone mechanism was blocking on file access.

### MinGW Cross-builds from Linux

The build configuration files that allow cross-building of the 32-bit
win32-x86-mingw cross-target have been adjusted to default to building shared
libraries (DLLs) as this is now supported by recent MinGW compilers. The 64-bit
windows-x64-mingw cross-target was already being built that way by default. The
configuration options to tell the minGW cross-compiler to link programs with
static versions of the compiler support libraries have now been moved into the
`CONFIG_SITE.linux-x86.<target>` files.

### General Time updates

The `iocInit` code now performs a sanity check of the current time
returned by the generalTime subsystem and will print a warning if the wall-clock
time returned has not been initialized yet. This is just a warning message; when
a time provider does synchonize the IOC will subsequently pick up and use the
correct time. This check code also primes the registered event system provider
if there is one so the `epicsTimeGetEventInt()` routine will work on IOCs
that ask for event time within an interrupt service routine.

The osiClockTime provider's synchronization thread (which is only used on
some embedded targets) will now poll the other time providers at 1Hz until the
first time it manages to get a successful timestamp, after which it will poll
for updates every 60 seconds as before.

The routine `generalTimeGetExceptPriority()` was designed for use by
backup (lower priority) time providers like the osiClockTime provider which do
not have their own absolute time reference and rely on other providers for an
absolute time source. This routine no longer implements the ratchet mechanism
that prevented the time it returned from going backwards. If the backup clock's
tick-timer runs fast the synchronization of the backup time provider would never
allow it to be corrected backwards when the ratchet was in place. The regular
`epicsTimeGetCurrent()` API still uses the ratchet mechanism, so this
change will not cause the IOC to see time going backwards.

### Microsoft Visual Studio builds

The build configuration files for builds using the Microsoft compilers have been
updated, although there should be no noticable difference at most sites. One
extra compiler warning is now being suppressed for C++ code, `C4344: behavior
change: use of explicit template arguments results in ...` which is gratuitous
and was appearing frequently in builds of the EPICS V4 modules.

Cross-builds of the windows-x64 target from a win32-x86 host have been
removed as they don't actually work within the context of a single `make`
run. Significant changes to the build configuration files would be necessary for
these kinds of cross-builds to work properly, which could be done if someone
needs them (email Andrew Johnson before working on this, and see
[this stack-overflow answer](http://stackoverflow.com/questions/5807647/how-do-you-compile-32-bit-and-64-bit-applications-at-the-same-time-in-visual-stu) for a starting point).

### Bazaar keywords such as 'Revision-Id' removed

In preparation for moving to git in place of the Bazaar revision control
system we have removed all the keywords from the Base source code.

### Linux systemd service file for CA Repeater

Building this version of Base on a Linux system creates a systemd service
file suitable for starting the Channel Access Repeater under systemd. The file
will be installed into the target bin directory, from where it can be copied
into the appropriate systemd location and modified as necessary. Installation
instructions are included as comments in the file.

## Changes made between 3.15.3 and 3.15.4

### New string input device support "getenv"

A new "getenv" device support for both the stringin and lsi (long string
input) record types can be used to read the value of an environment variable
from the IOC at runtime. See base/db/softIocExit.db for sample usage.

### Build rules and `DELAY_INSTALL_LIBS`

A new order-only prerequisite build rule has been added to ensure that
library files (and DLL stubs on Windows) get installed before linking any
executables, which resolves parallel build problems on high-powered CPUs. There
are some (rare) cases though where a Makefile has to build an executable and run
it to be able to compile code for a library built by the same Makefile. With
this new build rule GNUmake will complain about a circular dependency and the
build will probably fail in those cases. To avoid this problem the failing
Makefile should set `DELAY_INSTALL_LIBS = YES` before including the
`$(TOP)/configure/RULES` file, disabling the new build rule.

### IOC environment variables and build parameters

The IOC now sets a number of environment variables at startup that provide the
version of EPICS Base it was built against (`EPICS_VERSION_...`) and its build
architecture (ARCH). In some cases this allows a single iocBoot/ioc directory to
be used to run the same IOC on several different architectures without any
changes.

There are also 3 new environment parameters (`EPICS_BUILD_...`) available that
C/C++ code can use to find out the target architecture, OS class and compiler
class it was built with. These may be useful when writing interfaces to other
languages.

### New implementation of `promptgroup`/`gui_group` field property

The mechanism behind the `promptgroup()` field property inside a record type
definition has been changed. Instead of using a fixed set of choices,
the static database access library now collects the used gui group names
while parsing DBD information. Group names should start with a two-digit number
plus space-dash-space to allow proper sorting of groups.

The include file `guigroup.h` that defined the fixed set of choices
has been deprecated. Instead, use the conversion functions between index number
and group string that have been added to dbStaticLib.

When a DBD file containing record-type descriptions is expanded, any
old-style `GUI_xxx` group names will be replaced by a new-style
string for use by the IOC. This permits an older record type to be used with
the 3.15.4 release, although eventually record types should be converted by
hand with better group names used.

### CA server configuration changes

RSRV now honors `EPICS_CAS_INTF_ADDR_LIST` and binds only to the provided list
of network interfaces. Name searches (UDP and TCP) on other network interfaces
are ignored. For example on a computer with interfaces 10.5.1.1/24, 10.5.2.1/24,
and 10.5.3.1/24, setting `EPICS_CAS_INTF_ADDR_LIST='10.5.1.1 10.5.2.1'` will
accept traffic on the .1.1 and .2.1, but ignore from .3.1

RSRV now honors `EPICS_CAS_IGNORE_ADDR_LIST` and ignores UDP messages received
from addresses in this list.

Previously, CA servers (RSRV and PCAS) would build the beacon address list using
`EPICS_CA_ADDR_LIST` if `EPICS_CAS_BEACON_ADDR_LIST` was no set. This is no
longer done. Sites depending on this should set both environment variables to
the same value.

### IPv4 multicast for name search and beacons

libca, RSRV, and PCAS may now use IPv4 multicasting for UDP traffic (name search
and beacons). This is disabled by default. To enable multicast address(s) must
be listed in `EPICS_CA_ADDR_LIST` for clients and `EPICS_CAS_INTF_ADDR_LIST` for
servers (IOCs should set both). For example:

    EPICS_CAS_INTF_ADDR_LIST='224.0.2.9' EPICS_CA_ADDR_LIST=224.0.2.9

Please note that no IPv4 multicast address is officially assigned for Channel
Access by IANA. The example 224.0.2.9 is taken from the AD-HOC Block I range.

### Moved `mlockall()` into its own epicsThread routine

Since EPICS Base 3.15.0.2 on Posix OSs the initialization of the epicsThread
subsystem has called `mlockall()` when the OS supports it and thread
priority scheduling is enabled. Doing so has caused problems in third-party
applications that call the CA client library, so the functionality has been
moved to a separate routine `epicsThreadRealtimeLock()` which will be
called by the IOC at iocInit (unless disabled by setting the global variable
`dbThreadRealtimeLock` to zero).

### Added dbQuietMacroWarnings control

When loading database files, macros get expanded even on comment lines. If a
comment contains an undefined macro, the load still continues but an error
message gets printed. For this release the error message has been changed to a
warning, but even this warning can be made less verbose by setting this new
variable to a non-zero value before loading the file, like this:

```
    var dbQuietMacroWarnings 1      iocsh
    dbQuietMacroWarnings=1          VxWorks
```

This was [Launchpad bug
541119](https://bugs.launchpad.net/bugs/541119).

## Changes from the 3.14 branch between 3.15.3 and 3.15.4

### NTP Time Provider adjusts to OS tick rate changes

Dirk Zimoch provided code that allows the NTP Time provider (used on VxWorks
and RTEMS only) to adapt to changes in the OS clock tick rate after the provider
has been initialized. Note that changing the tick rate after iocInit() is not
advisable, and that other software might still misbehave if initialized before
an OS tick rate change. This change was back-ported from the 3.15 branch.

### Making IOC `ca_get` operations atomic

When a CA client gets data from an IOC record using a compound data type such
as `DBR_TIME_DOUBLE` the value field is fetched from the database in a
separate call than the other metadata, without keeping the record locked. This
allows some other thread such as a periodic scan thread a chance to interrupt
the get operation and process the record in between. CA monitors have always
been atomic as long as the value data isn't a string or an array, but this race
condition in the CA get path has now been fixed so the record will stay locked
between the two fetch operations.

This fixes
[Launchpad bug 1581212](https://bugs.launchpad.net/epics-base/+bug/1581212),
thanks to Till Strauman and Dehong Zhang.

### New `CONFIG_SITE` variable for running self-tests

The 'make runtests' and 'make tapfiles' build targets normally only run the
self-tests for the main `EPICS_HOST_ARCH` architecture. If the host is
able to execute self-test programs for other target architectures that are being
built by the host, such as when building a `-debug` version of the host
architecture for example, the names of those other architectures can be added to
the new `CROSS_COMPILER_RUNTEST_ARCHS` variable in either the
`configure/CONFIG_SITE` file or in an appropriate
`configure/os/CONFIG_SITE.<host>.Common` file to have the test
programs for those targets be run as well.

### Additional RELEASE file checks

An additional check has been added at build-time for the contents of the
`configure/RELEASE` file(s), which will mostly only affect users of the Debian
EPICS packages published by NSLS-2. Support modules may share an install path,
but all such modules must be listed adjacent to each other in any `RELEASE`
files that point to them. For example the following will fail the new checks:

```
    AUTOSAVE = /usr/lib/epics
    ASYN = /home/mdavidsaver/asyn
    EPICS_BASE = /usr/lib/epics
```

giving the compile-time error

```
    This application's RELEASE file(s) define
        EPICS_BASE = /usr/lib/epics
    after but not adjacent to
        AUTOSAVE = /usr/lib/epics
    Module definitions that share paths must be grouped together.
    Either remove a definition, or move it to a line immediately
    above or below the other(s).
    Any non-module definitions belong in configure/CONFIG_SITE.
```

In many cases such as the one above the order of the `AUTOSAVE` and
`ASYN` lines can be swapped to let the checks pass, but if the
`AUTOSAVE` module depended on `ASYN` and hence had to appear
before it in the list this error indicates that `AUTOSAVE` should also be
built in its own private area; a shared copy would likely be incompatible with
the version of `ASYN` built in the home directory.

### String field buffer overflows

Two buffer overflow bugs that can crash the IOC have been fixed, caused by
initializing a string field with a value larger than the field size
([Launchpad bug 1563191](https://bugs.launchpad.net/bugs/1563191)).

### Fixed stack corruption bug in epicsThread C++ API

The C++ interface to the epicsThread API could corrupt the stack on thread
exit in some rare circumstances, usually at program exit. This bug has been
fixed ([Launchpad bug 1558206](https://bugs.launchpad.net/bugs/1558206)).

### RTEMS NTP Support Issue

On RTEMS the NTP Time Provider could in some circumstances get out of sync
with the server because the `osdNTPGet()` code wasn't clearing its input socket
before sending out a new request. This
([Launchpad bug 1549908](https://bugs.launchpad.net/bugs/1549908))
has now been fixed.

### CALC engine bitwise operator fixes

The bitwise operators in the CALC engine have been modified to work properly
with values that have bit 31 (0x80000000) set. This modification involved
back-porting some earlier changes from the 3.15 branch, and fixes
[Launchpad bug 1514520](https://code.launchpad.net/bugs/1514520).

### Fix `ipAddrToAsciiAsync()`: Don't try to join the daemon thread

On process exit, don't try to stop the worker thread that makes DNS lookups
asynchronous. Previously this would wait for any lookups still in progress,
delaying the exit unnecessarily. This was most obvious with catools (eg.
cainfo).
[lp:1527636](https://bugs.launchpad.net/bugs/1527636)

### Fix `epicsTime_localtime()` on Windows

Simpler versions of the `epicsTime_gmtime()` and `epicsTime_localtime()`
routines have been included in the Windows implementations, and a new test
program added. The original versions do not report DST status properly. Fixes
[Launchpad bug 1528284](https://bugs.launchpad.net/bugs/1528284).
