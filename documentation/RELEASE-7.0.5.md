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

## EPICS Release 7.0.5

### Fix aai's Device Support Initialization

Krisztian Loki [reported](https://github.com/epics-base/epics-base/issues/97)
segfaults occurring when a Soft Channel aai record INP field was a DB link to
an array field of a compress record. This was caused by the aai record's
pass-0 device support initialization clashing with the semantics of the new
link support API.

The aai record
[has been modified](https://github.com/epics-base/epics-base/pull/114) to
allow the Soft Channel device support to request a pass-1 initialization
callback. See the Device Support section of the Array Analogue Input Record
Reference pages in this release for the API changes, which are fully backwards
compatible for existing aai device support.

### Prevent default DTYPs from changing

[Kay Kasemir reported](https://bugs.launchpad.net/epics-base/+bug/1908305) that
it is possible to change the Base record type's default DTYP if a `device()`
entry is seen before the `recordtype()` definition to which it refers. The
default DTYP is the first device loaded, which is normally the `Soft Channel`
support from Base. A warning was being displayed by dbdExpand when a `device()`
entry was see first, but that was easily missed.

The DBD file parser in dbdExpand.pl has now been modified to make this an error,
although the registerRecordDeviceDriver.pl script will still accept `device()`
entries without having their `recordtype()` loaded since this is necessary to
compile device supports as loadable modules.


### Priority inversion safe Posix mutexes

On Posix systems, epicsMutex now support priority inheritance if available.
The IOC needs to run with `SCHED_FIFO` engaged to use these.
Support for Posix implementations before POSIX.1-2001 (`_XOPEN_SOURCE < 500`,
glibc version &lt; 2.3.3) has been dropped.

The IOC shell's `epicsMutexShowAll` command prints "PI is enabled" if both
libc and kernel support is present.

### Fix for Periodic Scan threads hanging on Windows

Since 7.0.3.1 a Windows IOC could not run for more than 49.7 days; at that
time the periodic scan threads would stop processing. This issue should now
have been fixed and the Monotonic time functions on Windows should return
values which count at nanosecond resolution. However we have not waited 49.7
days to test the final software, so there is a small chance that it's still
broken.

This fixes [lauchpad bug #1896295](https://bugs.launchpad.net/bugs/1896295).

### Support for Apple M1/M2 (arm64) Processors

Thanks to Jeong Han Lee this release comes with build support for Apple's new
M1/M2 CPUs running macOS, using the target name `darwin-aarch64`.

It should also be possible to build universal binaries containing code for
both the Intel and arm64 processors under either target name: In the
appropriate `configure/os/CONFIG_SITE.Common.darwin-*` file add the other
architecture class name to the `ARCH_CLASS` variable (after a space).

### New String Comparison Routine `epicsStrSimilarity()`

The new `epicsStrSimilarity()` routine in epicsString.h uses a modified
Levenshtein distance to compare two strings, with a character case difference
being half the weight of a full substitution. The double return value falls in
the range 0.0 (identical) through 1.0 (no characters matching), or -1.0 for
error. This is used to provide a new "Did you mean ..." suggestion when a .db
file provides an invalid choice string for a `DBF_MENU` or `DBF_DEVICE` field.

### Build System: New `VALID_BUILDS` type "Command"

Target architectures that support command-line programs that run the `main()`
routine can now be marked as such in their `VALID_BUILDS` definition. This
enables a new set of Makefile target variables `PROD_CMD` (similar to
`PROD_HOST`), `LIBRARY_CMD` (like `LIBRARY_HOST`, etc.), `LOADABLE_LIBRARY_CMD`,
`OBJS_CMD`, `SCRIPTS_CMD`, `TARGETS_CMD`, `TESTLIBRARY_CMD`, `TESTSCRIPTS_CMD`
and `TESTPROD_CMD`. The CA client tools and programs such as `caRepeater` are now built for all such targets (previously they were built for all targets except where the OS was VxWorks, RTEMS and iOS).

If you have created your own site-specific target architectures you may need to
update the `VALID_BUILDS` variable if it gets set in your locally added
`configure/os/CONFIG.Common.<arch>` files. This is usually only needed for
cross-compiled targets though since `CONFIG.Common.UnixCommon` sets it.

The other `VALID_BUILDS` types are "Host" for target architectures that can
compile and run their own programs (`PROD_HOST` etc.), and "Ioc" for targets
that can run IOCs (`PROD_IOC` etc.).

### Support for JSON5

The YAJL parser and generator routines in libcom and in the IOC's dbStatic
parser now support the JSON5 standard. This adds various features to JSON
without altering the API for the code other than adding a new option to the
YAJL parser which can be used to disable JSON5 support if desired. The new
features include:

- The ability to handle numeric values `Infinity`, `-Infinity` and `NaN`.
- String values and map keys may be enclosed in single quotes `'`, inside which
  the double-quote character `"` doesn't have to be escaped with a back-slash
  `\`, although a single-quote character `'` (or apostrophy) must be escaped
  inside a single-quoted string.
- Numbers may start with a plus sign, `+`.
- Integers may be expressed in hexadecimal with a leading `0x` or `0X`.
- Floating-point numbers may start or end with their decimal point `.`
  (after the sign or before the exponent respectively if present).
- Map keys that match the regex `[A-Za-z_][A-Za-z_0-9]*` don't have to be
  enclosed in quotes at all. The dbStatic parser adds `.+-` to the characters
  allowed but will add quotes around such keys before passing them to YAJL.
- Arrays and maps allow a comma before the closing bracket/brace character.
- The YAJL parser will elide a backslash followed by a newline characters from
  a string value. The dbStatic parser doesn't allow that however.

Code that must also compile against the older API can use the new C macro
`HAS_JSON5` to detect the new version. This macro is defined on including
either the `yajl_parse.h` or `yajl_gen.h` headers, which also provide the
new configuration options to turn on JSON5 support.

All APIs in the IOC that previously accepted JSON will now accept JSON5.
This includes JSON field modifiers (channel filters), JSON link addresses,
constant input link array values and database info-tag values. JSON values
that get parsed by the dbLoadRecords() routine are still more liberal than
the other uses as the ability to use unquoted strings that was called
"relaxed JSON" is still supported, whereas the JSON5 standard and the YAJL
parser only allow unquoted strings to be used for keys in a JSON map.

This also fixes [lauchpad bug #1714455](https://bugs.launchpad.net/bugs/1714455).


### Character Escape Changes

- The libCom routines `epicsStrnRawFromEscaped()` and `dbTranslateEscape()`
  declared in epicsString.h no longer accept octal escaped characters such as
  `\123` or `\41`.
- The routine `epicsStrnEscapedFromRaw()` now generates hex
  escaped characters for unprintable characters such as `\x1f`.
- Hex escape character sequences `\xXX` must now contain exactly 2 hex digits.
- An escape sequence `\0` now generates a zero byte in the raw string, but the
  other digits `1-9` should not appear after a back-slash.

These changes are to more closely follow the JSON5 standard, which doesn't
support octal character escapes or the `\a` (Bel, `\x07`) escape sequence.

### Filters in database input links

Input database links can now use channel filters, it is not necessary to
make them CA links for the filters to work.

### ai Soft Channel support

The Soft Channel device support for ai records now returns failure when
fetching the INP link fails.

### Support for zero-length arrays

Several modifications have been made to properly support zero-length
array values inside the IOC and over Channel Access. Some of these changes
may affect external code that interfaces with the IOC, either directly or
over the CA client API so we recommend thorough testing of any external
code that handles array fields when upgrading to this release.

Since these changes affect the Channel Access client-side API they will
require rebuilding any CA Gateways against this version or Base to
properly handle zero-length arrays. The `caget`, `caput` and `camonitor`
client programs are known to work with empty arrays as long as they were
built with this or a later version of EPICS.

#### Change to the db\_access.h `dbr_size_n(TYPE, COUNT)` macro

When called with COUNT=0 this macro no longer returns the number of bytes
required for a scalar (1 element) but for an empty array (0 elements).
Make sure code that uses this doesn't call it with COUNT=0 when it really
means COUNT=1.

Note that the db\_access.h header file is included by cadef.h so the change
can impact Channel Access client programs that use this macro.

#### Channel Access support for zero-length arrays

The `ca_array_put()` and `ca_array_put_callback()` routines now accept an
element count of zero, and will write a zero-length array to the PV if
possible. No error will be raised if the target is a scalar field though,
and the field's value will not be changed.

The `ca_array_get_callback()` and `ca_create_subscription()` routines
still accept a count of zero to mean fetch as many elements as the PV
currently holds.

Client programs should be prepared for the `count` fields of any
`struct event_handler_args` or `struct exception_handler_args` passed to
their callback routines to be zero.

#### Array records

The soft device support for the array records aai, waveform, and subArray
as well as the aSub record type now correctly report reading 0 elements
when getting an empty array from an input link.

#### Array support for dbpf

The dbpf command now accepts array values, including empty arrays, when
provided as a JSON string. This must be enclosed in quotes so the iocsh
argument parser sees the JSON as a single argument:

```
epics> dbpf wf10:i32 '[1, 2, 3, 4, 5]'
DBF_LONG[5]:        1 = 0x1   2 = 0x2   3 = 0x3   4 = 0x4   5 = 0x5
```

#### Reading empty arrays as scalar values

Record links that get a scalar value from an array that is currently
empty will cause the record that has the link field to be set to an
`INVALID/LINK` alarm status.
The record code must call `dbGetLink()` with `pnRequest=NULL` for it to
be recognized as a request for a scalar value though.

This changes the semantics of passing `pnRequest=NULL` to `dbGetLink()`,
which now behaves differently than passing it a pointer to a long integer
containing the value 1, which was previously equivalent.
The latter can successfully fetch a zero-element array without triggering
a LINK alarm.

#### Writing empty arrays to scalar fields

Record links that put a zero-element array into a scalar field will now set
the target record to `INVALID/LINK` alarm without changing the field's value.
Previously the field was set to 0 in this case (with no alarm).
The target field must be marked as `special(SPC_DBADDR)` to be recognized
as an array field, and its record support must define a `put_array_info()`
routine.

### Timestamp before processing output links

The record processing code for records with output links has been modified to
update the timestamp via recGblGetTimeStamp() _before_ processing the output
links.  This ensures that other records which get processed via an output link
can use TSEL links to fetch the timestamp corresponding to the data processed
by the output link.

This change could result in a slightly earlier timestamp for records whose
output link is handled by a device driver, but only if the device driver does
not handle its own timestamping via TSE -2 and instead uses TSE 0 or TSE -1 to
get current time or best time, and the time spent in the device driver is
greater than your timestamp provider resolution.  For these situations it is
recommended to set TSE to -2 and set the timestamp in the driver code.

### Add registerAllRecordDeviceDrivers()

A new iocsh command `registerAllRecordDeviceDrivers` is provided and also
defined as a function in iocshRegisterCommon.h. This uses dynamic symbol
lookup with `epicsFindSymbol()` to perform the same function as a generated
`*_registerRecordDeviceDriver()` function. This allows for an alternative
approach to dynamic loading of support modules without code generation.

This feature is not intended for use by IOCs constructed using the standard
EPICS application build process and booted from a startup script in an iocBoot
subdirectory, although it might work in some of those cases &mdash; the
generated registerRecordDeviceDriver.cpp file is normally required to link
everything referred to in the DBD file into the IOC's executable. It also
won't work with some static build configurations, or if the symbol table has
been stripped from the executable.

### Using a `{const:"string"}` to initialize an array of `DBF_CHAR`

It is now possible to use a JSON Const link with a string value to initialize
an aai or waveform record that has `FTVL` set to `CHAR` through the INP link.
The string length is not limited to 40 characters. This should also work for
aSub record inputs similarly configured as long strings.

```
  record(waveform, "wf") {
    field(NELM, 100)
    field(FTVL, CHAR)
    field(INP, {const:"This is a waveform and more than 40 characters"})
  }
```

### RELEASE files may use `undefine`

GNUmake added the directive `undefine` in version 3.82 to allow variables to
be undefined. Support for this has been added to the EPICS Release file parser,
so `undefine` can now be used in configure/RELEASE files to unset variables.


### Submodule updates

The pvData module was updated to version 8.0.4:

- Incompatible changes
 - Remove `ByteBuffer::align()`
- Compatible changes
 - Deprecate `SerializableControl::alignBuffer()` and
   `DeserializableControl::alignData()`
 - `shared_vector_convert<>()` fix convert of empty, untyped, array

The pvAccess module was updated to version 7.1.3:

- Bug fixes
  - Increase default TCP timeout to 40 seconds.
    Applies a 4/3 multiplier on `$EPICS_PVA_CONN_TMO` for compatibility.
  - CA Provider implementation restructured to simplify, reduce duplication
    and fix issues #163 and #165.
- Changes
  - Enable building of pvtools to all except vxWorks, RTEMS and iOS.

The pva2pva module was updated to version 1.3.0:

- Changes
 - Add `dbLoadGroup()` iocsh function to read group JSON definitions
   from a file.  Mappings in files must refer to full record names
   instead of fields.  eg. 'recname.VAL' instead of 'VAL'.
