# Release Notes

This document describes changes that were included in the release of
EPICS listed below.
Notes from earlier EPICS releases are now provided in a separate document for
each version in the EPICS 7 series to date.
Release documents are also included for the older Base 3.15 and 3.16 series.

The external PVA submodules continue to maintain their own release notes files
as before, but the entries describing changes in those submodules since version
7.0.5 have been copied into the associated EPICS Release Notes files, and will
be added to new EPICS Release Notes published in the future.

## EPICS Release 7.0.7

### Doxygen Annotations

Thanks to several attendees at the 2022 EPICS Codeathon the number of header
files with Doxygen annotations in the EPICS Core has again increased.

### Build System updates

The top-level make targets `uninstall`, `archuninstall` and similar no
longer trigger the `clean` target which empties build directories, this
was a bug introduced in 7.0.5.

The `make distclean` target now properly deletes the generated file(s)
`modules/RELEASE.<host>.local` which are essential to build the external
submodules under the `modules` directory, and should not crash if the
build is configured with `INSTALL_LOCATION` pointing to an empty external directory (i.e. if you run `make distclean` twice in succession). When
`INSTALL_LOCATION` is set in the files `configure/CONFIG_SITE` or
`configure/CONFIG_SITE.local` the `modules/RELEASE.<host>.local` file
will now be regenerated in case the install path has been modified.

Note that passing `INSTALL_LOCATION=<path>` on the make command-line will
only work if you have run `make distclean` immediately beforehand, as the
`modules/RELEASE.<host>.local` file must be recreated using the new path.

### Enhancements to `capr.pl`

The `capr.pl` script can now display records from older Base versions to
which fields have since been added, and shows long strings and array data
up to 10 elements, use the new `-n` option to increase that number.
The script is fully event-driven and prints all the field data received by
the end of the CA wait time (`-w` option which defaults to 2 seconds).
The interest level can now be specified using the `-l` option before the
PV name, and the new `-D` flag outputs debugging information.

### Time Synchronization on VxWorks

VxWorks 6.9 can do its own OS clock time synchronization, if it has been
configured by setting `SNTPC_PRIMARY_IPV4_ADDR`. Since EPICS 3.15.3 the
IOC time support code has checked for the existence of the VxWorks time
synchronization task and avoided starting the EPICS one if the OS task
exists and the OS clock gives a "recent" time (i.e. after when EPICS was
compiled), unless the environment variable `EPICS_TS_FORCE_NTPTIME` is
also set. However a logic error in that code required the environment
variable to be set in more cases than it should have.

This error has been fixed and the IOC should work normally if the VxWorks
task is configured and running. The `TIMEZONE` value for the year is also
now calculated at initialization in this configuration, previously it was
only done when the IOC synchronzation task was used. Setting the above
environment variable will now cause the IOC support code to shut down the
VxWorks synchronization thread (if running) before starting the EPICS one.

Running the iocsh command `ClockTime_Report` now shows whether the VxWorks
task is running as well as giving the state of the IOC synchronization task.
The `ClockTime_Init` command can also be used to stop or restart the IOC
time synchronization task while the IOC is running, depending on the `0` or
`1` parameter passed to it. This last change also applies to RTEMS IOCs.

### Incompatible change to `struct db_field_log`

This change may cause channel filters which manipulate array updates
to fail to compile.

To avoid potential speculation issues arising from overlapping code pointers
with data values, `union dbfl_ref` is modified to remove the `dtor` member.
`dtor` is moved out into the enclosing `struct db_field_log`.

So eg. using a `db_field_log* p`, the expression `p->u.r.dtor` must be
changed to `(p)->dtor`.

### Fix undef ts on first camonitor update of NORD from waveformRecord

The order over operations when processing a waveformRecord is adjusted
so that updates to NORD is posted with the correct timestamp.

### Automatic `COMMANDLINE_LIBRARY` with newer compilers

When built with a compiler supporting `__has_include<>`, the presence
of a `readline/readline.h` header will be used to automatically determine
a default value for `COMMANDLINE_LIBRARY`.

Mingw builds with readline support now link `-ltermcap` instead of `-lcurses`.

This should not effect sites which set explicitly set `COMMANDLINE_LIBRARY`
as the only definition in Base now has the form `COMMANDLINE_LIBRARY ?= ...`.

### Perl CA support for empty long strings

The Perl CA bindings have been fixed to handle zero-length long string data
properly.

### `aao` gains `OMSL` and `DOL`

The `aao` record types gains the same `DOL` functionality found
in other output record types (`ao`, `longout`, etc.)

### Server exports `RSRV_SERVER_PORT`

During `iocInit()`, the environment variable `RSRV_SERVER_PORT` is set
with the TCP port number selected.

### `dbdExpand.pl` sorts all items by name

DBD files generated by the `dbdExpand.pl` script are now sorted within each
item type by the primary name of the item. The result should resolve any
issues with reproducable builds. No option is provided to prevent the sorting,
previously the order was essentially random and varied each time.

### `dbExpand.pl` sorts records by name

Records are now output by this program in order, sorted by name. The new flag
`-s` can be given to output the records in the same order they were read in,
instead of sorting them.

Note that there are currently no build rules provided with Base which make use
of this program.

### Simulation Mode RAW Support for Output Record Types

SIMM=RAW support has been added for the relevant output record types
(ao, bo, mbbo, mbboDirect).
RAW simulation mode will have those records do the appropriate conversion
and write RVAL to the location pointed to by SIOL.

### Fixed leak from a non-EPICS thread

On some targets, if a thread not created by `epicsThreadCreate*()` directly
or indirectly calls an `epicsThread*()` function, a specific tracking struct
is allocated.

