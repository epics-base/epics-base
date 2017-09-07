/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as operator of Argonne
* National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>

#include "dbAccess.h"
#include "dbStaticLib.h"
#include "dbTest.h"
#include "dbUnitTest.h"
#include "errlog.h"
#include "registryFunction.h"
#include "subRecord.h"
#include "testMain.h"

static
void checkDtyp(const char *rec)
{
    char dtyp[16];

    strcpy(dtyp, rec);
    strcat(dtyp, ".DTYP");

    testdbGetFieldEqual(dtyp, DBF_LONG, 0); /* Soft Channel = 0 */
}

static
void checkInput(const char *rec, int value)
{
    char proc[16];

    testDiag("Checking record '%s'", rec);

    strcpy(proc, rec);
    strcat(proc, ".PROC");

    testdbPutFieldOk(proc, DBF_CHAR, 1);

    testdbGetFieldEqual(rec, DBF_LONG, value);
}

static
void testGroup0(void)
{
    const char ** rec;
    const char * records[] = {
        "ai0", "bi0", "di0", "ii0", "li0", "lsi0", "mi0", "si0", NULL
    };

    testDiag("============ Starting %s ============", EPICS_FUNCTION);

    testdbPutFieldOk("source", DBF_LONG, 1);
    for (rec = records; *rec; rec++) {
        checkInput(*rec, 1);
        checkDtyp(*rec);
    }

    testdbPutFieldOk("source", DBF_LONG, 0);
    for (rec = records; *rec; rec++) {
        checkInput(*rec, 0);
    }
}

static
void testGroup1(void)
{
    const char ** rec;
    const char * records[] = {
        "bi1",
        "ai1", "di1", "ii1", "li1", "lsi1", "mi1", "si1", NULL
    };
    int init = 1;   /* bi1 initializes to 1 */

    testDiag("============ Starting %s ============", EPICS_FUNCTION);

    for (rec = records; *rec; rec++) {
        checkInput(*rec, init);
        init = 9;   /* remainder initialize to 9 */
    }
}

int dest;

static
long destSubr(subRecord *prec)
{
    dest = prec->val;
    return 0;
}

static
void checkOutput(const char *rec, int value)
{
    testDiag("Checking record '%s'", rec);

    testdbPutFieldOk(rec, DBF_LONG, value);

    testOk(dest == value, "value %d output -> %d", value, dest);
}

static
void testGroup2(void)
{
    const char ** rec;
    const char * records[] = {
        "ao0", "bo0", "io0", "lo0", "lso0", "mo0", "so0", NULL,
    };

    testDiag("============ Starting %s ============", EPICS_FUNCTION);

    for (rec = records; *rec; rec++) {
        checkOutput(*rec, 1);
        checkDtyp(*rec);
    }
    checkOutput("do0.B0", 1);
    checkDtyp("do0");

    for (rec = records; *rec; rec++) {
        checkOutput(*rec, 0);
    }
    checkOutput("do0.B0", 0);
}

static
void testGroup3(void)
{
    const char ** rec;
    const char * records[] = {
        "ao1", "bo1", "do1.B0", "io1", "lo1", "lso1", "mo1", "so1", NULL,
    };

    testDiag("============ Starting %s ============", EPICS_FUNCTION);

    for (rec = records; *rec; rec++) {
        checkOutput(*rec, 0);
    }
}

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(softTest)
{
    testPlan(114);

    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);
    registryFunctionAdd("destSubr", (REGISTRYFUNCTION) destSubr);

    testdbReadDatabase("softTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testGroup0();
    testGroup1();
    testGroup2();
    testGroup3();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
