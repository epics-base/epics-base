/*************************************************************************\
* Copyright (c) 2019 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

#include <osiUnistd.h>
#include <osiFileName.h>
#include <dbDefs.h>
#include <iocsh.h>
#include <libComRegister.h>
#include <dbmf.h>

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

const iocshFuncDef testHelpFunction1Def = {"testHelpFunction1",0,0,
                                    "Usage message of testHelpFunction1\n"};
const iocshFuncDef testHelpFunction2Def = {"testHelpFunction2",0,0,
                                    "Usage message of testHelpFunction2\n"};
const iocshFuncDef testHelpFunction3Def = {"testHelpFunction3",0,0,
                                    "Usage message of testHelpFunction3\n"};
void doNothing(const iocshArgBuf *args)
{
    return;
}

std::string readFile(std::string filename)
{
    std::ifstream t(filename.c_str());
    std::stringstream buffer;

    if (!t.is_open()) {
        throw std::invalid_argument("Could not open filename " + filename);
    }

    buffer << t.rdbuf();
    return buffer.str();
}

bool compareFiles(const std::string& p1, const std::string& p2)
{
  std::ifstream f1(p1.c_str(), std::ifstream::binary|std::ifstream::ate);
  std::ifstream f2(p2.c_str(), std::ifstream::binary|std::ifstream::ate);

  if (f1.fail() || f2.fail()) {
    testDiag("One or more files failed to open");
    testDiag("f1.fail(): %d f2.fail(): %d", f1.fail(), f2.fail());
    return false; // File problem
  }

  if (f1.tellg() != f2.tellg()) {
    testDiag("File sizes did not match");
    return false; // Size mismatch
  }

  // Seek back to beginning and use std::equal to compare contents
  f1.seekg(0, std::ifstream::beg);
  f2.seekg(0, std::ifstream::beg);

  bool are_equal = std::equal(
    std::istreambuf_iterator<char>(f1.rdbuf()),
    std::istreambuf_iterator<char>(),
    std::istreambuf_iterator<char>(f2.rdbuf())
  );

  if (! are_equal) {
    testDiag("File contents did not match");

    std::string line;
    f1.seekg(0, std::ifstream::beg);
    f2.seekg(0, std::ifstream::beg);

    testDiag("File1 contents: ");
    while(std::getline(f1, line)) {
        testDiag("%s", line.c_str());
    }

    testDiag("File2 contents: ");
    while(std::getline(f2, line)) {
        testDiag("%s", line.c_str());
    }
  }

  return are_equal;
}


void testHelp(void)
{
    testDiag("iocshTest testHelp start");

    // Filename to save help output to
    const std::string filename = "testHelpOutput";

    // Verify help lists expected commands
    iocshCmd(("help > " + filename).c_str());
    std::string contents = readFile(filename);
    testOk1(contents.find("help") != std::string::npos);
    testOk1(contents.find("testHelpFunction1") != std::string::npos);
    testOk1(contents.find("testHelpFunction2") != std::string::npos);
    testOk1(contents.find("testHelpFunction3") != std::string::npos);

    // Confirm formatting of a single command
    iocshCmd(("help testHelpFunction1 > " + filename).c_str());
    testOk1(compareFiles(filename, "iocshTestHelpFunction1") == true);

    // Confirm formatting of multiple commands
    iocshCmd(("help testHelp* > " + filename).c_str());
    testOk1(compareFiles(filename, "iocshTestHelpFunctions") == true);

    remove(filename.c_str());
}

} // namespace



MAIN(iocshTest)
{
    testPlan(25);
    libComRegister();
    iocshRegister(&positionFuncDef, &positionCallFunc);
    iocshRegister(&assertFuncDef, &assertCallFunc);
    iocshRegister(&testHelpFunction1Def, &doNothing);
    iocshRegister(&testHelpFunction2Def, &doNothing);
    iocshRegister(&testHelpFunction3Def, &doNothing);
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

    testHelp();

    // cleanup after macLib to avoid valgrind false positives
    dbmfFreeChunks();
    return testDone();
}
