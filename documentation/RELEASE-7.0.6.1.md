## EPICS Release 7.0.6.1

### `mbboDirectRecord` enhancements

The bit fields `B0` - `B1F` of this record are now always updated and have a
monitor posted when the `VAL` field is set and the record processed. It is now
possible to initialize the record's value by setting the bit fields inside a
database file as long as no other method was used to initialize it (suc as
setting `VAL` directly, using `DOL`, or by an initial readback from device
support). A new internal field `OBIT` was added to store information about
monitors posted on the bit fields.

### Minimum Perl Version is now 5.10.1

Some scripts now make use of features that were introduced to this Perl version
that was released in 2009.

### DB Links to `DBF_MENU` fields fixed

[GH:183](https://github.com/epics-base/epics-base/issues/183)
These were broken in a previous release, but now work again.

### Long String access to CALC fields fixed

[GH:194](https://github.com/epics-base/epics-base/issues/194)
This was broken in a previous release, but now works again.

### Minor Changes

+ Many code comments have been spell-checked and corrected.
+ Passing a `-DDEBUG` compiler flag no longer breaks the build.
+ Parallel builds of RTEMS-mvme2100 and RTEMS-mvme2700 targets now work.
+ Illegal characters seen in JSON strings in a database file should now get a
better error message.

### Other Launchpad Bugs and GitHub Issues Fixed

+ [lp:1938459](https://bugs.launchpad.net/epics-base/+bug/1938459)
  [GH:191](https://github.com/epics-base/epics-base/pull/191) int64in only
  checks lower 32 bits for change
+ [lp:1941875](https://bugs.launchpad.net/epics-base/+bug/1941875) Buggy
  warning message "Record/Alias name '...' should not contain non-printable ...
+ [GH:187](https://github.com/epics-base/epics-base/issues/187) waveformRecord
  missing PACT=true?
+ [GH:189](https://github.com/epics-base/epics-base/pull/189) Fix a couple
  memory leaks and a segfault
+ [GH:200](https://github.com/epics-base/epics-base/pull/200) and
  [GH:201](https://github.com/epics-base/epics-base/pull/201) Fix timers on MS
  Windows for non-EPICS threads

### Compiler interface for epicsAtomic tidied up

[GH:192](https://github.com/epics-base/epics-base/pull/192)
Both GCC and CLANG compiler intrisics used for the epicsAtomic APIs have been revised; implementations using CLANG should now run faster as they now use the compiler's built-in atomic functions instead of taking a mutex.

### The epicsTime code has been reimplemented

[GH:185](https://github.com/epics-base/epics-base/pull/185)
This was done to simplify the code and may have improved performance slightly for some uses. Support for the old NTP-specific `struct l_fp` has been dropped but all other routines and methods of the `class epicsTime` function as before.

### Updates to Record Reference documentation

Many of the built-in record types have had improvements to their documentation with additional fields added to the tables, rewrites of descriptions and links to other documents added or fixed.

### Submodule updates

The pvAccess module was updated to version 7.1.4:

- Changes to caProvider
  - Resolve issues with pv structures that don't have a value field
  - Add NULL checks for handling unusual structures
  - Speed up channel creation when using large numbers of channels
