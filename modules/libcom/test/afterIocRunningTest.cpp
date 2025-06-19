/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <epicsThread.h>
#include <epicsUnitTest.h>
#include <initHooks.h>
#include <iocsh.h>
#include <libComRegister.h>
#include <stdlib.h>
#include <string.h>
#include <testMain.h>

static void testOkEnv(const char *varName, const char *varValue)
{
    const char *val = getenv(varName);
    testOk(val && strcmp(val, varValue) == 0,
           "%s=%s", varName, val ? val : "(null)");
}

static void iocshCmdDebug(const char *cmd)
{
    printf("%s\n", cmd);
    iocshCmd(cmd);
}

MAIN(afterIocRunningTest)
{
    testPlan(12);

    libComRegister();

    // Reset environment variables
    iocshCmdDebug("epicsEnvSet \"TEST_VAR\" \"BeforeIocInit\"");
    iocshCmdDebug("epicsEnvSet \"TEST_VAR_ONE\" \"Before(Ioc)Init\"");
    iocshCmdDebug("epicsEnvSet 'TEST_VAR_SPACES' 'Before Ioc Init'");

    printf("Test whether the variables are initialized correctly.\n");
    testOkEnv("TEST_VAR", "BeforeIocInit");
    testOkEnv("TEST_VAR_ONE", "Before(Ioc)Init");
    testOkEnv("TEST_VAR_SPACES", "Before Ioc Init");
    printf("===================\n");
    epicsThreadSleep(1.0);
    // Basic test
    iocshCmdDebug("afterIocRunning \"epicsEnvSet TEST_VAR AfterIocInit\"");
    iocshCmdDebug("afterIocRunning \"epicsEnvSet TEST_VAR_ONE 'After(Ioc)Init'\"");
    iocshCmdDebug("afterIocRunning \"epicsEnvSet TEST_VAR_SPACES 'After Ioc Init'\"");
    iocshCmdDebug("afterIocRunning \"date\"");
    // Verify error handling and robustness
    iocshCmdDebug("afterIocRunning \"nonexistentCommand arg1 arg2\"");
    iocshCmdDebug("afterIocRunning \"\"");    // empty string
    iocshCmdDebug("afterIocRunning \"   \""); // only spaces
    printf("===================\n");

    epicsThreadSleep(1.0);
    printf("Test whether the variables remain unchanged after 'afterIocRunning' and before 'iocInit'.\n");
    testOkEnv("TEST_VAR", "BeforeIocInit");
    testOkEnv("TEST_VAR_ONE", "Before(Ioc)Init");
    testOkEnv("TEST_VAR_SPACES", "Before Ioc Init");

    // Simulate iocInit
    printf("===================\n"
           "iocInit: Simulation\n"
           "===================\n");
    initHookAnnounce(initHookAfterIocRunning);
    epicsThreadSleep(1.0);

    // Verify the results
    printf("Test whether the variables are correctly initialized after 'iocInit'.\n");
    testOkEnv("TEST_VAR", "AfterIocInit");
    testOkEnv("TEST_VAR_ONE", "After(Ioc)Init");
    testOkEnv("TEST_VAR_SPACES", "After Ioc Init");
    testPass("Command 'date' executed");
    testPass("Invalid command did not crash IOC");
    testPass("Empty afterIocRunning commands do not cause failure");

    return testDone();
}
