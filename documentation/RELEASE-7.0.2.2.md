# Release Notes

This document describes the changes that were included in the release of EPICS
noted below. Release entries are now provided in a separate document for each
version in the EPICS 7 series, but all are combined into a single page for
publishing on the EPICS website. Separate release documents are also included
from the older Base 3.15 and 3.16 series.

## EPICS Release 7.0.2.2

### Build System changes

 * The GNUmake build targets `cvsclean` and `depclean` are now available from
any directory; previously they were only available from application top
directories.

 * The approach that EPICS Base uses for building submodules inside the parent
module looks useful for support modules too. The rules for building submodules
have been modified and extracted into a new `RULES_MODULES` file, so a support
module will be able to use them too without having to copy them into its own
`modules/Makefile`. There are some specific requirements that support modules
and their submodules must follow, which are described as comments in the new
`base/configure/RULES_MODULES` file itself.

### `EPICS_BASE_VERSION` Update Policy change

In the past, a build of EPICS using sources checked out from the repository
branch between official releases would have shown the version number of the
previous release, followed by a -DEV suffix, for example 7.0.2.1-DEV.

The policy that controls when the number gets updated has been changed, and
now immediately after a release has been tagged the version number will be
updated to the next patch release version, plus the -DEV suffix as before.
Thus following 7.0.2.2 the version number will show as 7.0.2.3-DEV. This does
not require the next official release to be numbered 7.0.2.3 though, it could
become 7.0.3 or even 7.1.0 if the changes incorporated into it are more
substantial than bug fixes.

### Drop `CLOCK_MONOTONIC_RAW` from posix/osdMonotonic.c

Turns out this is ~10x slower to query than `CLOCK_MONOTONIC`.