Prior to this release, on POSIX and WIN32 targets, this
struct would not be `free()`d, resulting in a memory leak.

This release fixed the leak on POSIX targets.

See the associated github [issue 241](https://github.com/epics-base/epics-base/issues/241)
for WIN32 status.

### Fixed leak from a non-EPICS thread

On some targets, if a thread not created by `epicsThreadCreate*()` directly
or indirectly calls an `epicsThread*()` function, a specific tracking struct
is allocated.

Prior to this release, on POSIX and WIN32 targets, this
allocation would not be `free()`d, resulting in a memory leak.

This release fixed the leak on POSIX and WIN32 targets (excluding
MSVC before vs2012, and the WINE runtime).

### Fixed leak from a non-EPICS thread

On some targets, if a thread not created by `epicsThreadCreate*()` directly
or indirectly calls an `epicsThread*()` function, a specific tracking struct
is allocated.

Prior to this release, on POSIX and WIN32 targets, this
struct would not be `free()`d, resulting in a memory leak.

This release fixed the leak on POSIX targets.

See the associated github [issue 241](https://github.com/epics-base/epics-base/issues/241)
for WIN32 status.

### Fix `CHECK_RELEASE = WARN`

This now works again, it was broken in 2019 (7.0.3.1) by an errant commit.

### Document `DISP` as design-time field

The DISP field can be set to a non-zero value to prevent records being changed
from outside the IOC (this is ancient behavior), but has never been documented
as being usable at design-time (DCT=Yes in the Record Reference tables). This
has now been changed.

### Make `epicsInt8` signed on all architectures

The `epicsInt8` and thus `DBF_CHAR` types have always been unsigned on
architectures where `char` is unsigned, for example on many PowerPC CPU
architectures. This was counter-intuitive, and resulted in IOC behavior
differing between architectures when converting `DBF_CHAR` values into a
signed integer or floating point type.

**WARNING**: This fix may change behavior of existing databases on target
architectures with unsigned `char` (mainly PowerPC) when using input links to
read from `CHAR` arrays. Architectures with signed `char` (usually x86) should
be unaffected, although some compilers might generate new warnings.

### Allow hexadecimal and octal numbers in hardware links

[GH:213](https://github.com/epics-base/epics-base/pull/213)

Several types of hardware links (`VME_IO`, `CAMAC_IO`, etc) now accept
hexadecimal and octal numbers. (Hexadecimal numbers had already been valid
up to EPICS R3.15.) This change may introduce incompatibilities when using
numbers with leading `0` as they will now be parsed as octal.

### Fix embedded implementations of `epicsEvent`

[GH:202](https://github.com/epics-base/epics-base/issues/202) and
[GH:206](https://github.com/epics-base/epics-base/pull/206)

Heinz Junkes provided a new implementation of the `epicsEvent` API suitable for
RTEMS Posix targets (RTEMS 5.1 and later). In review a few issues related to
overflow of timeout values surfaced in this and other embedded implementations,
and these were also been fixed in this Pull Request. The API documentation for
this and some other routines has also been updated.

### Breakpoint Table Names

The names of breakpoint tables were made unnecessarily strict when DBD file
processing was moved to Perl for the 3.15 release series. Table names may now
contain the special characters `_` `-` `:` `;` `.` `[` `]` `<` `>` in addition
to letters and digits.

### Fix for `undefined` in configure/RELEASE files

Prevents `Use of uninitialized value` warnings from convertRelease.pl.

### Colorized Messages for errlog

Many internal error messages now emit ANSI escape sequences to highlight the
words "ERROR" and "WARNING" in an attempt to make occurrences more noticeable
during IOC startup.

The macros `ERL_ERROR` and `ERL_WARNING` are defined for external usage,
and expand as string constants.  eg.

```c
#include <errlog.h>
#ifndef ERL_ERROR
#  define ERL_ERROR "ERROR"
#endif
void fn() {
   ...
   errlogPrintf(ERL_ERROR ": something bad happens :(\n");
```

ANSI escapes are automatically removed from errlog output not destined
for a terminal.  For example, for logClient, if stderr is redirected,
or if unsupported (`$TERM` not set, or Windows < 10).

### `dbnd` filter pass through `DBE_ALARM|DBE_PROPERTY`

The `dbnd` server side filter now passes through alarm and property
change events, even when not exceeding the deadband.

### Submodule updates

The pvData module was updated to version 8.0.5:

- Compatible changes
 - Internal changes to use the YAJL API for generating JSON and JSON-5 output.

The pvAccess module was updated to version 7.1.6:

- Changes to caProvider
  - Bug fix related to enum values.
  - More internal changes to improve performance when connecting tens of
    thousands of CA channels.
- Several minor internal improvements.

The pva2pva module was updated to version 1.4.0:

- Bug Fixes
 - Apply ACF when writing to atomic group
- Additions
 - Add new "structure" to @ref qsrv_group_map_types
- Changes
 - Add Access Security hooks for single and group writes.
 - Enable "Async Soft Channel" for output links
 - When built against Base 7.0.6.1, set timeStamp.userTag from UTAG field.
 - Add DTYP="QSRV Set UTag" for longin, which sets UTAG=VAL.

The pvDatabase module was updated to version 4.7.0:

* Added support for the whole structure (master field) server side plugins.
  The whole structure is identified as the `_` string, and a pvRequest string
  that applies a plugin to it takes the form:

  `field(_[XYZ=A:3;B:uniqueId])`

  where `XYZ` is the name of a specific filter plugin that takes parameters
  `A` and `B` with values `3` and `uniqueId` respectively.
