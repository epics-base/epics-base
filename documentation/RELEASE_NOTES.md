# EPICS Base Release 3.15.7

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
[here](filters.html) or [here](../html/filters.html) depending on where you're
reading this document from.

### Imported Record Reference Documentation from Wiki

The remaining record types that had 3.14 reference documentation in the EPICS
Wiki have had that documentation converted and imported into their DBD files.
The preferred form for future updates to the record type descriptions is now an
emailed patch file, a Pull Request through GitHub, or a Merge Request through
Launchpad. Note that in some cases the behavior of a record type in a 7.0.x
release may differ from that of the same record type in a 3.15 release, although
this would be unusual, so it may be important to indicate the branch that your
changes apply to.

**NOTE:** *These documentation changes may have modified the order of the fields
in some record definitions, in which case this release will not be compatible
with record or device support binaries that were compiled against an earlier
release.*

### `make test-results` for Windows

The make target `test-results` should now work properly on Windows. Some Perl
installations used versions of `prove.bat` that would only display the results of
up to 3 tests or didn't return an error status in the event of tests failing. The
build system now calls its own perl script to summarize the results instead of
passing a list of TAP filenames to `prove`.

### Add option to avoid CALLBACK conflict

If a macro `EPICS_NO_CALLBACK` is defined, then callback.h will no longer
(re)define CALLBACK. The name 'CALLBACK' is used by the WIN32 API, and
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
in lp:1527636 introduced a bug which cased these short lived clients to crash on
exit. This bug should now be fixed.

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
    to a too small buffer being provided in dbContextReadNotifyCache
  - [lp:1479316](https://bugs.launchpad.net/epics-base/+bug/1479316) Some data
    races found using Helgrind
  - [lp:1495833](https://bugs.launchpad.net/epics-base/+bug/1495833) biRecord
    prompt groups are nonsensical
  - [lp:1606848](https://bugs.launchpad.net/epics-base/+bug/1606848) WSAIoctl
    `SIO_GET_INTERFACE_LIST` failed in Windows

### Whole-Program Optimization for MS Visual Studio Targets

When using the Microsoft compilers a new build system variable is provided
that controls whether whole program optimization is used or not. For static
builds using Visual Studio 2010 this optimization must be disabled. This is
controlled in the files `configure/os/CONFIG_SITE.Common.windows-x64-static` and
`configure/os/CONFIG_SITE.Common.win32-x86-static` by setting the variable
`OPT_WHOLE_PROGRAM = NO` to override the default value
`YES` that would otherwise be used.

Note that enabling this optimization slows down the build process. It is not
possible to selectively disable this optimization, when building a particular
module say; Microsoft's linker will restart itself automatically with the
`-LTCG` flag set and display a warning if it is asked to link any object
files that were compiled with the `-GL` flag.

### Add dynamic (variable length) array support to PCAS

Dynamic array sizing support was added to the IOC server (RSRV) in the
Base-3.14.12 release, but has not until now been supported in the “Portable
Channel Access Server” (PCAS). Channel Access server applications using the
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

### New implementation of promptgroup/gui\_group field property

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

libca, RSRV, and PCAS may now use IPv4 multicasting for UDP traffic (name
search and beacons). This is disabled by default. To enable multicast address(s)
must be listed in `EPICS_CA_ADDR_LIST` for clients and `EPICS_CAS_INTF_ADDR_LIST`
for servers (IOCs should set both). For example:
`EPICS_CAS_INTF_ADDR_LIST='224.0.2.9' EPICS_CA_ADDR_LIST=224.0.2.9`

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

## Changes made between 3.15.2 and 3.15.3

### Make the NTP Time provider optional on VxWorks

Recent versions of VxWorks (sometime after VxWorks 6) provide facilities for
automatically synchronizing the OS clock time with an NTP server. The EPICS time
system used to assume that it had to provide time synchronization on VxWorks,
but now it tests for the existance of either of the two OS synchronization
threads before starting the NTP time provider. It is still possible to force the
NTP provider to be started even if the OS synchronization is running by defining
the environment variable `EPICS_TS_FORCE_NTPTIME` in the startup script
before loading the IOC's `.munch` file. Forcing may be necessary if the VxWorks
image is not correctly configured with the IP address of a local NTP server.

### Assembling files from numbered snippets

A tool has been added that assembles file snippets specified on the command line
into a single output file, with sorting and replacing/adding of snippets done
based on their file names. The build system integration requires the output file
to be specified setting `COMMON_ASSEMBLIES` (arch independent) or `ASSEMBLIES`
(created by arch), then defining the snippets for each assembly setting
`*_SNIPPETS` (explicitly) or `*_PATTERN` (searched relative to all source
directories).

### Clean up after GNU readline()

If EPICS Base is built with readline support, any IOC that calls epicsExit()
from a thread other than the main thread is likely to leave the user's terminal
in a weird state, requiring the user to run something like 'stty sane' to clean
it up. This release patches the readline support code to clean up automatically
by registering an epicsAtExit() routine.

### Removed the last vestiges of `RSET::get_value()`

The IOC has not called the `get_value()` routine in the RSET for a very long
time, but there was still one implementation left in the event record support
code, and a structure definition for one of the original arguments to that
routine was defined in recGbl.h. Both of these have now been removed.

## Changes made between 3.15.1 and 3.15.2

### Raised limit on link field length in database files

The length of INP/OUT link fields in database files was limited to 79 chars
by an internal buffer size in the db file parser. This limitation will go away
completely in 3.16, and has been statically raised to 255 chars for the 3.15
series.

### aoRecord raw conversion overflows

The ao record type now checks converted raw values and limits them to the
32-bit integer range before writing them to the RVAL field. Previously value
overflows relied on Undefined Behaviour which could give different results on
different platforms. The ROFF fields of the ao and ai record types are now
`DBF_ULONG` to allow an ROFF setting of 0x80000000 to work properly.

### Changes to `<top>/cfg/*` files

The order in which `cfg/CONFIG*` and `cfg/RULES*` files are included from
support applications listed in the `configure/RELEASE*` files has been changed.
Previously these files were included in the order in which the top areas are
listed in the RELEASE file, but it makes more sense to load them in reverse
order since later entries override earlier ones in Makefiles but the release
file order is supposed to allow earlier entries to take precedence over later
ones. The same change has been made to the inclusion of the
`<top>/configure/RULES_BUILD` files.

Two new file types can also be provided in a module's `cfg` directory. Files
named `TOP_RULES*` will be included by the top-level Makefile of other modules
that refer to this module; files name `DIR_RULES*` will be included by all
Makefiles that merely descend into lower-level directories. The `cfg/RULES*`
files are only included when make is building code inside the `O.<arch>`
directories.

The new `cfg/DIR_RULES*` file inclusion was designed to permit new recursive
make actions to be implemented by appending the name of the new action to the
`ACTIONS` variable. There must be a matching rule in one of the `cfg/RULES*`
files when doing this. Similar rules may also be defined in the `cfg/TOP_RULES*`
and/or `cfg/DIR_RULES*` files, but these should only state prerequisites and not
directly provide commands to be executed.

### Build rules for RTEMS GESYS modules

RTEMS target builds can now be configured to make GESYS modules by changing
the `USE_GESYS=NO` setting in the file
`configure/os/CONFIG_SITE.Common.RTEMS` to `YES`.

### Added Make variables for command-line use

The following variables are now used during the build process, reserved for
setting on the command-line only (Makefiles should continue to use the
`USR_` equivalents):

  - `CMD_INCLUDES`
  - `CMD_CPPFLAGS`
  - `CMD_CFLAGS`
  - `CMD_CXXFLAGS`
  - `CMD_LDFLAGS`
  - `CMD_DBFLAGS`
  - `CMD_DBDFLAGS`
  - `CMD_REGRDDFLAGS`
  - `CMD_ARFLAGS`

For example:

```
    make CMD_INCLUDES=/opt/local/include CMD_LDFLAGS=-L/opt/local/lib
```

### Enhanced API for asTrapWrite listeners

External software such as the CA Put Logging module that registers a listener
with the asTrapWrite subsystem was not previously given access to the actual
data being sent by the CA client. In most cases this was not a problem as the
listener can look at the field being modified both before and after the
operation, but if the put processes the record which immediately overwrites the
new value, the client's value cannot be observed.

This release adds three fields to the asTrapWriteMessage structure that is
passed to the listener routines. These new fields provide the CA data type, the
number of array elements, and a pointer to the source data buffer. This change
is completely backwards compatible with listener code written against the
original API. The new API can be detected at compile-time as follows:

```
    #include "asLib.h"

    /* ... */

    #ifdef asTrapWriteWithData
        /* Enhanced API */
    #endif
```

### Use of `PATH_FILTER` in Makefiles deprecated

The `PATH_FILTER` variable was being called to convert forward slashes
`/` in file paths into pairs of backward slashes
`\\` on Windows architectures. This has never been strictly
necessary, and was added about 10 years ago to get around some short-comings in
Windows tools at the time. All uses of `PATH_FILTER` in Base have now been
removed; the definition is still present, but will result in a warning being
printed if it is ever used.

### Using msi for dependencies

To reduce confusion the msi program has been modified to allow the generation of
dependency rules by adding support for a `-D` option, and changing the commands
in RULES.Db to use this option instead of the mkmf.pl script. The new build
rules will not work with old versions of the msi program, so the command
variable name used in the rules has been changed from `MSI` to `MSI3_15`. Sites
that use a modified version of msi must provide support for both the `-D` and
`-o outfile` options, and should then point the `MSI3_15` variable in their
applications' `CONFIG_SITE` files to that updated executable.

## Changes made between 3.15.0.2 and 3.15.1

### `epicsStrnEscapedFromRaw()` and `epicsStrnRawFromEscaped()`

These routines have been rewritten; the previous implementations did not
always behave exactly as specified.

### Shared Library Versions

On architectures that can support it, the shared library version number for
libraries provided with Base has had the third component of the EPICS version
number added to it, thus libCom.so.3.15.1 instead of libCom.so.3.15. Windows
can only support two components to its internal product version number, and the
Darwin bug that external shared libraries were being built using the EPICS
version number has been fixed.

### Hooking into dbLoadRecords

A function pointer hook has been added to the dbLoadRecords() routine, to allow
external modules such as autosave to be notified when new records have been
loaded during IOC initialization. The hook is called `dbLoadRecordsHook` and
follows the model of the `recGblAlarmHook` pointer in that modules that wish to
use it must save the current value of the pointer before installing their own
function pointer, and must call the original function from their own routine.

The hook is activiated from the `dbLoadRecords()` routine and gets called only
after a database instance file has been read in without error. Note that the
`dbLoadTemplates()` routine directly calls `dbLoadRecords()` so this hook also
provides information about instantiated database templates. It is still possible
to load record instances using `dbLoadDatabase()` though, and doing this will
not result in the hook routines being called.

Code to use this hook should look something like this:

```
    #include "dbAccessDefs.h"

    static DB_LOAD_RECORDS_HOOK_ROUTINE previousHook;

    static void myRoutine(const char* file, const char* subs) {
        if (previousHook)
            previousHook(file, subs);

        /* Do whatever ... */
    }

    void myInit(void) {
        static int done = 0;

        if (!done) {
            previousHook = dbLoadRecordsHook;
            dbLoadRecordsHook = myRoutine;
            done = 1;
        }
    }
```

As with many other parts of the static database access library there is no
mutex to protect the function pointer. Initialization is expected to take place
in the context of the IOC's main thread, from either a static C++ constructor or
an EPICS registrar routine.

## Changes made between 3.15.0.1 and 3.15.0.2

### New iocshLoad command

A new command `iocshLoad` has been added to iocsh which executes a
named iocsh script and can also set one or more shell macro variables at the
same time, the values of which will be forgotten immediately after the named
script finishes executing. The following example shows the syntax:

```
    iocshLoad "serial.cmd", "DEV=/dev/ttyS0,PORT=com1,TYPE=RS485"
    iocshLoad "radmon.cmd", "PORT=com1,ADDR=0"
```

### Support routines for 64-bit integers

The libCom library now provides support for 64-bit integer types on all
supported architectures. The epicsTypes.h header file defines epicsInt64 and
epicsUInt64 type definitions for both C and C++ code. The epicsStdlib.h header
also declares the following for parsing strings into the relevent sized integer
variables: Functions epicsParseLLong(), epicsParseULLong() with related macros
epicsScanLLong() and epicsScanULLong(), and the functions epicsParseInt64()
and epicsParseUInt64(). Use the first two functions and the macros for long long
and unsigned long long integer types, and the last two functions for the
epicsInt64 and epicsUInt64 types. Note that the latter can map to the types long
and unsigned long on some 64-bit architectures such as linux-x86\_64, not to the
two long long types.

This version does not provide the ability to define 64-bit record fields, the
use of the 64-bit types in the IOC database will come in a later release of
EPICS Base.

### Full support for loadable support modules

Apparently later versions of Base 3.14 permitted support modules to be loaded
from a shared library at runtime without the IOC having been linked against that
shared library; the registerRecordDeviceDriver.pl program would accept a partial
DBD file containing just the entries needed for the library and generate the
appropriate registration code. In 3.15 however the registerRecordDeviceDriver.pl
program was replaced by one using the new DBD file parser, and in this a device
support entry would only be accepted after first loading the record type that it
depended on.

The parser has been modified to accept device entries without having seen the
record type first, although a warning is given when that happens. To remove the
warning the DBD file can provide a record type declaration instead (no fields
can be defined, so the braces must be empty), before the device() entry. The
result will generate the correct registration code for the device entry without
including anything for any merely declared record types. The generated code can
be linked into a shared library and loaded by an IOC at runtime using dlload.

### Parallel callback threads

The general purpose callback facility can run multiple parallel callback
threads per priority level. This makes better use of SMP architectures (e.g.
processors with multiple cores), as callback work - which includes second
stage processing of records with asynchronuous device support and I/O
scanned processing - can be distributed over the available CPUs.

Note that by using parallel callback threads the order of scan callback
requests in the queue is not retained. If a device support needs to be
informed when scanIoRequest processing has finished, it should use the new
scanIoSetComplete() feature to add a user function that will be called after
the scanIoRequest record processing has finished.

Parallel callback threads have to be explicitly configured, by default
the IOC keeps the old behavior of running one callback thread per priority.

### Merge MMIO API from devLib2

Added calls to handle 8, 16, and 32 bit Memory Mapped I/O reads and writes.
The calls added include `X_iowriteY()` and `X_ioreadY()`
where `X` is `nat` (native), `be` or `le`, and `Y` is `16` or `32`.
Also added are `ioread8()` and `iowrite8()`.

### Added optional dbServer API to database

A server layer that sits on top of the IOC database may now register itself
as such by calling `dbRegisterServer()` and providing optional routines
that other components can use. The initial purpose of this API allows the Trace
Processing implementation in `dbProcess()` to identify a client that
causes a record to process when TPRO is set.

To support the client identification, the server provides a routine that
returns that identity string when called by one of its own processing
threads.

### Concatenated database definition files

A series of database definition (dbd) files can now be concatenated during
the build process into a newly-created dbd file with result being installed into
`$(INSTALL_LOCATION)/dbd` without expanding it.

The following lines in an EPICS Makefile will create a file name.dbd in the
O.Common build directory containing the contents of file1.dbd followed by
file2.dbd then file3.dbd. The new file will then be installed into
`$(INSTALL_LOCATION)/dbd` without expanding any of its include statements.

```
    DBDCAT += name.dbd
    name_DBD += file1.dbd file2.dbd file3.dbd
```

The source files file1.dbd, file2.dbd and file3.dbd may be created by the
current Makefile, be located in the parent directory or any other directory in
the `SRC_DIRS` list, be specified by their full pathname, exist in the install
dbd directory, or be found in any dbd directory linked from the application's
RELEASE files.

### Posix: Drop `SCHED_FIFO` before exec() in child process

If Base is compiled with `USE_POSIX_THREAD_PRIORITY_SCHEDULING = YES`
in `configure/CONFIG_SITE` or related files, the Posix implementation of the
libCom `osiSpawnDetachedProcess()` routine will switch the child process
to use the normal `SCHED_OTHER` (non real-time) scheduler before executing the
named executable program. If it needs to use the real-time scheduler the new
program can request that for itself.

### Posix: Lock all memory when running with FIFO scheduler

On Posix systems, an IOC application's ability to meet timing deadlines is
often dependent on its ability to lock part or all of the process's virtual
address space into RAM, preventing that memory from being paged to the swap
area. This change will attempt to lock the process's virtual address space into
RAM if the process has the ability to run threads with different priorities. If
unsuccessful, it prints an message to stderr and continues.

On Linux, one can grant a process the ability to run threads with different
priorities by using the command `ulimit -r unlimited`. To use the
FIFO scheduler for an IOC, use a command like this:

```
    chrt -f 1 softIoc -d test.db
```

On Linux, one can grant a process the ability to lock itself into memory
using the command `ulimit -l unlimited`. These limits can also be
configured on a per user/per group basis by changing `/etc/security/limits.conf`
or its equivalent.

A child process created via `fork()` normally inherits its parent's resource
limits, so a child of a real-time soft-IOC will get its parent's real-time
priority and memlock limits. The memory locks themselves however are not
inherited by child processes.

### Implement `EPICS_CAS_INTF_ADDR_LIST` in rsrv

The IOC server can now bind to a single IP address (and optional port number)
read from the standard environment parameter `EPICS_CAS_INTF_ADDR_LIST`.
Additional addresses included in that parameter after the first will be ignored
and a warning message displayed at iocInit time.

### alarmString.h deprecated again

The string arrays that provide string versions of the alarm status and
severity values have been moved into libCom and the header file that used to
instanciate them is no longer required, although a copy is still provided for
backwards compatibility reasons. Only the alarm.h header needs to be included
now to declare the `epicsAlarmSeverityStrings` and `epicsAlarmConditionStrings`
arrays.

### General purpose thread pool

A general purpose threaded work queue API epicsThreadPool is added.
Multiple pools can be created with controllable priority and number
of worker threads. Lazy worker startup is supported.

### Database field setting updates

A database (.db) file loaded by an IOC does not have to repeat the record
type of a record that has already been loaded. It may replace the first
parameter of the `record(type, name)` statement with an asterisk
character inside double-quotes, `"*"` instead. Thus the following is a
legal database file:

```
    record(ao, "ao1") {}
    record("*", "ao1") {
        field(VAL, 10)
    }
```

Note that database configuration tools will not be expected to have to
understand this syntax, which is provided for scripted and hand-coded database
and template instantiation only. Setting the IOC's `dbRecordsOnceOnly`
flag also makes this syntax illegal, since its purpose is to prevent
multiply-defined records from being collapsed into a single instance.

### Added echo command to iocsh

The single argument string may contain escaped characters, which will be
translated to their raw form before being printed (enclose the string in quotes
to avoid double-translation). A newline is always appended to the output, and
output stream redirection is supported.

### Added macro `EPICS_UNUSED` to compilerDependencies.h

To prevent the compiler from warning about a known-unused variable, mark it
with the macro `EPICS_UNUSED`. On gcc and clang this will expand to
`__attribute__((unused))` to prevent the warning.

### User specified db substitution file suffix

Per Dirk Zimoch's suggestion, a user specified db substitution file suffix is
now allowed by setting the variable `SUBST_SUFFIX` in a configuration directory
`CONFIG_SITE` file or in a Makefile before the include $(TOP)/configure/RULES
line. The default for `SUBST_SUFFIX` is `.substitutions`

### NTP Time Provider adjusts to OS tick rate changes

Dirk Zimoch provided code that allows the NTP Time provider (used on VxWorks
and RTEMS only) to adapt to changes in the OS clock tick rate after the provider
has been initialized. Note that changing the tick rate after `iocInit()` is not
advisable, and that other software might still misbehave if initialized before
an OS tick rate change.

### Added newEpicsMutex macro

Internal C++ uses of `new epicsMutex()` have been replaced with a new
macro which calls a new constructor, passing it the file name and line number of
the mutex creation code. C code that creates mutexes has been using a similar
macro for a long time, but there was no equivalent constructor for the C++
wrapper class, so identifying a specific mutex was much harder to do.

### Post `DBE_PROPERTY` events automatically

A new record field attribute `prop(YES)` has been added to identify fields
holding meta-data. External changes to these fields will cause a CA monitor
event to be sent to all record subscribers who have asked for `DBE_PROPERTY`
updates. Meta-data fields have been marked for all Base record types.

### errlogRemoveListener() routine changed

Code that calls `errlogRemoveListener(myfunc)` must be modified to use
the new, safer routine `errlogRemoveListeners(myfunc, &pvt)` instead.
The replacement routine takes a second argument which must be the same private
pointer that was passed to `errlogAddListener()` when adding that
listener. It also deletes all matching listeners (hence the new plural name) and
returns how many were actually deleted, whereas the previous routine only
removed the first listener that matched.

### Simplified generation of .dbd files

The Perl script `makeIncludeDbd.pl` has been removed and the rules
that created an intermediate `xxxInclude.dbd` file from the
Makefile variable `xxx_DBD` have been modified to generate the target
`xxx.dbd` file directly. This should simplify applications that
might have had to provide dependency rules for the intermediate files in 3.15.
Applications which provide their own `xxxInclude.dbd` source file
will continue to have it expanded as before.

### New Undefined Severity field UDFS

A new field has been added to dbCommon which configures the alarm severity
associated with the record being undefined (when UDF=TRUE). The default value is
INVALID so old databases will not be affected, but now individual records can be
configured to have a lower severity or even no alarm when undefined. Be careful
when changing this on applications where the IVOA field of output records is
used, IVOA still requires an INVALID severity to trigger value replacement.

### New build target `tapfiles`

This new make target runs the same tests as the `runtests` target, but
instead of summarizing or displaying the output for each test script it creates
a `.tap` file inside the architecture build directory which contains the
detailed test output. The output file can be parsed by continuous integration
packages such as [Jenkins](http://www.jenkins-ci.org/) to show the
test results.

### Array field double-buffering

Array data can now be moved, without copying, into and out of the VAL field
of the waveform, aai, and aao record types by replacing the pointer in BPTR.
The basic rules which device support must follow are:

1.  BPTR, and the memory it is currently pointing to, can only be accessed
    while the record is locked.
2.  NELM may not be changed; NORD should be updated whenever the number of
    valid data elements changes.
3.  When BPTR is replaced it must always point to a block of memory large
    enough to hold the maximum number of elements, as given by the NELM and
    FTVL fields.

### Spin-locks API added

The new header file epicsSpin.h adds a portable spin-locks API which is
intended for locking very short sections of code (typically one or two lines of
C or C++) to provide a critical section that protects against race conditions.
On Posix platforms this uses the `pthread_spinlock_t` type if it's available and
the build is not configured to use Posix thread priorities, but otherwise it
falls back to a `pthread_mutex_t`. On the UP VxWorks and RTEMS platforms the
implementations lock out CPU interrupts and disable task preemption while a
spin-lock is held. The default implementation (used when no other implementation
is provided) uses an epicsMutex. Spin-locks may not be taken recursively, and
the code inside the critical section should be short and deterministic.

### Improvements to aToIPAddr()

The libCom routine aToIPAddr() and the vxWorks implementation of the
associated hostToIPAddr() function have been modified to be able to look up
hostnames that begin with one or more digits. The epicsSockResolveTest program
was added to check this functionality.

### mbboDirect and mbbiDirect records

These record types have undergone some significant rework, and will behave
slightly differently than they did in their 3.14 versions. The externally
visible changes are as follows:

##### mbbiDirect

  - If the MASK field is set in a database file, it will not be over-written
    when the record is initialized. This allows non-contiguous masks to be set,
    although only the device support actually uses the MASK field.
  - If process() finds the UDF field to be set, the record will raise a
    UDF/INVALID alarm.

##### mbboDirect

  - If the MASK field is set in a database file, it will not be over-written
    when the record is initialized. This allows non-contiguous masks to be set,
    although only the device support actually uses the MASK field.
  - After the device support's `init_record()` routine returns during record
    initialization, if OMSL is “supervisory” and UDF is clear the fields
    B0-BF will be set from the current VAL field.
  - When a put to the OMSL field sets it to “supervisory”, the fields
    B0-BF will be set from the current VAL field. This did not used to happen,
    the individual bit fields were previously never modified by the record.
    Note that this change may require some databases to be modified, if they
    were designed to take advantage of the previous behavior.

### Redirection of the errlog console stream

A new routine has been added to the errlog facility which allows the console
error message stream to be redirected from stderr to some other already open
file stream:

```
    int errlogSetConsole(FILE *stream);
```

The stream argument must be a `FILE*` pointer as returned by `fopen()` that is
open for output. If `NULL` is passed in, the errlog thread's stderr output stream
will be used instead. Note that messages to the console can be disabled and
re-enabled using the `eltc` routine which is also an iocsh command, but there is
no iocsh command currently provided for calling `errlogSetConsole()`.

### Add cleanup subroutine to aSub record

An aSub routine may set the CADR field with a function pointer which will be
run before a new routine in the event that a change to the SNAM field changes
the record's process subroutine.

This can be used to free any resources the routine needs to allocate. It can
also be used to determine if this is the first time this routine has been called
by this record instance. The CADR field is set to NULL immediately after the
routine it points to is called.

Example:

```
    void cleanup(aSubRecord* prec) {
        free(prec->dpvt);
        prec->dpvt = NULL;
    }

    long myAsubRoutine(aSubRecord* prec) {
        if (!prec->cadr) {
            /* check types of inputs and outputs */
            if (prec->ftva != menuFtypeDOUBLE)
                return 1; /* oops */

            dpvt = malloc(42);
            prec->cadr = &cleanup;
        }

        /* normal processing */
    }
    epicsRegisterFunction(myAsubRoutine);
```

### Sequence record enhancements

The sequence record type now has 16 link groups numbered 0 through 9 and A
through F, instead of the previous 10 groups numbered 1 through 9 and A. The
changes to this record are directly equivalent to those described below for the
fanout record. The fields OFFS and SHFT have been added and operate on the SELN
value exactly the same way. The result is backwards compatible with the 3.14
version of the sequence record as long as none of the new fields are modified
and the application does not rely on the SOFT/INVALID alarm that was generated
when the selection number exceeded 10. The record also now posts monitors on the
SELN field at the end of the sequence if its value changed when read through the
SELL link.

### Fanout record enhancements

The fanout record type now has 16 output links LNK0-LNK9 and LNKA-LNKF, plus
two additional fields which make the result backwards compatible with 3.14
databases, but also allow the link selection to be shifted without having to
process the SELN value through a calc or calcout record first.

Previously there was no LNK0 field, so when SELM is “Mask” bit 0 of SELN
controls whether the LNK1 link field was activated; bit 1 controls LNK2 and so
on. When SELM is “Specified” and SELN is zero no output link would be
activated at all; LNK1 gets activated when SELN is 1 and so on. Only 6 links
were provided, LNK1 through LNK6. The updated record type maintains the original
behavior when the new fields are not configured, except that the SOFT/INVALID
alarm is not generated when SELN is 7 through 15.

The update involved adding a LNK0 field, as well as fields LNK7 through LNK9
and LNKA through LNKF. To add flexibility and maintain backwards compatibility,
two additional fields have been added:

  - **OFFS**  
    This field holds a signed offset which is added to SELN to select which link
    to activate when SELM is “Specified”. If the resulting value is outside the
    range 0 .. 15 the record will go into a SOFT/INVALID alarm state. The default
    value of OFFS is zero, so if it is not explicitly set and SELN is 1 the LNK1
    link will be activated.
  - **SHFT**  
    When SELM is “Mask” the signed field SHFT is used to shift the SELN
    value by SHFT bits (positive means right-wards, values outside the range -15 ..
    15 will result in a SOFT/INVALID alarm), before using the resulting bit-pattern
    to control which links to activate. The default value is -1, so if SHFT is not
    explicitly set bit 0 of SELN will be used to control whether LNK1 gets
    activated.

The record also now posts monitors on the SELN field if it changes as a
result of record processing (i.e. when read through the SELL link).

### Deleted Java build rules

Java has its own build systems now, so we've deleted the rules and associated
variables from Base, although they might get added to the Extensions build rules
for a while in case anyone still needs them.

## Changes made between 3.14.x and 3.15.0.1

### Application clean rules

The `clean` Makefile target has changed between a single-colon rule
and a double-colon rule more than once in the life of the EPICS build rules, and
it just changed back to a single-colon rule, but now we recommend that
applications that wish to provide a Makefile that is backwards compatible with
the 3.14 build rules use the construct shown below. The 3.15 rules now support
a variable called `CLEANS` to which a Makefile can add a list of files to
be deleted when the user does a `make clean` like this:

```
    CLEANS += <list of files to be cleaned>

    ifndef BASE_3_15
    clean::
        $(RM) $(CLEANS)
    endif
```

The conditional rule provides compatibility for use with the 3.14 build
system.

### MSI included with Base

An enhanced version of the Macro Substitution and Include program “msi”
has been included with Base. Both this new version of msi and the IOC's
`dbLoadTemplates` command now support setting global macros in
substitution files, and `dbLoadTemplates` can now take a list of global
macro settings as the second argument on its command line. The substitution file
syntax is documented in the Application Developers Guide.

### Cross-builds targeting win32-x86-mingw

Some Linux distributions now package the MinGW cross-compiler which makes it
possible to cross-build the win32-x86-mingw target from a linux-x86 host. Build
configuration files for this combination are now included; adjust the settings
in `configure/os/CONFIG_SITE.linux-x86.win32-x86-mingw` and add win32-x86-mingw to
the `CROSS_COMPILER_TARGET_ARCHS` variable in `configure/CONFIG_SITE` or in
`configure/os/CONFIG_SITE.linux-x86.Common`.

### Architecture win32-x86-cygwin Removed

The ability to compile non-cygwin binaries using the Cygwin build tools is no
longer supported by current versions of Cygwin, so this architecture has been
removed. Use the MinWG tools and the win32-x86-mingw architecture instead.

### RTEMS and VxWorks Test Harnesses

The original libCom test harness has been renamed `libComTestHarness`,
and two additional test harnesses have been created `dbTestHarness` and
`filterTestHarness` which are all built for RTEMS and vxWorks targets.
The new ones include tests in src/ioc/db/test and src/std/filters/test.

Running the new tests requires additional .db and .dbd files to be loaded at
runtime, which can be found in the relevant source directory or its O.Common
subdirectory. If the target can access the Base source tree directly it may be
simplest to cd to the relevant source directory before running the test. If not,
the files needed are listed in the generated 'testspec' file found in the
associated build (O.*arch*) directory.

For RTEMS users the current directory is determined in a BSP specific way.
See `rtems_init.c` and `setBootConfigFromNVRAM.c` in `src/libCom/RTEMS`.

### New API to hook into thread creation

A hook API has been added allowing user-supplied functions to be called
whenever a thread starts. The calls are made from the thread's context,
and can be used to control additional thread properties not handled inside
EPICS base, e.g. setting the scheduling policy or CPU affinity (on SMP
systems).

The API also supports a mapping operation, calling a user-supplied function
for every thread that is currently running.

### New scan rate units

Scan rates defined in the menuScan.dbd file may now be specified in seconds,
minutes, hours or Hertz, and plural time units will also be accepted (seconds
are used if no unit is mentioned in the choice string). At `iocInit` each
scan rate is compared with the OS's clock tick and a warning printed if the
rate is too fast or likely to be more than 10% different to the requested rate.
For example the rates given below are all valid, although non-standard (the
default menuScan choices that come with Base have not been changed):

```
    menu(menuScan) {
        choice(menuScanPassive,     "Passive")
        choice(menuScanEvent,       "Event")
        choice(menuScanI_O_Intr,    "I/O Intr")
        choice(menuScan1_hour,      "1 hour")
        choice(menuScan0_5_hours, "0.5 hours")
        choice(menuScan15_minutes, "15 minutes")
        choice(menuScan5_minutes,   "5 minutes")
        choice(menuScan1_minute,    "1 minute")
        choice(menuScan10_seconds, "10 seconds")
        choice(menuScan5_seconds,   "5 seconds")
        choice(menuScan2_seconds,   "2 seconds")
        choice(menuScan1_second,    "1 second")
        choice(menuScan2_Hertz,     "2 Hertz")
        choice(menuScan5_Hertz,     "5 Hertz")
        choice(menuScan10_Hertz,   "10 Hz")
    }
```

### Alarm filtering added to input record types

The record types ai, calc, longin and mbbi have a new alarm filter added to
them. This provides a low-pass filter that can be used to delay the reporting of
alarms caused by the input level passing the HIGH, HIHI, LOW or LOLO values. The
filter is controlled with a new AFTC field that sets the filter's time constant.
The default value for this field is zero, which keeps the record's original
alarm behaviour.

The record must be scanned often enough for the filtering action to work
effectively and the alarm severity can only change when the record is processed,
but that processing does not have to be regular; the filter uses the time since
the record last processed in its calculation. Setting AFTC to a positive number
of seconds will delay the record going into or out of a minor alarm severity or
from minor to major severity until the input signal has been in that range for
that number of seconds.

### Post events on Waveform record's NORD field

When the record type or device support modify the NORD field of a waveform
record, the record support code now posts `DBE_VALUE` and `DBE_LOG` events for that
field, signaling the array length change to any client monitoring the NORD
field.

### Attributes of Non-VAL Fields

Non-VAL fields now report meaningful information for precision, units,
graphic limits, control limits, and alarm limits instead of simply using
PREC, EGU, HOPR, LOPR, DRVL, DRVH, HIHI, HIGH, LOW, and LOLO. All delay
fields have a default precision of 2 digits, units "s" and control limits
of 0 to 100,000 seconds (these precision and limit values can be changed
for each record type as a whole at runtime by updating a registered global
variable). Input fields like A-L of the calc record read their metadata
from the corresponding INPn link if possible.

#### epicsStdioRedirect.h merged into epicsStdio.h

The definitions from the header file epicsStdioRedirect.h have been moved
into epicsStdio.h so all calls to printf(), puts() and putchar() in files that
include that OSI header will now be subject to stdout redirection. In past
releases (3.14.7 and later) it was necessary to request the redirection support
by including the epicsStdioRedirect.h header file. The header file is still
provided, but now it just includes epicsStdio.h.

#### Named Soft Events

Soft events can now be given meaningful names instead of just using the
numbers 1-255. The EVNT field is now a `DBF_STRING`. The `post_event()` API
is now deprecated but still works. It should be replaced by code that in advance
looks up the `EVNTPVT` event handle associated with the named event by
calling `eventNameToHandle(char *)`, and when that event occurs passes
that handle to the new `postEvent(EVNTPVT)` routine (which may be called
from interrupt level). A new iocsh command `postEvent name` will
trigger a named event from the command-line or a startup script (on vxWorks the
expression `postEvent(eventNameToHandle("name"))` must be used
instead though).

#### Parallel Builds

As EPICS sites get computers with more CPUs they report additional bugs in our
parallel build rules. Various issues have been fixed by separating out the build
rules that generate dependency (.d) files, ensuring that they are constructed at
the appropriate time in the build.

These rule changes can cause additional warning messages to appear when building
support modules. Where an application provides its own Makefile rules it may now
have to add rules to construct an associated dependency file. In many cases
though the change needed is just to replace a dependency for a
`target$(OBJ)` with the `target$(DEP)` so this

```
    myLib$(OBJ): myLib_lex.c
```

becomes

```
    myLib$(DEP): myLib_lex.c
```

To debug build issues associated with dependency files, use the command
`make --debug=m` which tells GNUmake to display information about what it is doing
during the first pass when it updates its makefiles.

### Removed tsDefs.h

The deprecated tsDefs API was provided for 3.13 compatibility only, and has now
been removed. Convert any remaining code that used it to call the epicsTime API
instead.

### Changes to epicsVersion.h

The two macros `EPICS_UPDATE_LEVEL` and `EPICS_CVS_SNAPSHOT` have
been deleted from the epicsVersion.h file; they were deprecated in R3.14 and can
be replaced with `EPICS_PATCH_LEVEL` and `EPICS_DEV_SNAPSHOT`
respectively.

A new pair of macros has been added to make version number comparisons easier.
Code that will not work with a version of Base before 3.15.0 can now be
written like this to prevent it from compiling:

```
    #if defined(VERSION_INT) && EPICS_VERSION_INT < VERSION_INT(3,15,0,0)
    #  error EPICS Base R3.15.0 or later is required
    #endif
```

### Added support for iocLogPrefix

Added a `iocLogPrefix` command to `iocsh`. This adds a
prefix to all messages from this IOC (or other log client) as they get sent to the
iocLogServer. This lets sites use the `fac=<facility>` syntax for
displaying the facility, process name etc. in log viewers like the
`cmlogviewer`.

### Reworked the epicsEvent C & C++ APIs

  - Renamed the enum epicsEventWaitStatus to epicsEventStatus
  - Defined epicsEventWaitStatus as a macro for epicsEventStatus
  - Renamed epicsEventWaitOk to epicsEventOk
  - Renamed epicsEventWaitError to epicsEventError
  - Defined epicsEventWaitOK and epicsEventWaitError as macros
  - Added epicsEventTrigger(id) which triggers an event and returns OK or an
    error status if the underlying OS primitives report an error
  - Added epicsEventMustTrigger(id) which halts on error
  - Defined epicsEventSignal(id) as a macro for epicsEventMustTrigger(id)
  - Added a new C++ method epicsEvent::trigger() which throws an
    epicsEvent::invalidSemaphore in the event of an error
  - epicsEvent::signal() makes an inline call to epicsEvent::trigger()
  - epicsEventWait() and epicsEventWaitWithTimeout() now return an error
    status if the underlying OS primitives report an error
  - All the epicsEventMust...() routines are now implemented in the common
    libCom/osi/epicsEvent.cpp source file, and call cantProceed() instead of
    mis-using assert()
  - Implemented epicsEventShow() on Posix
  - Win32: Removed all epicsShareAPI decorations

### Enabled histogram record type

The histogram record was not included in the base.dbd file in any 3.14 release,
but has now been added along with its associated soft device support. The build
system now generates the list of all the record.dbd files in base automatically
in src/std/rec/Makefile.

### Reorganization of src/

Reorganization of subdirectories of src/ to better represent the relation
between different parts as described in the following table.

This change also allows the number of libraries built to be reduced to:
libCap5.so, libca.so, libdbCore.so, libdbStaticHost.so,
libCom.so, libcas.so, libdbRecStd.so, and libgdd.so

| Component          | Dependency        | Library name | Description                                           |
| ------------------ | ----------------- | ------------ | ----------------------------------------------------- |
| src/tools          |                   |              | Build system scripts                                  |
| src/libCom         | src/tools         | Com          | Utility routines and OS-independant API               |
| src/template       | src/tools         |              | User application templates (e.g. makeBaseApp)         |
| src/ca/client      | src/libCom        | ca           | Channel Access client                                 |
| src/ca/legacy/gdd  | src/ca/client     | gdd          | Generic data layer for PCAS                           |
| src/ca/legacy/pcas | src/ca/legacy/gdd | cas          | Portable Channel Access Server                        |
| src/ioc            | src/ca            | dbCore       | Core database processing functions                    |
| src/std            | src/ioc           | dbRecStd     | Standard records, soft device support and the softIoc |

In order to better reflect these relations the following
directories and files were moved as described:

#### Relocations

Category | Previous | New
:--- | :--- | :---
libCom | srcRTEMS | src/libCom/RTEMS
| | src/toolsComm/flex | src/libCom/flex |
| | src/toolsComm/antelope | src/libCom/yacc
| | src/dbStatic/alarm.h | src/libCom/misc
| | .../alarmString.h | src/libCom/misc
IOC Core Components | src/bpt | src/ioc/bpt
| | src/db | src/ioc/db
| | src/dbStatic | src/ioc/dbStatic
| | src/dbtools | src/ioc/dbtemplate
| | src/misc | src/ioc/misc
| | src/registry | src/ioc/registry
| | src/rsrv | src/ioc/rsrv [1](#rsrv)
Standard Record Definitions | src/dev/softDev | src/std/dev
| | src/rec | src/std/rec
| | src/softIoc | src/std/softIoc
Channel Access | src/ca | src/ca/client
| | src/catools | src/ca/client/tools
| | src/cap5 | src/ca/client/perl
| | src/gdd | src/ca/legacy/gdd
| | src/cas | src/ca/legacy/pcas
| | src/excas | src/ca/legacy/pcas/ex
User Templates | src/makeBaseApp | src/template/base
| | src/makeBaseExt | src/template/ext
Dispersed | src/util [2](#util) | src/ca/client
| | | src/ca/client/test
| | | src/libCom/log
| | src/as [3](#as) | src/libCom/as
| | | src/ioc/as

<span id="rsrv">1</span>
RSRV is built as part of dbCore due to its tight (bidirectional) coupling
with the other database code.

<span id="util">2</span>
The contents for src/util/ moved to three locations. The caRepeater init script
was moved to src/ca/client/. `ca_test` is now in src/ca/client/test/.
The iocLogServer was moved into the same directory (src/libCom/log) as
the log client code.

<span id="as">3</span>
The Access Security code has been divided, with the parts not related to the
database (lexer/parser and trap registration) becoming part of libCom.
The remaining components are included in the dbCore library

### Moved src/RTEMS/base directory

These files are now found under src/RTEMS.

### Removed 3.13 compatibility

Removed the 3.13 `<top>/config` directory and build compatibility rules and
variables, and various conversion documents.
