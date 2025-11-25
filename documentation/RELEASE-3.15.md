# Changes incorporated from the 3.15 branch

## Changes from the 3.15 branch since 3.15.9

### Support for Apple M1/M2 (arm64) Processors

Thanks to Jeong Han Lee this release comes with build support for Apple's new
M1/M2 CPUs running macOS, using the target name `darwin-aarch64`.

### Set thread names on Windows

On MS Windows, epicsThread names are made available to the OS and debugger
using `SetThreadDescription()` if available as well as using the older
exception mechanism.

### Fix timers on MS Windows for non-EPICS threads

The waitable timer changes in 3.15.9 broke calls to `epicsThreadSleep()` and
similar routines that used timers (including `ca_pend_event()`) when made from
threads that were not started using the epicsThread APIs.
[This problem](https://github.com/epics-base/epics-base/pull/200)
[has now been fixed](https://github.com/epics-base/epics-base/pull/201).

## Changes made between 3.15.8 and 3.15.9

### Use waitable timers on Microsoft Windows

The `epicsEventWaitWithTimeout()` and `epicsThreadSleep()` functions have
been changed to use waitable timers. On Windows 10 version 1803 or higher
they will use high resolution timers for more consistent timing.

See [this Google Groups thread](https://groups.google.com/a/chromium.org/g/scheduler-dev/c/0GlSPYreJeY)
for a comparison of the performance of different timers.

### Build target for documentation

The build target `inc` now works again after a very long hiatus. It now
generates and installs just the dbd, header and html files, without compiling
any C/C++ code. This can be used to speed up CI jobs that only generate
documentation.

### Bug fixes

- The error status returned by a record support's `special()` method is now propagated out of the `dbPut()` routine again (broken since 3.15.0).
- [gh: #80](https://github.com/epics-base/epics-base/issues/80), VS-2015 and
later have working strtod()
- [lp: #1776141](https://bugs.launchpad.net/epics-base/+bug/1776141), Catch
buffer overflow from long link strings
- [lp: #1899697](https://bugs.launchpad.net/epics-base/+bug/1899697), Records
in wrong PHAS order

### Change to the `junitfiles` self-test build target

The names of the generated junit xml test output files have been changed
from `<testname>.xml` to `<testname>-results.xml`, to allow better
distinction from other xml files. (I.e., for easy wildcard matching.)

### Fixes and code cleanups

Issues reported by various static code checkers.

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
they can read junittest output instead of TAP `make junitfiles`) followed by
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

-----

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
Larry Hoff in 2009; it is unfortunate that it has taken 10 years for them to be
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

-----

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
these instructions, run `perl bin/<host>/dbdToHtml.pl -H`
or `perldoc bin/<host>/dbdToHtml.pl`.

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
unlike the EPICS code) so the IOC fails to start properly. The relevant EPICS
bind() checks have now been updated so the IOC will request that a dynamic port
number be allocated for this TCP socket instead when this happens.

### Checking Periodic Scan Rates

Code has been added to the IOC startup to better protect it against bad
periodic scan rates, including against locales where `.` is not
accepted as a decimal separator character. If the scan period in a menuScan
choice string cannot be parsed, the associated periodic scan thread will no
longer be started by the IOC and a warning message will be displayed at iocInit
time. The `scanppl` command will also flag the faulty menuScan value.

-----

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
a time provider does synchronize the IOC will subsequently pick up and use the
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
updated, although there should be no noticeable difference at most sites. One
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

-----

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

-----

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
