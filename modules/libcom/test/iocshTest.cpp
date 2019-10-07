/*************************************************************************\
* Copyright (c) 2019 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string>
#include <fstream>
#include <set>

#include <osiUnistd.h>
#include <osiFileName.h>
#include <dbDefs.h>
#include <iocsh.h>
#include <libComRegister.h>

#include <epicsUnitTest.h>
#include <testMain.h>

namespace {
void findTestData()
{
    const char *locations[] = {
        ".",
        "..",
        ".." OSI_PATH_LIST_SEPARATOR "O.Common",
        "O.Common",
    };

    for(size_t i=0; i<NELEMENTS(locations); i++) {
        std::string path(locations[i]);
        path += OSI_PATH_SEPARATOR "iocshTestSuccess.cmd";

        std::ifstream strm(path.c_str());
        if(strm.good()) {
            testDiag("Found test data in %s", locations[i]);
            if(chdir(locations[i])!=0)
                testAbort("Unable to cd \"%s\"", locations[i]);
            return;
        }
    }

    testAbort("Failed to find test data");
}

void testFile(const char *fname, bool expect=true)
{
    testDiag("run \"%s\"", fname);
    int err = iocsh(fname);
    testOk((err==0) ^ (!expect), "%s \"%s\" -> %d", expect ? "ran" : "expected error from", fname, err);
}

void testCmd(const char *cmd, bool expect=true)
{
    testDiag("eval \"%s\"", cmd);
    int err = iocshCmd(cmd);
    testOk((err==0) ^ (!expect), "%s \"%s\" -> %d", expect ? "eval'd" : "expected error from", cmd, err);
}

std::set<std::string> reached;
const iocshArg positionArg0 = {"position", iocshArgString};
const iocshArg * const positionArgs[1] = { &positionArg0 };
const iocshFuncDef positionFuncDef = {"position",1,positionArgs};
void positionCallFunc(const iocshArgBuf *args)
{
    testDiag("Reaching \"%s\"", args[0].sval);
    reached.insert(args[0].sval);
}

void testPosition(const std::string& pos, bool expect=true)
{
    testOk((reached.find(pos)!=reached.end()) ^ !expect,
           "%sreached position %s", expect ? "" : "not ", pos.c_str());
}

const iocshArg assertArg0 = {"condition", iocshArgInt};
const iocshArg * const assertArgs[1] = {&assertArg0};
const iocshFuncDef assertFuncDef = {"assert",1,assertArgs};
void assertCallFunc(const iocshArgBuf *args)
{
    iocshSetError(args[0].ival);
}

} // namespace

MAIN(iocshTest)
{
    testPlan(19);
    libComRegister();
    iocshRegister(&positionFuncDef, &positionCallFunc);
    iocshRegister(&assertFuncDef, &assertCallFunc);
    findTestData();

    testFile("iocshTestSuccess.cmd");
    testPosition("success");
    reached.clear();
    testPosition("success", false);

    testCmd("<iocshTestSuccess.cmd");
    testPosition("success");
    reached.clear();

    testCmd("no_such_command", false);
    testFile("no_such_file.cmd", false);
    testCmd("<no_such_file.cmd", false);

    testCmd("assert 0");
    testCmd("assert 1", false);

    testDiag("Check argument parsing errors");

    // iocshArgDouble
    testCmd("epicsThreadSleep foo", false);
    // iocshArgInt
    testCmd("epicsMutexShowAll foo", false);

    testDiag("successful indirection");

    testFile("iocshTestSuccessIndirect.cmd");
    testPosition("success");
    testPosition("after_success");
    reached.clear();

    testFile("iocshTestBadArgIndirect.cmd", false);
    testPosition("before_error");
    testPosition("after_error", false);
    testPosition("after_error_1", false);
    reached.clear();

    return testDone();
}
