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

## EPICS Release 7.0.11

The changes described below have been merged into EPICS
since the last published release.

### CA Server Hardening

In response to several independent reports of security vulnerabilities,
the CA server in the IOC (RSRV) will more strictly validate messages.
**These changes introduce an added risk of interoperability issues with
older CA clients.**

Sites using older versions of libca or CAJ,
or with other CA client implementations,
are advised to do extra testing prior to deploying this release.

### Add GDB Helper Script

A new [Debugging with GDB](gdb.md) document which is linked under the
"General Information" heading in the side-bar provides information on
how to debug C/C++ code in an IOC running on a workstation OS.

This release added a GDB helper script that pretty-prints some of the
common database types.
Tested with GDB 16.3.
Compatible with python 3.

### Fix undefined timestamp for NORD field

When monitoring non-val fields such as `NORD` for several record types updates
are not sent out correctly resulting in incorrect timestamps. This fixes this
for the record types `aai`, `aao`, `subArray`.

Note that support modules that already implement this in the device support
layer will need to be updated to avoid doubled monitor updates. If those modules
are to be built against older versions of EPICS base then they will need to use
the defined macros `AAO_POSTS_NORD`, `AAI_POSTS_NORD`, `SA_POSTS_NORD`, or
`WF_POSTS_NORD` as appropriate to detect this change.

### iocInit wait for local CA links to connect

