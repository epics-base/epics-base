# Contribution guidelines

```{tags} beginner, user, developer, advanced
```

This document aims to show to software developers
how to get the current EPICS Base code, modify it
and create pull request with the changes.

## Organization of the EPICS Git Repository

The main EPICS repositories are hosted on Github: [github.com/epics-base](https://github.com/epics-base/). 
The source code of EPICS Base is at [github.com/epics-base/epics-base](https://github.com/epics-base/epics-base).

A mirror of the repository is available on Launchpad: <https://git.launchpad.net/epics-base>.

All current and past EPICS Base versions are in the same repository on different branches.

### EPICS 7

The current development branch is `7.0`.
However this branch only acts as a kind of envelope.
The actual EPICS 7 code is divided into modules,
each included as a Git submodule.

### Older EPICS Versions

There is a separate branch for each of the older EPICS Base versions:
3.13, 3.14, 3.15, and 3.16. (However there is no 3.12 branch.)

Use these to check out the latest developments of one of the older versions,
for example to fix a bug in one of those versions.

### Specific Releases

Individual releases as well as pre-releases and
release candidates are tagged like R3.16.1, R7.0.1-pre or R7.0.1-rc1.

## Making Changes

Changes should always be made against the head of the relevant branch,
not against the release tags.

For bug fixes check out the branch where the bug appears first.
The fix will be merged into newer EPICS versions by the core developer team.

For new features better announce your idea on the <core-talk@aps.anl.gov> mailing list 
and ask which branch is most appropriate.
For revolutionary new features it is probably the EPICS 7 master branch
respectively the branch of the submodule as referenced in the .gitmodule file.

### Forking the Repository

To be able to contribute any changes,
you should first [create a fork](https://docs.github.com/pull-requests/collaborating-with-pull-requests/working-with-forks/fork-a-repo) of EPICS base
or the relevant submodule.
If you want to work on an older version,
make sure to fork all existingg branches.
For contributions to EPICS 7 you will only need to fork the master branch.

### Cloning the Code

After forking you will be able to clone your fork,
including all submodules:

```bash
git clone --recurse-submodules git@github.com:<your-Github-space>/epics-base.git
```

If you want to work on an older version you can clone the specific branch via:

```bash
git clone --branch 3.14 git@github.com:<your-Github-space>/epics-base.git
```

### Maintaining Compatibility

Build and test your changes on as many systems as possible.
Important operating systems are Linux, Windows, OS X, vxWorks and RTEMS.

Keep in mind that in particular vxWorks 5 uses old compiler versions.
Do not break working systems with dependencies on new compiler versions.
This means for example C++ 11 features.

EPICS up to 3.15 works with vxWorks 5.5 
which uses gcc 3.3.2 with a quite old C++ implementation.
EPICS 3.16 works with vxWorks 6.3 using gcc 3.4.4.
Do not break that!

### Testing

All new features must come with automated tests to prove their functionality.
This also helps to find out if future changes break existing features.

There are several “test” directories.
Choose the one appropriate for the test.
Keep in mind that some tests may run before all parts of Base are built.
Details vary depending on the EPICS Base version.

EPICS Base comes with a testing framework which allows to run IOCs, 
set and read/compare values and more.

To add a test, you will typically create a xxxTest.c
and probably some records in a xxxTest.db file. 
(Choose a suitable name.)
Also you need to edit the Makefile in the test directory
as well as a file with a name like "epicsRun*Tests.c" to include your new test.

Here is a basic example of a test code (xxxTest.c):

```c
#include "dbAccess.h"
#include "dbUnitTest.h"
#include "testMain.h"      
MAIN(xxxTest) {
    epicsUInt32 value;
    
    /* Announce how many test will be done, see comments below. */
    testPlan(total_number_of_tests);

    testdbPrepare();

    /* Load your own IOC or one of the provided. */
    /* "dbTestIoc" or "recTestIoc" may be suitable. */
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    /* Load your records */
    testdbReadDatabase("xxxTest.db", NULL, "MACRO=VALUE");

    /* start up IOC */
    testIocInitOk();

    /* You may structure the test output with your own comments
    * (This does not count as a test.)
    */
    testDiag("##### This text goes to the test log #####");

    /* Set values and check for success. Counts as 1 test.
    * Make sure that DBF type matches your variable
    */
    testdbPutFieldOk("record.FIELD", DBF_ULONG, value);
    
    /* Get value and compare with expected result. Counts as 1 test.
    * Make sure that DBF type matches your variable
    */
    testdbGetFieldEqual("record.FIELD", DBF_ULONG, value);

    /* Do some arbitrary test. Counts as 1 test. */
    testOk(condition, formatstring, ...);
        
    /* The same without your own message. Counts as 1 test. */
    testOk1(condition);
        
    /* Finish */
    testIocShutdownOk();
    testdbCleanup();
    return testDone();
}
```

Your test should run (and succeed) when you execute

```bash
make runtests
```

## Merging Your Work into EPICS Base

Before creating a pull request,
you should first [synchronize your fork](https://docs.github.com/pull-requests/collaborating-with-pull-requests/working-with-forks/syncing-a-fork),
pull the latest changes
and rebase your changes on top of it, if necessary.

You can then proceed to create a [pull request](https://docs.github.com/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/creating-a-pull-request-from-a-fork).
The core developer team will review your changes,
suggest changes, highlight issues
or merge your code into EPICS base.
You can push updates at any time,
even after making a pull request.
The updates will automatically be part of the pull request.
Do **not** create a new pull request,
if you want to update your contribution.
