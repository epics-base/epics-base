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

    testdbGetFieldEqual(dtyp, DBR_LONG, 0);
    testdbGetFieldEqual(dtyp, DBR_STRING, "Soft Channel");
}

static
void doProcess(const char *rec)
{
    char proc[16];

    strcpy(proc, rec);
    strcat(proc, ".PROC");

    testdbPutFieldOk(proc, DBR_CHAR, 1);
}

/* Group 0 are all soft-channel input records with INP being a DB link
 * to the PV 'source'. Their VAL fields all start out with the default
 * value for the type, i.e. 0 or an empty string. Triggering record
 * processing should read the integer value from the 'source' PV.
 */

static
void testGroup0(void)
{
    const char ** rec;
    const char * records[] = {
        "ai0", "bi0", "di0", "ii0", "li0", "lsi0", "mi0", "si0", NULL
    };

    testDiag("============ Starting %s ============", EPICS_FUNCTION);

    testdbPutFieldOk("source", DBR_LONG, 1);
    for (rec = records; *rec; rec++) {
        if (strcmp(*rec, "lsi0") != 0)
            testdbGetFieldEqual(*rec, DBR_LONG, 0);
        checkDtyp(*rec);
        doProcess(*rec);
        testdbGetFieldEqual(*rec, DBR_LONG, 1);
    }

    testdbPutFieldOk("source", DBR_LONG, 0);
    for (rec = records; *rec; rec++) {
        doProcess(*rec);
        testdbGetFieldEqual(*rec, DBR_LONG, 0);
    }
}

/* Group 1 are all soft-channel input records with INP being a non-zero
 * "const" JSON-link, 9 for most records, 1 for the binary. Their VAL
 * fields should all be initialized to that constant value. Triggering
 * record processing should succeed, but shouldn't change VAL.
 */
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
        testdbGetFieldEqual(*rec, DBR_LONG, init);
        doProcess(*rec);
        testdbGetFieldEqual(*rec, DBR_LONG, init);
        init = 9;   /* other records initialize to 9 */
    }
}

/* Group 2 are all soft-channel input records with INP being a CONSTANT
 * link with value 9 for most records, 1 for the binary. Their VAL
 * fields should all be initialized to that constant value. Triggering
 * record processing should succeed, but shouldn't change VAL.
 */
static
void testGroup2(void)
{
    const char ** rec;
    const char * records[] = {
        "bi2",
        "ai2", "di2", "ii2", "li2", "mi2", NULL
    };
    int init = 1;   /* bi1 initializes to 1 */

    testDiag("============ Starting %s ============", EPICS_FUNCTION);

    for (rec = records; *rec; rec++) {
        testdbGetFieldEqual(*rec, DBR_LONG, init);
        doProcess(*rec);
        testdbGetFieldEqual(*rec, DBR_LONG, init);
        init = 9;   /* other records initialize to 9 */
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

    testdbPutFieldOk(rec, DBR_LONG, value);

    testOk(dest == value, "value %d output -> %d", value, dest);
}

/* Group 3 are all soft-channel output records with OUT being a DB link
 * to the PV 'dest' with PP. Putting a value to the record writes that
 * value to 'dest' and processes it.
 */
static
void testGroup3(void)
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

/* Group 4 are all soft-channel output records with OUT being empty
 * (i.e. a CONSTANT link). Putting a value to the record must succeed.
 */
static
void testGroup4(void)
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
    testPlan(163);

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
    testGroup4();

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
