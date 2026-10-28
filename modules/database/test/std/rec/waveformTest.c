/*************************************************************************\
* Copyright (c) 2026 Grass Wang
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>

#include "epicsStdio.h"

#include "dbAccess.h"
#include "dbAccessDefs.h"
#include "dbEvent.h"
#include "dbUnitTest.h"
#include "errlog.h"
#include "menuFtype.h"
#include "testMain.h"
#include "waveformRecord.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void processWaveform(const char *record)
{
    char field[32];

    epicsSnprintf(field, sizeof(field), "%s.PROC", record);
    testdbPutFieldOk(field, DBF_LONG, 1);
}

static void putArray(const char *record, unsigned long count,
    const epicsInt32 *values)
{
    testdbPutArrFieldOk(record, DBF_LONG, count, values);
}

static void testSoftInput(void)
{
    static const epicsInt32 shortInput[] = {1, 2, 3};
    static const epicsInt32 longInput[] = {10, 20, 30, 40, 50, 60};
    static const epicsInt32 expectedLong[] = {10, 20, 30, 40};

    testDiag("soft input copies array and updates NORD");
    putArray("wfSource", NELEMENTS(shortInput), shortInput);
    processWaveform("wfSoft");
    testdbGetFieldEqual("wfSoft.NORD", DBF_ULONG, 3u);
    testdbGetArrFieldEqual("wfSoft", DBF_LONG, 4, NELEMENTS(shortInput),
        shortInput);
    testdbGetFieldEqual("wfSoft.UDF", DBF_UCHAR, 0);

    testDiag("soft input truncates to destination NELM");
    putArray("wfSource", NELEMENTS(longInput), longInput);
    processWaveform("wfSoft");
    testdbGetFieldEqual("wfSoft.NORD", DBF_ULONG, 4u);
    testdbGetArrFieldEqual("wfSoft", DBF_LONG, 4, 4, expectedLong);

    testDiag("soft input lowers NORD for a shorter source array");
    putArray("wfSource", NELEMENTS(shortInput), shortInput);
    processWaveform("wfSoft");
    testdbGetFieldEqual("wfSoft.NORD", DBF_ULONG, 3u);
    testdbGetArrFieldEqual("wfSoft", DBF_LONG, 4, NELEMENTS(shortInput),
        shortInput);
}

static void testArrayWrites(void)
{
    static const epicsInt32 longInput[] = {10, 20, 30, 40, 50, 60};
    static const epicsInt32 shortInput[] = {7, 8};
    static const epicsInt32 expectedLong[] = {10, 20, 30, 40};

    testDiag("array write is limited by NELM");
    putArray("wfMeta", NELEMENTS(longInput), longInput);
    testdbGetFieldEqual("wfMeta.NORD", DBF_ULONG, 4u);
    testdbGetArrFieldEqual("wfMeta", DBF_LONG, 4, 4, expectedLong);

    testDiag("shorter array write lowers NORD");
    putArray("wfMeta", NELEMENTS(shortInput), shortInput);
    testdbGetFieldEqual("wfMeta.NORD", DBF_ULONG, 2u);
    testdbGetArrFieldEqual("wfMeta", DBF_LONG, 4, 2, shortInput);
}

static void checkMonitorCount(testMonitor *monitor, unsigned expected,
    const char *name)
{
    testMonitorSync(monitor);
    testOk(testMonitorCount(monitor, 1) == expected,
        "%s monitor count is %u", name, expected);
}

static void testValueMonitors(void)
{
    static const epicsInt32 input[] = {1, 2, 3};
    static const epicsInt32 changed[] = {1, 9, 3};
    static const epicsInt32 shorter[] = {1, 9};
    testMonitor *monitor;

    putArray("wfSource", NELEMENTS(input), input);
    testdbPutFieldOk("wfSoft.MPST", DBF_ENUM, waveformPOST_Always);
    testdbPutFieldOk("wfSoft.APST", DBF_ENUM, waveformPOST_Always);
    monitor = testMonitorCreate("wfSoft", DBE_VALUE, 0);
    testMonitorSync(monitor);
    (void)testMonitorCount(monitor, 1);

    testDiag("MPST Always posts unchanged waveform data");
    processWaveform("wfSoft");
    checkMonitorCount(monitor, 1, "MPST Always");
    processWaveform("wfSoft");
    checkMonitorCount(monitor, 1, "MPST Always");

    testDiag("MPST On Change posts the first hash result");
    testdbPutFieldOk("wfSoft.MPST", DBF_ENUM, waveformPOST_OnChange);
    processWaveform("wfSoft");
    checkMonitorCount(monitor, 1, "MPST On Change first process");
    processWaveform("wfSoft");
    checkMonitorCount(monitor, 0, "MPST On Change unchanged data");

    testDiag("MPST On Change posts changed waveform data");
    putArray("wfSource", NELEMENTS(changed), changed);
    processWaveform("wfSoft");
    checkMonitorCount(monitor, 1, "MPST On Change changed data");

    testDiag("MPST On Change posts changed active length");
    putArray("wfSource", NELEMENTS(shorter), shorter);
    processWaveform("wfSoft");
    checkMonitorCount(monitor, 1, "MPST On Change changed NORD");

    testMonitorDestroy(monitor);
}

static void testArchiveMonitors(void)
{
    static const epicsInt32 input[] = {1, 2, 3};
    static const epicsInt32 changed[] = {1, 9, 3};
    testMonitor *monitor;

    putArray("wfSource", NELEMENTS(input), input);
    testdbPutFieldOk("wfSoft.MPST", DBF_ENUM, waveformPOST_Always);
    testdbPutFieldOk("wfSoft.APST", DBF_ENUM, waveformPOST_Always);
    monitor = testMonitorCreate("wfSoft", DBE_LOG, 0);
    testMonitorSync(monitor);
    (void)testMonitorCount(monitor, 1);

    testDiag("APST Always posts unchanged waveform data");
    processWaveform("wfSoft");
    checkMonitorCount(monitor, 1, "APST Always");
    processWaveform("wfSoft");
    checkMonitorCount(monitor, 1, "APST Always");

    testDiag("APST On Change posts the first hash result");
    testdbPutFieldOk("wfSoft.APST", DBF_ENUM, waveformPOST_OnChange);
    processWaveform("wfSoft");
    checkMonitorCount(monitor, 1, "APST On Change first process");
    processWaveform("wfSoft");
    checkMonitorCount(monitor, 0, "APST On Change unchanged data");

    testDiag("APST On Change posts changed waveform data");
    putArray("wfSource", NELEMENTS(changed), changed);
    processWaveform("wfSoft");
    checkMonitorCount(monitor, 1, "APST On Change changed data");

    testMonitorDestroy(monitor);
}

static void testUnits(const char *pv, const char *expected)
{
    DBADDR address;
    struct {
        DBRunits
        epicsInt32 value;
    } buffer;
    long options = DBR_UNITS;
    long count = 1;

    testOk(!dbNameToAddr(pv, &address), "find %s", pv);
    testOk(!dbGetField(&address, DBR_LONG, &buffer, &options, &count, NULL),
        "get units for %s", pv);
    testOk(!strcmp(buffer.units, expected), "%s units are %s", pv, expected);
}

static void testPrecision(const char *pv, long expected)
{
    DBADDR address;
    struct {
        DBRprecision
        epicsFloat64 value;
    } buffer;
    long options = DBR_PRECISION;
    long count = 1;

    testOk(!dbNameToAddr(pv, &address), "find %s", pv);
    testOk(!dbGetField(&address, DBR_DOUBLE, &buffer, &options, &count, NULL),
        "get precision for %s", pv);
    testOk(buffer.precision.dp == expected, "%s precision (%ld) is %ld",
        pv, buffer.precision.dp, expected);
}

static void testGraphicLimits(const char *pv, double low, double high)
{
    DBADDR address;
    struct {
        DBRgrDouble
        epicsFloat64 value;
    } buffer;
    long options = DBR_GR_DOUBLE;
    long count = 1;

    testOk(!dbNameToAddr(pv, &address), "find %s", pv);
    testOk(!dbGetField(&address, DBR_DOUBLE, &buffer, &options, &count, NULL),
        "get graphic limits for %s", pv);
    testOk(buffer.lower_disp_limit == low, "%s lower graphic limit is %g",
        pv, low);
    testOk(buffer.upper_disp_limit == high, "%s upper graphic limit is %g",
        pv, high);
}

static void testControlLimits(const char *pv, double low, double high)
{
    DBADDR address;
    struct {
        DBRctrlDouble
        epicsFloat64 value;
    } buffer;
    long options = DBR_CTRL_DOUBLE;
    long count = 1;

    testOk(!dbNameToAddr(pv, &address), "find %s", pv);
    testOk(!dbGetField(&address, DBR_DOUBLE, &buffer, &options, &count, NULL),
        "get control limits for %s", pv);
    testOk(buffer.lower_ctrl_limit == low, "%s lower control limit is %g",
        pv, low);
    testOk(buffer.upper_ctrl_limit == high, "%s upper control limit is %g",
        pv, high);
}

static void testMetadata(void)
{
    testdbPutFieldOk("wfMeta.EGU", DBF_STRING, "mm");
    testdbPutFieldOk("wfMeta.PREC", DBF_SHORT, 4);
    testdbPutFieldOk("wfMeta.HOPR", DBF_DOUBLE, 12.5);
    testdbPutFieldOk("wfMeta.LOPR", DBF_DOUBLE, -2.5);

    testDiag("waveform VAL metadata uses display fields");
    testUnits("wfMeta", "mm");
    testPrecision("wfMeta", 4);
    testGraphicLimits("wfMeta", -2.5, 12.5);
    testControlLimits("wfMeta", -2.5, 12.5);

    testDiag("string waveform VAL has no engineering units");
    testdbPutFieldOk("wfString.EGU", DBF_STRING, "mm");
    testUnits("wfString", "");

    testDiag("NORD and BUSY advertise fixed ranges");
    testGraphicLimits("wfMeta.NORD", 0.0, 4.0);
    testControlLimits("wfMeta.NORD", 0.0, 4.0);
    testGraphicLimits("wfMeta.BUSY", 0.0, 1.0);
    testControlLimits("wfMeta.BUSY", 0.0, 1.0);
}

MAIN(waveformTest)
{
    testPlan(90);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("waveformTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testSoftInput();
    testArrayWrites();
    testValueMonitors();
    testArchiveMonitors();
    testMetadata();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
