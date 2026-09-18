/*************************************************************************\
* Copyright (c) 2026  Jerzy Jamroz
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
Behavioral tests for the calc/calcout record.
*/

#include "dbAccess.h"
#include "dbTest.h"
#include "dbUnitTest.h"
#include "epicsStdio.h"
#include "errlog.h"
#include "testMain.h"

#include <string>

extern "C" {
void recTestIoc_registerRecordDeviceDriver(struct dbBase *);
}

static std::string pv(const char *rec, const char *field)
{
    return std::string(rec) + "." + field;
}

static void testPutOk(const std::string &pvname, double v)
{
    testDiag("put %s (double) = %.17g", pvname.c_str(), v);
    testdbPutFieldOk(pvname.c_str(), DBF_DOUBLE, v);
}

static void testPutOk(const std::string &pvname, const char *v)
{
    testDiag("put %s (string) = %s", pvname.c_str(), v);
    testdbPutFieldOk(pvname.c_str(), DBF_STRING, v);
}

static void processRecord(const char *rec)
{
    testDiag("process %s", rec);
    testdbPutFieldOk(pv(rec, "PROC").c_str(), DBF_LONG, 1);
}

static void setCalcAndProcess(const char *rec, const char *expr)
{
    testPutOk(pv(rec, "CALC"), expr);
    processRecord(rec);
}

static void record_expect_valid(const char *rec, double val)
{
    testDiag("%s expect valid", rec);

    testdbGetFieldEqual(pv(rec, "VAL").c_str(), DBF_DOUBLE, val);
    testdbGetFieldEqual(pv(rec, "SEVR").c_str(), DBF_STRING, "NO_ALARM");
    testdbGetFieldEqual(pv(rec, "STAT").c_str(), DBF_STRING, "NO_ALARM");
    testdbGetFieldEqual(pv(rec, "AMSG").c_str(), DBF_STRING, "");
    testdbGetFieldEqual(pv(rec, "NAMSG").c_str(), DBF_STRING, "");
    testdbGetFieldEqual(pv(rec, "UDF").c_str(), DBF_USHORT, 0);
}

static void record_expect_invalid_INPB(const char *rec)
{
    testDiag("%s expect invalid", rec);

    testdbGetFieldEqual(pv(rec, "SEVR").c_str(), DBF_STRING, "INVALID");
    testdbGetFieldEqual(pv(rec, "STAT").c_str(), DBF_STRING, "LINK");
    testdbGetFieldEqual(pv(rec, "AMSG").c_str(), DBF_STRING, "field INPB");
    testdbGetFieldEqual(pv(rec, "NAMSG").c_str(), DBF_STRING, "field INPB");
}

static void test_broken_inp_link(const char *rec)
{
    testDiag("TEST %s broken input link", rec);

    testPutOk(pv(rec, "INPA"), "srcA.VAL NPP NMS");
    testPutOk(pv(rec, "INPB"), "srcB.VAL NPP NMS");
    testPutOk(pv(rec, "SCAN"), "Passive");

    testDiag("CASE %s: normal calculation", rec);
    setCalcAndProcess(rec, "A+B");
    record_expect_valid(rec, 22.0);

    testDiag("CASE %s: break INPB", rec);
    testPutOk(pv(rec, "INPB"), "__no_pv__ NPP NMS");

    testDiag("CASE %s: broken INPB but expression uses only A", rec);
    setCalcAndProcess(rec, "A");
    record_expect_invalid_INPB(rec);

    testDiag("CASE %s: broken INPB but ternary selects A", rec);
    setCalcAndProcess(rec, "1>0?A:B");
    record_expect_invalid_INPB(rec);

    testDiag("CASE %s: broken INPB and expression requires B", rec);
    setCalcAndProcess(rec, "A+B");
    record_expect_invalid_INPB(rec);

    testDiag("CASE %s: broken INPB and expression is B only", rec);
    setCalcAndProcess(rec, "B");
    record_expect_invalid_INPB(rec);
}

MAIN(calcTest)
{
    testPlan(74);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("calcTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    // Prepare inputs
    testPutOk("srcA.VAL", 10.0);
    testPutOk("srcB.VAL", 12.0);
    // System under test
    test_broken_inp_link("sutCalc");
    test_broken_inp_link("sutCalcout");

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
