# Release Notes

This document describes the changes that were included in the release of EPICS
noted below. Release entries are now provided in a separate document for each
version in the EPICS 7 series, but all are combined into a single page for
publishing on the EPICS website. Separate release documents are also included
from the older Base 3.15 and 3.16 series.

The external PVA submodules continue to maintain their own release notes files
as before, but the entries describing changes in those submodules since version
7.0.5 have been copied into the associated EPICS Release Notes files; they will
also be manually added to new EPICS Release Notes published in the future.


## EPICS Release 7.0.10

### Reduce symbol and macro pollution from epicsAtomic.h on WIN32

`epicsAtomic.h` no longer pulls in as many unneeded declarations and macros from
`windows.h`. Prior to this change, including `epicsAtomic.h` at the wrong time
could result in unexpected compiler errors. Due to the nature of `windows.h`,
some unneeded declarations are still pulled in, however the number is greatly reduced.
Code that needs these declarations should explicitly include `windows.h` before `epicsAtomic.h`.

### New `afterIocRunning` IOC Shell Command Added

This release incorporates [PR #558](https://github.com/epics-base/epics-base/pull/558) which added a new IOC shell command `afterIocRunning`. This command allows startup scripts to schedule arbitrary commands to be executed automatically after the IOC initialization phase (`iocInit`).

`afterIocRunning` allows you to write better-structured IOC shell files to include in your startup scripts without tracking where `iocInit` is located (and how IOC is deployed) e.g.:
- to achieve the best maintainability (e.g. encapsulation of the context into one file),
- to improve writing boot sequences,
- to improve IOC startup flexibility and scripting capabilities,
- it replaces the community [`afterInit`](https://github.com/paulscherrerinstitute/iocsh_utilities/blob/master/afterInit.c) and [`doAfterIocInit`](https://github.com/epics-modules/std/blob/master/stdApp/src/delayCmd.cpp) IOC shell commands,
- community usage examples:
  - [`pf4filters.iocsh`](https://github.com/epics-modules/optics/blob/master/opticsApp/iocsh/pf4filters.iocsh) - enable/disable with a single comment ([full description](https://github.com/epics-base/epics-base/pull/558#issuecomment-2430057167))
  - [ALS-U autosave management](https://github.com/epics-base/epics-base/pull/558#issuecomment-2430447220)

#### Features

- Define commands that run after IOC initialization completes.
- Executes following `iocInit` and `autosave` initialization (important for proper PV configuration).
- Supports any valid IOC shell command as an argument.
- Example usages:
  - `afterIocRunning "dbpf <PV> <VAL>"`
  - `afterIocRunning "date"`
  - `afterIocRunning "dbpf $(P)EvtClkSource-Sel 'Upstream (fanout)'"`
  - `afterIocRunning "dbpf $(P)Enable-Sel Enabled"`

### fdManager file descriptor limit removed

In order to support file descriptors above 1023, fdManager now uses
`poll()` instead of `select()` on all architectures that support it
(Linux, MacOS, Windows, newer RTEMS).

### New `dbServerStats()` API for iocStats

A new routine provides the ability to request channel and client counts from
named server layers that implement the `stats()` method, or to get a summary
of the counts from all registered server layers. A preprocessor macro
`HAS_DBSERVER_STATS` macro is defined in the `dbServer.h` header file to
simplify code that needs to support older versions of Base as well.

### epicsExport simplifications

`epicsExportAddress()`, `epicsExportRegistrar()` and `epicsRegisterFunction()`
no longer require to be wrapped in `extern "C" { }` in C++ code.

### Support for multiline strings in iocsh files

GitHub [PR #603](https://github.com/epics-base/epics-base/pull/603)

This update introduces support for multiline strings in IOC shell (iocsh) files.
Previously, string values in iocsh files were limited to a single line, making
it difficult to include longer or formatted text. With this change, users can
now define strings that span multiple lines, improving readability and
flexibility when configuring IOC shell scripts.

To create a multiline string, end a line with a backslash (`\`). The following
line, including any leading whitespace, will be joined to the previous line.
If the backslash is immediately followed by any character other than a newline,
it will not be treated as a multiline continuation.

### Enhancement to IOC `dbgrep` command

`dbgrep` now takes an optional second string argument consisting of a list of field names
separated by spaces, e.g. `dbgrep "*PRESSURE*", "VAL DESC"`

### ACF Syntax Forward Compatibility

This release modifies the Access Security Configuration File (ACF) parser to
**standardize the ACF grammar for forward compatibility**.
It does not change the syntax that was accepted by earlier versions of the parser,
so **existing access security configuration files will not need to be modified.**
All ACF definitions will adhere to a consistent syntax format,
which will allow future additions to the access security language
without breaking existing configurations.
In practice, this means the structure of ACF files is now formally defined
and will remain stable going forward,
so any new grammar features will fit into the same pattern.
(Existing ACF files continue to work as-is under the new parser,
so no changes are required for legacy configurations or tools.).

**Generic ACF Syntax:**
The ACF file consists of definitions for User Access Groups (UAG),
Host Access Groups (HAG),
and Access Security Groups (ASG),
using the following general format
(angle brackets below denote placeholders):

```text

UAG(<name>) [{ <user> [, <user> ...] }]
...

HAG(<name>) [{ <host> [, <host> ...] }]
...

ASG(<name>) [{
    [INP<index>(<pvname>)
     ...]

    RULE(<level>, NONE | READ | WRITE [, NOTRAPWRITE | TRAPWRITE]) {
        [UAG(<name> [, <name> ...])]
        [HAG(<name> [, <name> ...])]
        [CALC(<calculation>)]
    }
    ...
}]
...

```

Under this schema each definition comprises a keyword,
a name in parentheses,
and (optionally) a braced block of contents.
This uniform structure ensures that
**future keywords or sections**
can be introduced in the same form,
maintaining compatibility with the parser.
For example, if a new type of condition or group is added in a later release,
it would follow the `KEYWORD(name) { ... }` pattern,
so 7.0.10-era parsers can handle or ignore it gracefully
instead of failing on unknown syntax.

**Supported Syntax in EPICS 7.0.10:**
The current release defines the following specific elements
within the above generic format:

-   **UAG** -- *User Access Group*.
Defines a group of user names.
-   **HAG** -- *Host Access Group*.
Defines a group of host names
(or IP addresses) that clients can connect from.
-   **ASG** -- *Access Security Group*.
Defines a security group which records can be assigned to.
An ASG entry may contain a block with input definitions and access rules.
For example:

```text
ASG(MyGroup) {
    INPA(myPV1)
    INPB(myPV2)
    RULE(1, WRITE) { ... }
    RULE(1, READ) { ... }
}
```

If no rules are defined for an ASG,
the access permissions default to always allowed.

-   **INP<index>(<pvname>)** -- *Input link*.
Declares an input process variable whose value can be used in a CALC condition.
-   **RULE(<level>, <permission> [, <logOption>]) { ... }** --
Defines an access rule for the ASG.

Inside the curly braces of a RULE,
**optional conditions** can restrict when that rule applies.
All conditions that are present must be satisfied
(they function as a logical AND):

-   **UAG(<name>, ...)** -- User-group condition.
The rule only applies if the Channel Access client's user
is a member of one of the listed UAGs.
-   **HAG(<name>, ...)** -- Host-group condition.
The rule only applies if the client's host
(as determined by its IP or hostname) is in one of the listed HAGs
-   **CALC("<expression>")** -- Calculation condition.
The rule only applies if the given expression evaluates to true (non-zero).

**Special Semantics for RULEs:**
Rules will continue to allow the prescribed access if and only if
all the predicates the rule contains are satisfied.
- If the rule contains predicates that are unknown to the parser
(indicating future functionality),
then the rule will NOT not match,
but no syntax error will be reported as long as the syntax is correct.
- If the rule contains predicates that the parser does not recognise
which are malformed (e.g. missing parentheses),
then the rule will not match and the parser will report a syntax error.
- In this way rules can be extended with new predicates
without breaking older clients or giving those older clients elevated privileges.

**Special Semantics for unrecognised ACF file elements:**
Any elements that are included in an ACF file will be ignored silently
by a parser that does not understand them.
- If an element is seen in an ACF file that is not understood by the parser,
the parser will simply ignore it silently,
without reporting an error,
as long as its syntax is correct.
- If elements are added to the ACF file that are malformed
(e.g. missing parentheses),
the parser will report a syntax error.
- Thus new elements can be added to ACF files in new EPICS releases
without breaking older clients that loads those files.

In summary, **ACF forward compatibility**
means that from EPICS 7.0.10 onward,
any new access security features will use this established syntax.
The parser will recognize new group types or rule options using the same
`<KEYWORD>(...) { ... }` convention,
ensuring they can be used in files loaded by IOCs running EPICS 7.0.10 or later
without breaking those IOCs or requiring their parser to be modified.
This change **does not require any modifications to existing ACF files
or downstream tools** -- all legacy syntax remains valid,
and the new standardized grammar provides a robust foundation for future extensions.

A full EBNF grammar for the new syntax can be found in the
[IOC Access Security](ACF-Language.md) document added to this release.

### Add `dbglob` to replace `dbgrep`

A new IOC shell command, `dbglob` has been added, with `dbgrep` becoming
an alias of this new function, with the intent of deprecating it in a
future release.

### Conflict-free release note entries for GitHub pull requests

GitHub [PR #628](https://github.com/epics-base/epics-base/pull/628)

This release replaces the developer-edited `documentation/RELEASE_NOTES.md`
source file in the EPICS tree with a process which generates that file from a
series of individual files added for each changeset in the release, thus
preventing merge conflicts when entries are added by many different pull
requests.

For this new approach each pull request must add its own Markdown file to the
`documentation/new-notes` directory, using a unique filename.
When a release is made, all these files will be combined into a single
`RELEASE-<version>.md` file and the `new-notes` directory emptied to prepare
for development of the next release.

Developers can generate the `RELEASE_NOTES.md` file by running `make` in the
`documentation` directory, which will install the result in the `doc` top-level
directory along with the `RELEASE-<version>.md` files describing older EPICS
releases going back to 3.15.
The `documentation/Makefile` provides some other targets which can also be
requested, but they require additional non-EPICS software such as Doxygen and
Sphinx to have been installed first.

Between releases the generated `RELEASE_NOTES.md` file contains the text from
any `new-notes` files added to document changes already merged.
It also provides links to the older `RELEASE-<version>.md` files, so it remains
the starting point for documentation on all release changes.

Detailed instructions on creating new entries are provided in a `README.txt`
file in the `documentation/new-notes` directory.
The release-time process that generates a new `RELEASE-<version>.md` file is
described in the developers' [Release Checklist](ReleaseChecklist.md).

### Add support for `EPICS_DB_INCLUDE_PATH` to `dbLoadTemplate`

GitHub [PR #636](https://github.com/epics-base/epics-base/pull/636)

Allow finding the substitution file through a path in `EPICS_DB_INCLUDE_PATH` or
an additional parameter to the iocsh `dbLoadTemplate` command.

### Expand the use of colour in the IOCs output

This release includes various changes to iocsh.cpp and elsewhere to add and
expand the use of color:

- When loading a startup script, the IOC Shell now displays comment lines in
blue, and uses bold to make command lines stand out from other text.

- The `softIoc -v` output also uses the above color scheme for the commands it
prints.

- The default IOC Shell prompt is now displayed in green; this color can be
modified in the `configure/CONFIG_SITE_ENV` file for all targets, or set for
a specific target by adding a `configure/os/CONFIG_SITE_ENV.<arch>` file.
The value of the `IOCSH_PS1` environment parameter in those files can use the
`ANSI_ENV_*` and `ANSI_*()` color macros found in errlog.h to configure the
appearance of the prompt. The C string literal concatenation syntax can be
used to construct the prompt string:

```Makefile
    IOCSH_PS1 = ANSI_ESC_RED "e" ANSI_ESC_YELLOW "p" ANSI_ESC_GREEN "i" \
        ANSI_ESC_CYAN "c" ANSI_BLUE("s") "> "
```

- More error messages printed by IOC Shell commands now appear in red, or use
the red `ERROR` prefix that was introduced in previous releases.

- The word "Illegal" has been replaced with "Invalid" in several Shell error
messages.

- The iocsh `var` command now shows the data type of the registered variables
as well as their values.

### Documentation Updates

The reference documentation for the [event](eventRecord.md) record type
has been updated to cover the use of named events which were added in Base
3.14.12.3 and 3.15.1.

Documentation for CALC expression evaluation has been updated for format
enhancements and to add some missing operators.
The best documentation for these expressions can be found in the
[postfix.h](postfix_h.md) header in libCom, but both the
[calc](calcRecord.md) and [calcout](calcoutRecord.md) record reference
pages also cover the infix expressions supported.

### Records calc, calcout and sub extended

The record types calc, calcout and sub have been extended from 12 inputs
A - L to 21 inputs A - U.
The macro `CALCPERFORM_NARGS` reflects this change.
The new inputs can be used in calc links and access security as well.
The size of CALC and OCAL fields has been doubled to 160 chars.

### Allow hex and octal strings in dbPut and dbGet

It is now possible to convert hex and octal strings to integer fields
with `dbPut()`, `dbGet()` and related functions like the iocsh command
`dbpf` or through database links.

Possible incompatibility: Up to now, leading `0`s have been ignored,
now they switch to octal mode.

For backward compatibility, this behavior can be switched off, returning
to the old decimal only conversions, by setting the environment variable
`EPICS_DB_CONVERT_DECIMAL_ONLY` to `YES` (case insensitive) before `iocInit`.

### `dfanout` improvements

The [dfanout](dfanoutRecord.md) record now has invalid output handling with the usual fields
`IVOA` and `IVOV` just like many other output record types.

The number of output links has also been increased from 8 to 16.

### CA Client Library Enhancement

GitHub [PR #711](https://github.com/epics-base/epics-base/pull/711)

* Allow CA clients to determine a server's protocol version.

  Adds a call to the CA client API that allows a client to determine the
  server's protocol minor version number.
  This is needed to allow the ca-nameserver to report a server's protocol
  version correctly to a client.

* `ca_host_minor_protocol()` return for disconnected channels

  `ca_host_minor_protocol()` now explicitly returns `CA_UKN_MINOR_VERSION`
  for a disconnected channel.

### Add environment variable to opt out of POSIX Real-Time scheduling

On POSIX systems, processes with real-time capabilities can opt out of using
Posix thread priority scheduling and memory locking.
Set `EPICS_ALLOW_POSIX_THREAD_PRIORITY_SCHEDULING=NO` to achieve this.

### Avoid early expiration of timers on non-RTOS IOCs

Previously the epicsTimer code rounded down user requested delays
by subtracting one half of the sleep "quantum".
On RTEMS and vxWorks,
this allowed periodic timers which expired on every tick.
However this also resulted in timers expiring slightly
[earlier than requested](https://github.com/epics-base/epics-base/issues/106).

With [PR 744](https://github.com/epics-base/epics-base/pull/744)
rounding is only done for RTEMS and vxWorks, which still have tick timers.

This affects several facilities which use epicsTimer,
including record delays.
For example, `calcout.ODLY` becomes more
[accurate](https://github.com/epics-base/epics-base/issues/106#issuecomment-1260232765)
on non-RTOS IOCs.

### normativeTypes Module

Changes to this module since the previous release:

#### Release 6.0.2

- Fix potential NULL pointer dereference in `NTNDArray::getValueSize()`

### pvAccess Module

Changes to this module since the previous release:

#### Release 7.1.8

- Compatible changes
  - Capped the number and age of PVA beacons to avoid a resource leak. Beacons
    older than 360 seconds will be destroyed automatically, new beacons will be
    ignored if >=2048 exist already.
  - Various Clang, MSVC and GCC compiler warnings cleaned up.

### pvData Module

Changes to this module since the previous release:

#### Release 8.0.7

- Compatible changes
  - Allow epics::pvData::Timer to be cancelled during callback execution.
  - Clang compiler warnings cleaned up.
  - Limit periodic timers to one catch-up after missing many events.
 

### pvaClient Module

Changes to this module since the previous release:

#### Release 4.8.1

* Fix error message generation code.


