# Release Notes

This document describes changes that were included in the release of
EPICS listed below.
Notes from earlier EPICS releases are now provided in a separate document for
each version in the EPICS 7 series to date.
Release documents are also included for the older Base 3.15 and 3.16 series.

## EPICS Release 7.0.2.1

### Linking shared libraries on macOS

The linker flag `-flat_namespace` has been restored for creating shared
libraries, although not for loadable libraries (bundles). This was required
for building using the latest versions of Apple XCode.

### Fix `DB_LINK` loop breaking

A regression was introduced in 7.0.2 which caused record chains with loops to
be incorrectly broken. Processing should be skipped when a `DB_LINK` with
Process Passive (PP) closes a loop to a synchronous record.

Instead in 7.0.2 the targeted record would be processed if processing began
with a remote action (or some other caller of `dbPutField()`). This would
result in the loop running a second time. The loop would be broken on the
second iteration.

[See lp: #1809570](https://bugs.launchpad.net/epics-base/+bug/1809570)

### Old dbStaticLib APIs removed

Support for some obsolete dbStaticLib Database Configuration Tool (DCT) APIs
was removed some time ago, but vestiges of them still remained. The following
routines and macros and have now finally been removed:

  * `int dbGetFieldType(DBENTRY *pdbentry)`
  * `int dbGetLinkType(DBENTRY *pdbentry)`
  * `DCT_STRING`
  * `DCT_INTEGER`
  * `DCT_REAL`
  * `DCT_MENU`
  * `DCT_MENUFORM`
  * `DCT_INLINK`
  * `DCT_OUTLINK`
  * `DCT_FWDLINK`
  * `DCT_NOACCESS`
  * `DCT_LINK_CONSTANT`
  * `DCT_LINK_FORM`
  * `DCT_LINK_PV`

### Fix for `dbhcr` before `iocInit`

The `dbhcr` command used to work before `iocInit` as well as afterwards. It
displays all records that have hardware addresses (`VME_IO`, `CAMAC_IO`,
`GPIB_IO`, `INST_IO` etc.) but stopped working if run before iocInit due to the
rewrite of the link address parser code in dbStaticLib. This release fixes that
issue, although in some cases the output may be slightly different than it used
to be.
