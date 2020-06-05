/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>

#include <epicsUnitTest.h>
#include <testMain.h>

#include <osiFileName.h>

MAIN(testexecname)
{
    testPlan(1);

    {
        char *buf = epicsGetExecName();
        if(!buf) {
            testSkip(1, "epicsGetExecName() not available for this target");
        } else {
            char *loc = strstr(buf, "testexecname");
            testOk(!!loc, "Find \"testexecname\" in \"%s\"", buf);
        }
    }

    return testDone();
}
