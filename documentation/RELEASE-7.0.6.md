## EPICS Release 7.0.6

### Support for obsolete architectures removed

These target architectures have been removed:

+ darwin-ppc, darwin-ppcx86
+ linux-386, linux-486, linux-586, linux-686, linux-athlon (cross-build)
+ linux-cris, linux-cris\_v10, linux-cris\_v32 (cross-build)
+ RTEMS-at91rm9200ek, RTEMS-gen68360, RTEMS-mcp750, RTEMS-mvme167,
RTEMS-psim (cross-build)

### Experimental Support for RTEMS 5

The new major release of the RTEMS real-time OS contains many changes
including the ability to support SMP systems. This release of EPICS
can still be built with RTEMS 4.9.x or 4.10.x and should work just
the same as earlier releases, although due to code having moved around
we recommend thorough testing before this release is first used in
production systems.

This release of EPICS comes with support for several new RTEMS targets
running on RTEMS 5:

- RTEMS-beagleboneblack
- RTEMS-pc686
- RTEMS-qoriq\_e500 (MVME2500)
- RTEMS-xilinx\_zynq\_a9\_qemu
- RTEMS-xilinx\_zynq\_zedboard

The EPICS support for RTEMS 4 has always relied on RTEMS-specific
kernel APIs which cannot be used on an SMP system, so a new port was
created to use the Posix real-time APIs that are now recommended for
RTEMS 5. Note that a single installation of EPICS cannot build both
RTEMS 4 and RTEMS 5 targets, if you need to support targets running
on both versions you must use a separate installation, and be sure
to run `make distclean` if switching a single source tree from one
to the other (both header files and dependency files are different
between the two and must be cleaned out).

The configuration variable `RTEMS_VERSION` in the EPICS config file
`configure/os/CONFIG_SITE.Common.RTEMS` must be set to the full 3-
part version number for RTEMS 4 releases, e.g. `4.9.1`, `4.10.2`
but for RTEMS 5.1 and later it must only contain the major version
number e.g. `5`.

Some RTEMS BSPs can be built and may work with the newer libbsd
network stack which RTEMS is moving over to, but most of the MVME
boards (and the uC5282) still require the legacy network stack.

The dependency on bspExt has been removed, EPICS now provides its
own routine for VMEbus probing (or uses one built into the BSP).

Anyone using this release on RTEMS is advised to discuss problems
building or running it on either the tech-talk or core-talk email
lists so the core developers can help with and find out about any
problems with the old or new port.

Known Issues:
- MVME2100 and MVME2700 need changes to the RTEMS 5 BSP to build.
- VMEBus support is not yet available for the MVME2500 BSP.
- There are some known issues with floating point on MVME2500,
  probably related to its newer e500 FPU.
- Changed network driver for beatnik to work with libbsd.  Some
  issues with DHCP, but network stack usable.  Can load env from
  NVRAM.

### `epicsEnvShow` accepts a glob pattern

The optional argument to epicsEnvShow can now be a glob pattern.

### New function `epicsStrnGlobMatch()`

The function `epicsStrnGlobMatch(char* str, size_t len, char* pattern)`
works exactly the same as `epicsStrGlobMatch()` but takes an additional
length arguments which limits the number of characters of `str` to match.

### Automatic fallback to thread when unable to exec caRepeater

A process using libca which does not find an existing caRepeater process
will attempt to start one by running the caRepeater executable.
This is not always possible, usually when caRepeater is not in `$PATH`.
Now, instead of printing a warning, an internal caRepeater thread
will be started (as is done be RTEMS and vxWorks targets).

If this fallback occurs, the lifetime of the caRepeater thread
may be shorter than the lifetime of a separate caRepeater process
would have been.

It remains the recommended practice to explicitly start a caRepeater
instance.  Examples of both systemd (`caRepeater.service`) and sysv
(`S99caRepeater`) scripts may be found under `bin/`.

### Glob pattern allowed in `var` command

When used with one argument, the `var` command can be used with a glob pattern
for printing matching variables.

### Formalize/fix `FINAL_LOCATION`

The `FINAL_LOCATION` make variable has for some time been an undocumented
means of performing a staged build.  This is a build which "installs" to
a temporary location, which will later be moved to a final location.

This has now been added to `configure/CONFIG_SITE`.

Usage analogous to the autotools recipe

```sh
./configure --prefix=/usr/lib/epics
make install DESTDIR=/tmp/build
```

would be

```sh
make INSTALL_LOCATION=/tmp/build FINAL_LOCATION=/usr/lib/epics
```

`FINAL_LOCATION` is now correctly used in systemd and sysv init scripts
`caRepeater.service`, `S99caRepeater`, and `S99logServer`.

### IOCsh sets `${PWD}`

IOC shell will now ensure `${PWD}` is set on startup,
and updated by the `cd` iocsh function.

### Add Alarm Message and Time Tag Fields

Two new fields have been added to `dbCommon` so will be present in all
records: `AMSG` and `UTAG`.

#### `AMSG`

