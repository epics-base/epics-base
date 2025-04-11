## EPICS Release 7.0.2

### Launchpad Bugs

The list of tracked bugs fixed in this release can be found on the
[Launchpad Milestone page for EPICS Base 7.0.2](https://launchpad.net/epics-base/+milestone/7.0.2).

### Git Branches Recombined

The four separate Git branches `core/master`, `libcom/master`, `ca/master` and
`database/master` have been recombined into one branch called `7.0`. Keeping
these as 4 separate branches in the same repository made it impossible to
create merge requests that contained changes in more than one of these
modules. The layout of the source files has not changed at all however, so the
source code for libcom, ca and the database are still found separately under
the module subdirectory.
