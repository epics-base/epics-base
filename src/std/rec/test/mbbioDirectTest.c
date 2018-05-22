/*************************************************************************\
* Copyright (c) 2017 Dirk Zimoch
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Theory of Operation
 *
 * For each of the two soft device supports (soft/raw soft/soft),
 * there is a combination of mbboDirect -> val / sim -> mbbiDirect.
 *
 * The intermediate records are of type double (val) and long (sim) to
 * check conversion from/to double.
 *
 * For each device support, the following is done:
 * 1. The mbboDirect record is set to a specific value through the database.
 * 2. The initial value is checked on both ends.
 * 3. Two single bits (5 & 31) are toggled, values checked on both ends.
 * 4. Sim mode is activated, one bit (0) is toggled, values are checked,
 *    data path (old value in val, new value in sim) is checked.
 */

#include <string.h>

#include "dbAccess.h"
#include "errlog.h"
#include "dbStaticLib.h"
#include "dbTest.h"
#include "dbUnitTest.h"
#include "testMain.h"
#include "epicsThread.h"
#include "epicsExport.h"

static
void testmbbioFields(const char* rec, unsigned int value)
{
    char field[40];
    unsigned int i;
    
    testdbGetFieldEqual(rec, DBF_ULONG, value);
    for (i=0; i < 32; i++)
    {
        sprintf(field,"%s.B%X", rec, i);
        testdbGetFieldEqual(field, DBF_ULONG, (value>>i)&1);
    }
}

static
void testmbbioRecords(unsigned int count, unsigned int value)
{
    char rec[40];
    unsigned int i;
    
    for (i = 1; i <= count; i++)
    {
        sprintf(rec, "do%d", i);
        testDiag("  ### %s ###", rec);
        testmbbioFields(rec, value);
        sprintf(rec, "di%d", i);
        testmbbioFields(rec, value);
    }
}

static
void putN(const char* pattern, unsigned int count, unsigned int value)
{
    char field[40];
    unsigned int i;
    
    for (i = 1; i <= count; i++)
    {
        sprintf(field, pattern, i);
        testdbPutFieldOk(field, DBF_ULONG, value);
    }
}

static
void testN(const char* pattern, unsigned int count, unsigned int value)
{
    char field[40];
    unsigned int i;
    
    for (i = 1; i <= count; i++)
    {
        sprintf(field, pattern, i);
        testdbGetFieldEqual(field, DBF_ULONG, value);
    }
}


void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(mbbioDirectTest)
{
    unsigned int value = 0xdeadbeef;
    unsigned int simvalue = 0;
    char macros [40];
    const unsigned int N = 2;
    
    testPlan(N*((32+1)*2*4+4+3));

    testdbPrepare();

    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);

    recTestIoc_registerRecordDeviceDriver(pdbbase);

    sprintf(macros, "INIT=%#x", value);
    testdbReadDatabase("mbbioDirectTest.db", NULL, macros);

    eltc(0);
    testIocInitOk();
    eltc(1);

    testDiag("##### check initial value #####");
    testmbbioRecords(N, value);

    testDiag("##### set bit 5 #####");
    putN("do%u.B5", N, 1);
    value |= (1<<5);
    testN("val%d", N,  value);
    testmbbioRecords(N, value);

    testDiag("##### clear bit 31 (0x1f) #####");
    putN("do%u.B1F", N, 0);
    value &= ~(1<<31);
    testN("val%d", N,  value);
    testmbbioRecords(N, value);

    testDiag("##### simulation mode #####");
    dbpf("sim", "1");
    simvalue = value & ~1;
    putN("do%u.B0", N, 0);
    /* old value in lo* */
    testN("val%d", N,  value);
    /* sim value in sim* */
    testN("sim%d", N,  simvalue);
    testmbbioRecords(N, simvalue);

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
