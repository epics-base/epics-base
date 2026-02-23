# Contribution guidelines

For bug fixes check out the branch where the bug appears first.
The fix will be merged into newer EPICS versions by the core developer team.

For new features, please register an issue or discuss the idea on mailing lists
or in Matrix beforehand.

## Maintaining Compatibility

Build and test your changes on as many systems as possible.
Important operating systems are Linux, Windows, macOS, vxWorks and RTEMS.

## Testing

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

Your test should run (and succeed) when you execute:

```bash
make runtests
```

CI will run these tests automatically. All checks must pass before a
pull request can be merged.

## Review Process

The core developer team will review your changes, suggest changes, highlight
issues or merge your code into EPICS base.
You can push updates at any time, even after making a pull request.
The updates will automatically be part of the pull request.
Do **not** create a new pull request, if you want to update your contribution.
