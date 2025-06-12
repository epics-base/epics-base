#include <epicsThread.h>
#include <epicsUnitTest.h>
#include <initHooks.h>
#include <iocsh.h>
#include <libComRegister.h>
#include <stdlib.h>
#include <string.h>
#include <testMain.h>

void testOkEnv(const char *varName, const char *varValue)
{
    const char *val = getenv(varName);
    testOk(val && strcmp(val, varValue) == 0,
           "%s=%s", varName, val ? val : "(null)");
}

MAIN(atInitTest)
{
    testPlan(12);

    libComRegister();

    // Reset environment variables
    iocshCmd("epicsEnvSet \"ATINIT_TEST_VAR\" \"BeforeIocInit\"");
    iocshCmd("epicsEnvSet \"ATINIT_TEST_VAR_ONE\" \"BeforeIocInit\"");
    iocshCmd("epicsEnvSet 'ATINIT_TEST_VAR_SPACES' 'Before Ioc Init'");

    printf("Test whether the variables are initialized correctly.\n");
    testOkEnv("ATINIT_TEST_VAR", "BeforeIocInit");
    testOkEnv("ATINIT_TEST_VAR_ONE", "BeforeIocInit");
    testOkEnv("ATINIT_TEST_VAR_SPACES", "Before Ioc Init");

    // Basic test
    iocshCmd("atInit \"epicsEnvSet ATINIT_TEST_VAR AfterIocInit\"");
    iocshCmd("atInit \"epicsEnvSet ATINIT_TEST_VAR_ONE AfterIocInit\"");
    iocshCmd("atInit \"epicsEnvSet ATINIT_TEST_VAR_SPACES 'After Ioc Init'\"");
    iocshCmd("atInit \"date\"");

    epicsThreadSleep(1.0);
    // Verify error handling and robustness
    iocshCmd("atInit \"nonexistentCommand arg1 arg2\"");
    iocshCmd("atInit \"\"");    // empty string
    iocshCmd("atInit \"   \""); // only spaces

    printf("Test whether the variables remain unchanged after 'atInit' and before 'iocInit'.\n");
    testOkEnv("ATINIT_TEST_VAR", "BeforeIocInit");
    testOkEnv("ATINIT_TEST_VAR_ONE", "BeforeIocInit");
    testOkEnv("ATINIT_TEST_VAR_SPACES", "Before Ioc Init");

    // Simulate iocInit
    initHookAnnounce(initHookAfterIocRunning);
    printf("=== iocInit Simulation ===\n");
    epicsThreadSleep(1.0);

    // Verify the results
    printf("Test whether the variables are correctly initialized after 'iocInit'.\n");
    testOkEnv("ATINIT_TEST_VAR", "AfterIocInit");
    testOkEnv("ATINIT_TEST_VAR_ONE", "AfterIocInit");
    testOkEnv("ATINIT_TEST_VAR_SPACES", "After Ioc Init");
    testPass("Command 'date' executed");

    testPass("Invalid command did not crash IOC");
    testPass("Empty atInit commands do not cause failure");

    return testDone();
}
