/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdio.h>
#include <string.h>

#include "dbUnitTest.h"
#include "dbBase.h"
#include "dbAccessDefs.h"
#include "iocsh.h"
#include "testMain.h"

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

static int iocshOutput(const char *command, char *output, size_t size)
{
    static const char filename[] = "casmatchTestOutput";
    char line[1024];
    FILE *file;
    int status;
    size_t nread;

    snprintf(line, sizeof(line), "%s > %s", command, filename);
    status = iocshCmd(line);
    file = fopen(filename, "r");
    if (!file)
        return -1;
    nread = fread(output, 1, size - 1, file);
    output[nread] = '\0';
    fclose(file);
    remove(filename);
    return status;
}

MAIN(casmatchTest)
{
    char output[1024] = "";

    testPlan(8);
    testdbPrepare();
    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testOk(iocshCmd("casmatch \"*\"") == 0,
        "casmatch is registered with the IOC shell");
    testOk(iocshCmd("casmatch") == 0,
        "casmatch accepts a missing pattern for diagnostic output");
    testOk(iocshCmd("casmatch \"\"") == 0,
        "casmatch accepts an empty pattern for diagnostic output");
    testOk(iocshCmd("casmatch \"*\" -1") == 0,
        "casmatch accepts a negative level for diagnostic output");
    testOk(iocshOutput("help casmatch", output, sizeof(output)) == 0,
        "casmatch help is available");
    testOk(strstr(output, "0  - matching client and channel summary") != NULL,
        "casmatch help documents level 0");
    testOk(strstr(output, "1  - extends report with matching client information") != NULL &&
        strstr(output, "2  - extends report with matching channel names") != NULL,
        "casmatch help documents levels 1 and 2");
    testOk(strstr(output, "3  - extends report with channel and access information") != NULL &&
        strstr(output, "4  - extends report with connection diagnostics") != NULL &&
        strstr(output, "5+ - extends report with memory and lock diagnostics") != NULL,
        "casmatch help documents levels 3 through 5");
    testdbCleanup();
    return testDone();
}
