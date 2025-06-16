# Release Notes

This document describes the changes that were included in the release of EPICS
noted below. Release entries are now provided in a separate document for each
version in the EPICS 7 series, but all are combined into a single page for
publishing on the EPICS website. Separate release documents are also included
from the older Base 3.15 and 3.16 series.

## EPICS Release 7.0.4.1

### ARM Architecture Changes

Build configuration files for a new cross-build architecture `linux-aarch64`
have been added, and the targets `linux-arm_el` and `linux-arm_eb` removed.
The 64-bit ARM architecture target doesn't have build files for self-hosting
yet but they should be relatively easy to add, contributions welcome!

### Bug fixes

The following bugs/issues have fixes included in this release:

- [lp: 1884339](https://bugs.launchpad.net/epics-base/+bug/1884339),
  Inaccessible CA servers on Windows
- [github: 83](https://github.com/epics-base/epics-base/issues/83)
  osdTimeGetCurrent doesn't work for subprocess on macOS
- Recent Cygwin build problem with a missing `TCP_NODELAY` declaration.

### Perl CA Bindings under Conda

Builds of the Perl CA bindings weren't working properly when the Perl
installation was from Conda. This release also fixed the capr.pl script
to handle the INT64 data types, and to be able to properly handle missing
fields, as happens if the IOC is running an older EPICS version for example.

### epicsMessageQueue implementation on RTEMS

The implementation of the `epicsMessageQueue` used on RTEMS has switched from
the native RTEMS-specific one to the EPICS generic version, avoiding a bug
in the RTEMS Kernel message queue code.

### Record Name Validation

Historically, there have been very few restrictions on which characters
may be present in record and alias names.  Base 3.14.12.3 added a warning
for names containing space, single or double quote, period/dot, or
dollar sign.

```
Bad character ' ' in record name "bad practice"
```

7.0.4.1 Turns this warning into an error, and adds a new warning
if a record name begins with a minus, plus, left square bracket,
or left curly bracket.
