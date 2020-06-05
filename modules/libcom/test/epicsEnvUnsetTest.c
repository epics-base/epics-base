/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "macLib.h"
#include "envDefs.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

static void check(const char* variable, const char* expected)
{
    const char* value;

    value = getenv(variable);
    if (!testOk((!expected && !value) || (expected && value && strcmp(expected, value) == 0),
        "%s = \"%s\"", variable, value))
    {
        testDiag("should have been \"%s\"", expected);
    }
}

MAIN(epicsEnvUnsetTest)
{
    eltc(0);
    testPlan(3);

    check("TEST_VAR_A",NULL);
    epicsEnvSet("TEST_VAR_A","test value");
    check("TEST_VAR_A","test value");
    epicsEnvUnset("TEST_VAR_A");
    check("TEST_VAR_A",NULL);

    testDone();
    return 0;
}
