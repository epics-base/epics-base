/*************************************************************************\
* Copyright (c) 2020 Dirk Zimoch
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>

#include "dbAccess.h"
#include "devSup.h"
#include "alarm.h"
#include "dbUnitTest.h"
#include "errlog.h"
#include "epicsThread.h"

#include "longinRecord.h"

#include "testMain.h"

void recTestIoc_registerRecordDeviceDriver(struct dbBase *);

static void startTestIoc(const char *dbfile)
{
    testdbPrepare();
    testdbReadDatabase("recTestIoc.dbd", NULL, NULL);
    recTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase(dbfile, NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);
}

struct pv {
    const char *name;
    long count;
    double values[4];
};

static void expectProcSuccess(struct pv *pv)
{
    char fieldname[20];
    testdbPutFieldOk("reset.PROC", DBF_LONG, 1);
    testDiag("expecting success from %s", pv->name);
    sprintf(fieldname, "%s.PROC", pv->name);
    testdbPutFieldOk(fieldname, DBF_LONG, 1);
    sprintf(fieldname, "%s.SEVR", pv->name);
    testdbGetFieldEqual(fieldname, DBF_LONG, NO_ALARM);
    sprintf(fieldname, "%s.STAT", pv->name);
    testdbGetFieldEqual(fieldname, DBF_LONG, NO_ALARM);
}

#if 0
static void expectProcFailure(struct pv *pv)
{
    char fieldname[20];
    testdbPutFieldOk("reset.PROC", DBF_LONG, 1);
    testDiag("expecting failure S_db_badField %#x from %s", S_db_badField, pv->name);
    sprintf(fieldname, "%s.PROC", pv->name);
    testdbPutFieldFail(S_db_badField, fieldname, DBF_LONG, 1);
    sprintf(fieldname, "%s.SEVR", pv->name);
    testdbGetFieldEqual(fieldname, DBF_LONG, INVALID_ALARM);
    sprintf(fieldname, "%s.STAT", pv->name);
    testdbGetFieldEqual(fieldname, DBF_LONG, LINK_ALARM);
}
#endif

static double initial[] = {0,1,2,3,4,5,6,7,8,9};
static double buf[10];

static void changeRange(struct pv *pv, long start, long incr, long end)
{
    char linkstring[60];
    char pvstring[10];

    if (incr)
        sprintf(linkstring, "tgt.[%ld:%ld:%ld]", start, incr, end);
    else if (end)
        sprintf(linkstring, "tgt.[%ld:%ld]", start, end);
    else
        sprintf(linkstring, "tgt.[%ld]", start);
    testDiag("modifying %s.OUT link: %s", pv->name, linkstring);
    sprintf(pvstring, "%s.OUT", pv->name);
    testdbPutFieldOk(pvstring, DBF_STRING, linkstring);
}

static void expectRange(double *values, long start, long incr, long end)
{
    int i,j;
    if (!incr)
        incr = 1;
    for (i=0; i<10; i++)
        buf[i] = initial[i];
    for (i=0, j=start; j<=end; i++, j+=incr)
        buf[j] = values[i];
    testdbGetFieldEqual("tgt.NORD", DBF_LONG, 8);
    testdbGetFieldEqual("tgt.SEVR", DBF_LONG, NO_ALARM);
    testdbGetFieldEqual("tgt.STAT", DBF_LONG, NO_ALARM);
    testdbGetArrFieldEqual("tgt.VAL", DBF_DOUBLE, 10, 8, buf);
}

#if 0
static void expectEmptyArray(void)
{
    /* empty arrays are now allowed at the moment */
    testDiag("expecting empty array");
    testdbGetFieldEqual("tgt.NORD", DBF_LONG, 0);
}
#endif

struct pv ao_pv = {"ao",1,{20,0,0,0}};
struct pv src_pv = {"src",4,{30,40,50,60}};
struct pv *ao = &ao_pv;
struct pv *src = &src_pv;

static double pini_values[] = {20,30,40,50};

#define expectEmptyRange() expectRange(0,0,0,-1)

MAIN(addrModifierTest)
{
    testPlan(205);
    startTestIoc("addrModifierTest.db");

    testdbGetFieldEqual("src.NORD", DBF_LONG, 4);
    testDiag("PINI");
    expectRange(pini_values,2,1,5);

    testDiag("after processing");
    testdbPutFieldOk("ao.PROC", DBF_LONG, 1);
    testdbPutFieldOk("src.PROC", DBF_LONG, 1);
    expectRange(pini_values,2,1,5);

    testDiag("after processing target record");
    testdbPutFieldOk("tgt.PROC", DBF_LONG, 1);
    expectRange(pini_values,2,1,5);

    testDiag("modify range");

    changeRange(ao,3,0,0);
    expectProcSuccess(ao);
    expectRange(ao->values,3,1,3);

    changeRange(src,4,0,7);
    expectProcSuccess(src);
    expectRange(src->values,4,1,7);

    testDiag("put more than available");

    changeRange(ao,3,0,6);
    expectProcSuccess(ao);
    expectRange(ao->values,3,1,3); /* clipped range */

    changeRange(src,3,0,9);
    expectProcSuccess(src);
    expectRange(src->values,3,1,6); /* clipped range */

    testDiag("backward range");

    changeRange(ao,5,0,3);
    expectProcSuccess(ao);
    expectEmptyRange();

    changeRange(src,5,0,3);
    expectProcSuccess(src);
    expectEmptyRange();

    testDiag("step 2");

    changeRange(ao,1,2,6);
    expectProcSuccess(ao);
    expectRange(ao->values,1,1,1); /* clipped range */

    changeRange(src,1,2,6);
    expectProcSuccess(src);
    expectRange(src->values,1,2,5);

    testDiag("range start beyond tgt.NORD");

    changeRange(ao,8,0,0);
    expectProcSuccess(ao);
    expectEmptyRange();

    changeRange(src,8,0,9);
    expectProcSuccess(src);
    expectEmptyRange();

    testDiag("range end beyond tgt.NORD");

    changeRange(ao,3,0,9);
    expectProcSuccess(ao);
    expectRange(ao->values,3,1,3); /* clipped range */

    changeRange(src,3,0,9);
    expectProcSuccess(src);
    expectRange(src->values,3,1,6); /* clipped range */

    testDiag("range start beyond tgt.NELM");

    changeRange(ao,11,0,12);
    expectProcSuccess(ao);
    expectEmptyRange();

    changeRange(src,11,0,12);
    expectProcSuccess(src);
    expectEmptyRange();

    testDiag("range end beyond tgt.NELM");

    changeRange(src,4,0,12);
    expectProcSuccess(src);
    expectRange(src->values,4,1,7); /* clipped range */

    testDiag("single value beyond tgt.NORD");

    changeRange(ao,8,0,0);
    expectProcSuccess(ao);
    expectEmptyRange();

    changeRange(src,8,0,0);
    expectProcSuccess(src);
    expectEmptyRange();

    testDiag("single value");

    changeRange(ao,5,0,0);
    expectProcSuccess(ao);
    expectRange(ao->values,5,1,5);

    changeRange(src,5,0,0);
    expectProcSuccess(src);
    expectRange(src->values,5,1,5);

    testDiag("single values beyond tgt.NELM");

    changeRange(ao,12,0,0);
    expectProcSuccess(ao);
    expectEmptyRange();

    changeRange(src,12,0,0);
    expectProcSuccess(src);
    expectEmptyRange();

    testIocShutdownOk();
    testdbCleanup();
    return testDone();
}
