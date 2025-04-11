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
- [lp: 1852653](https://bugs.launchpad.net/bugs/1852653), `USE_TYPED_DSET`
  incompatible with C++
- [lp: 1862328](https://bugs.launchpad.net/bugs/1862328), Race condition on
  IOC start leaves rsrv unresponsive
- [lp: 1866651](https://bugs.launchpad.net/bugs/1866651), thread joinable race
- [lp: 1868486](https://bugs.launchpad.net/bugs/1868486), epicsMessageQueue
  lost messages
- [lp: 1868680](https://bugs.launchpad.net/bugs/1868680), Access Security file
  reload (asInit) fails

### `*_API` macros in EPICS headers

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
[this entry below](#type-safe-device-and-driver-support-tables) for the
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
