# Changes incorporated from the 3.16 branch

## Changes made between 3.16.1 and 3.16.2

### Launchpad Bugs

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

-----

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
(0x10\_0000\_0000\_0000 = 4503599627370496) cannot be transported over Channel
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

-----

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
