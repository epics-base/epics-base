/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsEnvTest.c */

/* Author:  Andrew Johnson
 * Date:    2013-12-13
 */

/* Check environment variable APIs.
 * TODO: Add tests for envDefs.h routines.
 *
 * The thread test is needed on VxWorks 6.x, where the OS can be
 * configured to maintain separate, totally independent sets
 * of environment variables for each thread. This configuration
 * is not supported by EPICS which expects child threads to at
 * least inherit the partent thread's environment variables.
 */

#include <stdlib.h>
#include <string.h>

#include "envDefs.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define PARENT "Parent"
#define CHILD "Child"

static void child(void *arg)
{
    const char *value = getenv(PARENT);

    if (!testOk(value && (strcmp(value, PARENT) == 0),
            "Child thread sees parent environment values")) {
#ifdef vxWorks
        testDiag("VxWorks image needs ENV_VAR_USE_HOOKS configured as FALSE");
#else
        testDiag("Check OS configuration, environment inheritance needed");
#endif
    }

    epicsEnvSet(CHILD, CHILD);
}

MAIN(epicsEnvTest)
{
    unsigned int stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    const char *value;

    testPlan(3);

    epicsEnvSet(PARENT, PARENT);

    value = getenv(PARENT);
    if (!testOk(value && (strcmp(value, PARENT) == 0),
            "epicsEnvSet correctly modifies environment"))
        testAbort("environment variables not working");

    epicsThreadCreate("child", 50, stackSize, child, NULL);
    epicsThreadSleep(0.1);

    value = getenv(CHILD);
    if (value && (strcmp(value, CHILD) == 0))
        testDiag("Child and parent threads share a common environment");

    value = getenv(PARENT);
    testOk(value && (strcmp(value, PARENT) == 0),
            "PARENT environment variable not modified");

    testDone();
    return 0;
}

