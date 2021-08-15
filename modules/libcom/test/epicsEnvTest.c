/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
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
 * least inherit the parent thread's environment variables.
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

static void testThreadEnv(void)
{
    unsigned int stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    const char *value;

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
}

static void testChangeEnv(void)
{
    const char *foo = "foo", *bar = "bar", *name = "testChangeEnv";
    const char *temp;
    testDiag("Changing env");

    temp = getenv("testChangeEnv");
    testOk(!temp, "temp=\"%s\"", temp);

    /* make sure that "foo" has been copied into environ instead of referencing
     * our string constant
     */
    epicsEnvSet(name, foo);
    temp = getenv("testChangeEnv");
    testOk(temp && temp!=foo && temp!=name && strcmp(temp, foo)==0,
           "env set temp=\"%s\" name=\"%s\" foo=\"%s\"", temp, name, foo);

    /* check the same when changing */
    epicsEnvSet(name, bar);
    temp = getenv("testChangeEnv");
    testOk(temp && temp!=foo && temp!=name && temp!=bar && strcmp(temp, bar)==0,
           "env change temp=\"%s\" name=\"%s\" foo=\"%s\" bar=\"%s\"", temp, name, foo, bar);

    epicsEnvUnset(name);

    temp = getenv("testChangeEnv");
    testOk(!temp, "temp=\"%s\"", temp);
}

MAIN(epicsEnvTest)
{

    testPlan(7);
    testThreadEnv();
    testChangeEnv();
    return testDone();
}