PR [768](https://github.com/epics-base/epics-base/pull/768)

During iocInit(), wait for local CA links to connect.
With this change, database authors can be certain that
any local CA link is connected prior to `initHookAfterIocRunning`
and `field(PINI, RUNNING)`.

### epicsThreadGetCPUs and callbackParallelThreads

On systems that support it (e.g. recent Linux systems), `epicsThreadGetCPUs()`
now takes the current cpu affinity mask of the calling thread into acount.
That fixes the problem that an IOC which was started with a limited affinity
mask (e.g. using `taskset`) and that called `callbackParallelThreads` with an
argument of 0 in the startup script, had started more parallel callback
threads than it has CPU cores available.

The iocsh function `callbackParallelThreads` now also accepts positive or
negative percentages for `count`, referring to the availabple number of cores
and always rounding the number of used cores down with a minimum of 1.

#### Examples

```text
# use up to 25% of available cpu cores for callback threads
callbackParallelThreads 25%
# keep at least 10% of available cpu cores unused for callback threads
callbackParallelThreads -10%
```

### errlogInit can be run more than once

Previously, running `errlogInit` after initialisation would silently fail. It is
now possible to call `errlogInit` or `errlogInit2` multiple times in order to
increase the buffer size.

Note that it is not possible to _shrink_ the buffer size.

### Expose `dbCreateRecord` in IOC shell

Add a new IOC shell command `dbCreateRecord` allowing for records to be created
from the shell.

Only works when called before `iocInit` and will fail if called afterwards.

The pdbbase variable, a record type, and the new record's name should be passed
as arguments.

Example usage:

```sh
dbCreateRecord(pdbbase, "ai", "example:ai")
iocInit()
```

### AFTC for bi record

The bi record now also supports the AFTC parameter.
The AFTC field sets the time constant on a low-pass filter that delays the
reporting of limit alarms until the signal has been within the alarm range for
that number of seconds (the default AFTC value of zero retains the previous
behavior). One has to make sure that the bi record is processed at a reasonably fast rate 
when using AFTC, so that the record can respond to changes in the input signal
within the time constant.

### AFTC and LALM fix for mbbi record

Fixed two issues in the mbbi record where the AFTC and LALM fields were not operating as expected.

### Static network configuration for RTEMS targets using libBSD

GitHub [PR #853](https://github.com/epics-base/epics-base/pull/853)

RTEMS targets built against the libBSD network stack can now use a static
network configuration in addition to DHCP.

When a static IP address and netmask can be read from the board's boot
configuration, the first non-loopback interface is configured with that static
address; otherwise the target falls back to DHCP as before.

The boot configuration is read using the mechanism appropriate to the board:

 * MOTLOAD boards read the NVRAM Global Environment Variables (GEV), falling
   back to the parameters in the `mot-script-boot` boot script.
 * PPCBUG boards read their PPCBUG NVRAM.
 * ColdFire (`mcf528x`) boards read their bootloader environment variables.

### Template fix for modules with `cfg/RULES*` files

EPICS modules that installed custom build `RULES*` files into
`$(INSTALL_CFG)` weren't loading them properly when the module
was built with `INSTALL_LOCATION` set in `configure/CONFIG_SITE`
or `configure/CONFIG_SITE.local`.
This release provides a fix in the template `configure/CONFIG`
file to solve the issue for new modules created from the
`makeBaseApp` template.

Maintainers of old modules that provide special build rules for
IOC applications should check their `configure` directory.
Assuming a `configure/CONFIG` file is present, replace it with
the `templates/makeBaseApp/top/configure/CONFIG` file from this
release. If any changes were made to the old file they would
need to be merged into the new file.

If the module's `configure` directory contains a file named
`CONFIG_APP` and no `CONFIG_SITE` file, the application must
date back to before March 2006; the files in the `configure`
directory should all be replaced in this case.

### macOS: Drop the `-flat_namespace` linker flag

It's not clear when this flag was no longer required on macOS, we
tried dropping it
[in 2019](https://github.com/epics-base/epics-base/commit/cbb13bf6b170c6ba6660d594bdd9ca44a5bdaea5)
but had to [restore it](https://github.com/epics-base/epics-base/commit/8c3c5a9731887c6d8b1918394090564af38461b5).
XCode 15 seems a safe version though. See discussion in issue
[#895](https://github.com/epics-base/epics-base/issues/895) for
more details.

### Fix registration of DRVETs

The C++ code generated in an IOC's `RecordDeviceDriver.cpp` file can fail
to compile if the Makefile building the IOC defines the `USE_TYPED_DRVET`
macro. Compiler errors caused by this bug will look something like this:

```
test_registerRecordDeviceDriver.cpp:198:15: error:
typedef 'drvet' cannot be referenced with the 'struct' specifier
  198 | static struct drvet *drvsl[] = {
      |               ^
/epics/base-7.0/include/drvSup.h:38:21: note: declared here
   38 | typedef typed_drvet drvet;
      |                     ^
1 error generated.
```

[This PR](https://github.com/epics-base/epics-base/pull/902)
modifies the generated C++ code to resolve that problem, and adds a
driver to the example IOC template to catch regressions.

### pvAccess Module

Changes to this module since the previous release:

#### Release 7.1.9

- pvget: Add `-S` option to `pvget` and `pvmonitor` to
  stringify byte arrays (Jeremy Lorelli)


### pvData Module

Changes to this module since the previous release:

#### Release 8.0.9

- Add a max string len param to
  epics::pvData::SerializeHelper::deserializeString() (Andrew Johnson)
- Add method
  epics::pvData::PVStructure::Formatter::arrayAsString() (Jeremy Lorelli)


### pva2pva Module

Changes to this module since the previous release:

#### Release 1.4.2

- Minor bug fixes

-----

## EPICS Release 7.0.10

```{include} ../../RELEASE-7.0.10.md
:start-after: EPICS Release 7.0.10
```

-----

## EPICS Release 7.0.9

```{include} ../../RELEASE-7.0.9.md
:start-after: EPICS Release 7.0.9
```

-----

## EPICS Release 7.0.8.1

```{include} ../../RELEASE-7.0.8.1.md
:start-after: EPICS Release 7.0.8.1
```

-----

## EPICS Release 7.0.8

```{include} ../../RELEASE-7.0.8.md
:start-after: EPICS Release 7.0.8
```

-----

## EPICS Release 7.0.7

```{include} ../../RELEASE-7.0.7.md
:start-after: EPICS Release 7.0.7
```

-----

## EPICS Release 7.0.6.1

```{include} ../../RELEASE-7.0.6.1.md
:start-after: EPICS Release 7.0.6.1
```

-----

## EPICS Release 7.0.6

```{include} ../../RELEASE-7.0.6.md
:start-after: EPICS Release 7.0.6
```

-----

## EPICS Release 7.0.5

```{include} ../../RELEASE-7.0.5.md
:start-after: EPICS Release 7.0.5
```

-----

## EPICS Release 7.0.4.1

```{include} ../../RELEASE-7.0.4.1.md
:start-after: EPICS Release 7.0.4.1
```

-----

## EPICS Release 7.0.4

```{include} ../../RELEASE-7.0.4.md
:start-after: EPICS Release 7.0.4
```

-----

## EPICS Release 7.0.3.1

```{include} ../../RELEASE-7.0.3.1.md
:start-after: EPICS Release 7.0.3.1
```

-----

## EPICS Release 7.0.3

```{include} ../../RELEASE-7.0.3.md
:start-after: EPICS Release 7.0.3
```

-----

## EPICS Release 7.0.2.2

```{include} ../../RELEASE-7.0.2.2.md
:start-after: EPICS Release 7.0.2.2
```

-----

## EPICS Release 7.0.2.1

```{include} ../../RELEASE-7.0.2.1.md
:start-after: EPICS Release 7.0.2.1
```

-----

## EPICS Release 7.0.2

```{include} ../../RELEASE-7.0.2.md
:start-after: EPICS Release 7.0.2
```

-----

## EPICS Release 7.0.1.1

```{include} ../../RELEASE-7.0.1.1.md
:start-after: EPICS Release 7.0.1.1
```

-----

```{include} ../../RELEASE-3.16.md
:heading-offset: 1
```

-----

```{include} ../../RELEASE-3.15.md
:heading-offset: 1
```

