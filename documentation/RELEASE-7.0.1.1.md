# Release Notes

This document describes the changes that were included in the release of EPICS
noted below. Release entries are now provided in a separate document for each
version in the EPICS 7 series, but all are combined into a single page for
publishing on the EPICS website. Separate release documents are also included
from the older Base 3.15 and 3.16 series.

## EPICS Release 7.0.1.1

### Changed SIML failure behavior

A failure when fetching the simulation mode through `SIML` will not put the
record into INVALID alarm state anymore. Instead, as long as the record's
current alarm severity (`SEVR`)is `NO_ALARM`, its alarm status (`STAT`) will be
set to `LINK_ALARM` without increasing the severity. This allows clients to get
some notification of a failing or bad `SIML` link without otherwise affecting
record processing.

### `dbVerify()` has been restored to dbStaticLib

This routine was removed in Base-3.16.1 but has been reimplemented in this
release by special request. Note that the error message strings that it
returns when verification fails have changed, but are still designed for
display to the user.

### Simulation mode improvements

Records that support simulation mode have two new fields, `SSCN` (Simulation
Scan Mode) and `SDLY` (Simulation Delay). `SSCN` is a menu field that provides
an alternate value for the `SCAN` field to be used while the record is in
simulation mode. This is especially useful for I/O scanned records, for which
simulation mode was not working at all. Setting `SDLY` to a positive value
makes the record process asynchronously in simulation mode, with the second
stage processing happening after the specified time (in seconds).

### Extend the dbServer API with init/run/pause/stop methods

This change permits IOCs to be built that omit the CA server (RSRV) by
removing its registrar entry which is now provided in the new `rsrv.dbd` file.
Other server layers can be built into the IOC (alongside RSRV or in place of
it) by registering them in a similar manner. The dbServer API is documented
with Doxygen comments in the header file.

Specific IOC server layers can be disabled at runtime by adding their name to
the environment variable `EPICS_IOC_IGNORE_SERVERS` (separated by spaces if more
than one should be ignored).

### Grand source-code reorganization

EPICS 7.0.1 contains the IOC Database, RSRV server and the Channel Access
client code from EPICS Base 3.16.1 along with all the original record types
and soft device support, but GDD and the Portable Channel Access Server have
been unbundled and are now available separately. In their place we have
brought in the more recently written EPICS V4 C++ libraries (collectively
referred to as the PVA modules). The directory tree for EPICS is somewhat
larger as a result, and the original structure of the Base directories has
been split into 4 separate Git repositories. External modules should build
against this new structure with little or no changes needed, except that some
allowance may be needed for the merging of the V4 modules.

There should be rather more description and documentation of these changes
than is currently available, but as developers we generally much prefer to
write code than documentation. Send questions to the tech-talk mailing list
and we'll be happy to try and answer them!
