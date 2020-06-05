/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as operator of Argonne
* National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>

#include "dbAccess.h"
#include "dbStaticLib.h"
#include "dbTest.h"
#include "dbUnitTest.h"
#include "epicsThread.h"
#include "epicsEvent.h"
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

/* Group 0 are soft-channel input records with INP being a DB or CA link
 * to the PV 'source'. Their VAL fields all start out with the default
 * value for the type, i.e. 0 or an empty string. Triggering record
 * processing reads the value from the 'source' PV.
 */

static
void testGroup0(void)
{
    const char ** rec;
    const char * records[] = {
        "ai0", "bi0", "di0", "ii0", "li0", "lsi0", "mi0", "si0",
        "ai0c", "bi0c", "di0c", "ii0c", "li0c", "lsi0c", "mi0c", "si0c",
        NULL
    };

    testDiag("============ Starting %s ============", EPICS_FUNCTION);

    testdbPutFieldOk("source", DBR_LONG, 1);
    /* The above put sends CA monitors to all of the CA links, but
     * doesn't trigger record processing (the links are not CP/CPP).
     * How could we wait until all of those monitors have arrived,
     * instead of just waiting for an arbitrary time period?
     */
    epicsThreadSleep(1.0);  /* FIXME: Wait here? */
    for (rec = records; *rec; rec++) {
        if (strncmp(*rec, "lsi0", 4) != 0)
            testdbGetFieldEqual(*rec, DBR_LONG, 0);
        checkDtyp(*rec);
        doProcess(*rec);
        testdbGetFieldEqual(*rec, DBR_LONG, 1);
    }

    testdbPutFieldOk("source", DBR_LONG, 0);
    epicsThreadSleep(1.0);  /* FIXME: Wait here as above */
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
epicsEventId destEvent;

static
long destSubr(subRecord *prec)
{
    dest = prec->val;
    prec->val = -1;
    epicsEventMustTrigger(destEvent);
    return 0;
}

static
void checkOutput3(const char *rec, int value)
{
    testDiag("Checking record '%s'", rec);

    testdbPutFieldOk(rec, DBR_LONG, value);

    epicsEventMustWait(destEvent);
    testOk(dest == value, "value %d output -> %d", value, dest);
}

/* Group 3 are all soft-channel output records with OUT being a DB or
 * local CA link to the subRecord 'dest'; DB links have the PP flag,
 * for CA links the VAL field is marked PP. Putting a value to the
 * output record writes that value to 'dest'.
 */
static
void testGroup3(void)
{
    const char ** rec;
    const char * records[] = {
        "ao3", "bo3", "io3", "lo3", "lso3", "mo3", "so3",
        "ao3c", "bo3c", "io3c", "lo3c", "lso3c", "mo3c", "so3c",
        NULL,
    };

    destEvent = epicsEventMustCreate(epicsEventEmpty);

    testDiag("============ Starting %s ============", EPICS_FUNCTION);

    for (rec = records; *rec; rec++) {
        checkOutput3(*rec, 1);
        checkDtyp(*rec);
    }
    checkOutput3("do3.B0", 1);
    checkDtyp("do3");
    checkOutput3("do3c.B0", 1);
    checkDtyp("do3c");

    for (rec = records; *rec; rec++) {
        checkOutput3(*rec, 0);
    }
    checkOutput3("do3.B0", 0);
    checkOutput3("do3c.B0", 0);
}


static
void checkOutput4(const char *rec, int value)
{
    testDiag("Checking record '%s'", rec);

    testdbPutFieldOk(rec, DBR_LONG, value);
}

/* Group 4 are all soft-channel output records with OUT being empty
 * (i.e. a CONSTANT link). Putting a value to the record must succeed.
 */
static
void testGroup4(void)
{
    const char ** rec;
    const char * records[] = {
        "ao4", "bo4", "do4.B0", "io4", "lo4", "lso4", "mo4", "so4", NULL,
    };

    testDiag("============ Starting %s ============", EPICS_FUNCTION);

    for (rec = records; *rec; rec++) {
        checkOutput4(*rec, 0);
    }
}

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);


MAIN(softTest)
{
    testPlan(258);

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