`AMSG` can hold an arbitrary 40-character string, providing additional
information about the alarm condition indicated in `STAT` and `SEVR`. With no
alarm it will hold an empty string. The new `recGblSetSevrMsg()` function can
be used in place of `recGblSetSevr()` to signal an alarm while providing a
message.

For example, a device support's `read_bi()` routine for a hypothetical
multi-channel ethernet attached device might flag a communication error
between the IOC and controller, or an error involving a certain channel like
this:

```c
static long read_bi(biRecord* prec) {
    ...
    if (!priv->connected) {
        recGblSetSevrMsg(prec, COMM_ALARM, INVALID_ALARM,
            "No controller connected");
        return S_dev_noDevice;
    }
    if (!priv->err) {
        recGblSetSevrMsg(prec, READ_ALARM, INVALID_ALARM,
            "Channel %u disconnexted", priv->chan);
        return S_dev_noDevice;
    }
    return status;
}
```

#### `UTAG`

`UTAG` holds an `epicsUInt64` value which is semantically part of the record's
timestamp (`TIME`). The value defaults to zero if not explicitly set. Device
support or an event time provider which supports this feature may write a tag
value directly to the `dbCommon::utag` field.

`TSEL` links will copy both `TIME` and `UTAG` between records if the link type
supports this (CA links do not).

A `utag` server side channel filter has been added which can be configured to
filter out monitor updates which don't pass the test `(UTAG & M) == V` where
`M` and `V` are client specified integers. For example running the command
`camonitor BPM0:X.{utag:{M:1,V:1}}` will only show updates for which
`(UTAG & 1) == 1` i.e. the least significant bit of the `UTAG` field is set.

This feature is intended for use by intelligent devices which can provide
contextual information along with a value/alarm/time.  For example, a beam
diagnostic device which is aware of whether a beam signal should be present
(eg. from a global timing system).

#### Link Support

Two new optional methods have been added to the Link Support Entry Table
(`struct lset`): `lset::getAlarmMsg()` and `lset::getTimeStampTag()`. See
comments in dbLink.h for details on implementing these.

Two new accessor functions have also been added which call these methods:
`dbGetAlarmMsg()` and `dbGetTimeStampTag()`.

#### Compatibility

User code wishing to call these interfaces while maintaining compatibility with older
versions of Base may add some of the following macro definitions, and ensure
that the variables referenced by output pointers are initialized.

```c
#ifndef HAS_ALARM_MESSAGE
#  define recGblSetSevrMsg(REC, STAT, SEVR, ...) recGblSetSevr(REC, STAT, SEVR)
#endif
#ifndef dbGetAlarmMsg
#  define dbGetAlarmMsg(LINK, STAT, SEVR, BUF, BUFLEN) dbGetAlarm(LINK, STAT, SEVR)
#endif
#ifndef dbGetTimeStampTag
#  define dbGetTimeStampTag(LINK, STAMP, TAG) dbGetTimeStamp(LINK, STAMP)
#endif
```


### Timeouts for Unit Test Programs

The unit test programs that are run by the `make runtests` or `make tapfiles`
commands get executed by a `.t` wrapper script which is normally generated by
the EPICS `makeTestfile.pl` program. Those generated wrapper scripts now
impose a time-limit on the test program they execute, and will kill it if it
runs for longer than 500 seconds (8 minutes 20) without exiting. That
time-limit can be changed for any such test by modifying the Makefile which
creates and runs the `.t` wrapper script.

Setting the environment variable `EPICS_UNITTEST_TIMEOUT` to the desired
number of seconds while the Makefile is generating the test script changes the
timeout in that script. For example:

```
  TESTSCRIPTS_HOST += hourLongTest.t
  hourLongTest.t: export EPICS_UNITTEST_TIMEOUT=3600
```

When selecting such a timeout remember that different Continuous Integration
systems such as GitHub Actions and Appveyor run on processors with different
speeds, so allow enough head-room for slower systems to complete the test.

Test programs written directly in Perl as a `.plt` script should implement a
similar timeout for themselves. The "netget" test in Base does this in a way
that works on Windows as well as Unix-like hosts.

### Submodule updates

The pvAccess module was updated to version 7.1.4:

- Changes
  - Adjust argument parsing with pvput (Jesus Vasquez).

The pva2pva module was updated to version 1.3.1:

- Bug Fixes
 - Correct handling for server side filters.
- Changes
 - Syncing softMain.cpp with epics-base

The pvDatabase module was updated to version 4.6.0:

* Access Security is now supported.
* <b>special</b> has been revised and extended.
* addRecord, removeRecord, processRecord, and traceRecord are replaced by pvdbcr versions.
* <b>support</b> is DEPRECATED

The pvaClient module was updated to version 4.8.0:

* `PvaClientNTMultiData::getChannelChangeFlags` is a new method. It fixes
  issue #66.
* Fix for issue #68. Both `PvaClientArray` and `PvaClientField` are not longer
  present. Neither was previously implemented.
* Several public methods are now protected. They were never meant to be called
  by clients.
* Issue #70 has been fixed.
* Changes was made to increase the performance of `pvaMultiChannel`.
* doxygen changes were made.
