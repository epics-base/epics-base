# Release Notes

This document describes the changes that were included in the release of EPICS
noted below. Release entries are now provided in a separate document for each
version in the EPICS 7 series, but all are combined into a single page for
publishing on the EPICS website. Separate release documents are also included
from the older Base 3.15 and 3.16 series.

The external PVA submodules continue to maintain their own release notes files
as before, but the entries describing changes in those submodules since version
7.0.5 have been copied into the associated EPICS Release Notes files, and will
be added to new EPICS Release Notes published in the future.

## EPICS Release 7.0.8

### bi "Raw Soft Channel" use MASK

If MASK is non-zero, The raw device support will now apply MASK to the
value read into RVAL.
eg. allows extraction of a bit from an input integer.

```
record(longin, "integer") {
    field(VAL, "0xff")
}
record(bi, "bit1") {
    field(DESC, "extract bit 1")
    field(DTYP, "Raw Soft Channel")
    field(INP , "integer")
    field(MASK, "0x2")
    field(ZNAM, "Clear")
    field(ONAM, "Set")
}
```

### ANSI escapes in stderr

ANSI escape character sequences may now be printed to the stderr stream.
These escapes will appear in logs captured from that stream.
Tools which parse and/or render these logs may need to be adjusted to
either strip out the escapes, or to translate them into markup.
(see [ansi2html](https://pypi.org/project/ansi2html/) for example)

### Allow explicit append with `dbRecordsOnceOnly!=0`

Previously setting `dbRecordsOnceOnly!=0` prevented any further changes to a record via a .db file.  eg.

```
record(ai, "myrec") {}
```

`dbRecordsOnceOnly!=0` previously disallowed appending fields with either form:

```
record("*", "myrec") {} # error
record(ai, "myrec") {}  # error
```

Beginning with this release, `dbRecordsOnceOnly!=0` allows appending when explicitly intended (when record type is `"*"`).

```
record("*", "myrec") {} # allowed
record(ai, "myrec") {}  # error
```

### Add `$EPICS_CLI_TIMEOUT`

Add support for CA tools timeout from environment variable `$EPICS_CLI_TIMEOUT`
which sets the default the default timeout for `caget` et al.
The `-w` argument continues to take precedence.

### Fixed leak from a non-EPICS thread on WIN32

On Windows targets, if a thread not created by `epicsThreadCreate*()` directly
or indirectly calls an `epicsThread*()` function, a specific tracking struct
is allocated.  Prior to this release the allocation would not be `free()`d,
resulting in a memory leak.

A similar issue on POSIX targets was previously fixed.

### Change compiler for FreeBSD to clang

The default compiler for FreeBSD targets changes from GCC to clang.

### Expose `dbCreateAlias` in IOC shell

Add a new IOC shell command `dbCreateAlias` allow record aliases to be added.
Intended for use before `iocInit`.  eg. to add an alias "bar" for a record "foo".

```
dbLoadRecords("some.db") # includes: record(ai, "foo") { ...
dbCreateAlias("foo", "bar")
iocInit()
```

### dbEvent eventsRemaining missed on cancel

In some cases, RSRV may queue a subscription update, but not flush it.
This partially addresses this issue.

### subRecord on bad INP links

Previously, if a subRecord has an invalid `INP*` link, it was silently failing
(and not running the proc function).  Now the the status code returned by the
subroutine is returned from `dbProcess()`.

### COMMANDLINE\_LIBRARY fallback to GNU\_DIR

Fall back to the previous behavior when searching for `readline.h` with older compilers.

### Search for readline installed via HomeBrew.

Look for `/opt/local/include/readline` on OSX.

### Always stop worker threads

The SCAN and callback threads are now stopped during normal IOC shutdown.

### Allow runtime bypass of free list allocator

The environment variable `$EPICS_FREELIST_BYPASS` may be set to `YES` to cause the `freeListLib` functions to always call directly to `malloc()`/`free()`.  May be useful when troubleshooting some kinds of memory allocation bugs which would otherwise be "hidden".  eg. use-after-free data races.  This may also improve the results of dynamic analysis tools which are not aware of this internal free list.

### `compress` record enhancement

The compress record now supports the use of partially-filled buffers when using
any of the N-to-one algorithms. This is achieved by setting the new field `PBUF`
to `YES`.

### Extended timestamp channel filter

The `"ts"` filter can now retrieve the record's timestamp in several numeric
and string formats, some of which support full nanosecond precision.

    Hal$ caget -a test:channel
    test:channel                   2021-03-11 18:23:48.265386 42
    Hal$ caget -f9 'test:channel.{"ts": {"num": "dbl"}}'
    test:channel.{"ts": {"num": "dbl"}} 984331428.265386105
    Hal$ caget 'test:channel.{"ts": {"str": "iso"}}'
    test:channel.{"ts": {"str": "iso"}} 2021-03-11T18:23:48.265386+0100
    Hal$ caget -f1 'test:channel.{"ts": {"num": "ts"}}'
    test:channel.{"ts": {"num": "ts"}} 2 984331428.0 265386163.0

More information is included in the filters documentation, which can be found in
the `html/filters.html` document that is generated during the build

### Allow adding new error symbols at any time

`errSymbolAdd` can now be called after early initialization.

### Add conditional output (OOPT) to the longout record

The longout record can now be configured using its new OOPT and OOCH fields
to (not) write to its output link depending on the contents of VAL, in a
similar manner to the calcout record. More information can be found on the
reference page for the longout record type that accompanies this release.

This functionality was suggested in
[lp# 1398215](https://bugs.launchpad.net/epics-base/+bug/1398215) and may
be added to other output record types if the community finds it useful,
please send feedback about the feature to tech-talk.

### IOC Shell

#### Tab completion

When built with optional GNU libreadline support, the interactive IOC shell
will perform tab completion for command names as well as for some arguments
of the built-in commands. For example, the record name argument of `dbpr`,
and the path name argument of `cd`.

Externally defined commands have a limited ability to opt into completion by
using the new `iocshArgStringRecord` and `iocshArgStringPath` argument types.
Both function identically to `iocshArgString` but indicate how to suggest
completion strings.

Builds on macOS (`darwin-x86` or `darwin-aarch64` targets) normally use Apple's
libedit library in readline compatibility mode, which doesn't support the tab
completion API that GNU readline provides. You can use Homebrew or some other
third-party package manager to install the GNU readline package, then edit the
`configure/os/CONFIG_SITE.darwinCommon.darwinCommon` file to have EPICS use the
real thing to get tab completion in the IOC shell. The default settings in that
file currently look for and use a Homebrew-installed readline if present.

#### Persist history

Attempt to read and write command to a file (`./.iocsh_history` by default).
Name may be overwritten with by setting `$EPICS_IOCSH_HISTFILE` to an
alternate path, or disabled by setting to an empty string.

#### Changes to help output

Rework the `help` command output to improve formatting and readability,
and include a visual marker (a line of underlines) between different help commands.

### Add FMOD as CALC Expression

The floating point modulo function `FMOD(NUM,DEN)` has been added to the CALC
expression engine and is available to all software using that (calc and calcout
record types, access security library and some extensions).

### Submodule updates

The pvData module was updated to version 8.0.6:

- Compatible changes
  - Actually enable JSON-5 output in PVStructure::Formatter::JSON when available.
  - Fix unaligned access issues for some ARM/Linux targets.

The pvAccess module was updated to version 7.1.7:

- Changes
  - Registering the PVA server with the IOC now sets the `PVAS_SERVER_PORT`
    variable in the environment.

The pva2pva module was updated to version 1.4.1:

- Bug Fixes
  - `dbLoadGroup` was fixed
- Additions
  - Support for "meta" member at top of array of structs

The pvDatabase module was updated to version 4.7.1:

* Added data distributor plugin which can be used for distributing data between
  a group of clients. The plugin is triggered by the request string of the
  form:

  `_[distributor=group:<group id>;set:<set_id>;trigger:<field_name>;updates:<n_updates>;mode:<update_mode>]`

  The plugin parameters are optional and are described below:

  - group: this parameter indicates a group that client application belongs to (default value: "default"); groups of clients are completely independent of each other

  - set: this parameter designates a client set that application belongs to within its group (default value: "default")

  - trigger: this is the PV structure field that distinguishes different channel updates (default value: "timeStamp"); for example, for area detector images one could use the "uniqueId" field of the NTND structure

  - updates: this parameter configures how many sequential updates a client (or a set of clients) will receive before the data distributor starts updating the next one (default value: "1")

  - mode: this parameter configures how channel updates are to be distributed between clients in a set:
    - one: update goes to one client per set
    - all: update goes to all clients in a set
    - default is "one" if client set id is not specified, and "all" if set id is specified
