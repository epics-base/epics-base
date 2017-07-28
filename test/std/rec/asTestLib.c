/*************************************************************************\
* Copyright (c) 2015 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 *
 * Test the hooks that autosave uses during initialization
 */

#include "string.h"

#include "epicsString.h"
#include "dbUnitTest.h"
#include "epicsThread.h"
#include "iocInit.h"
#include "dbBase.h"
#include "link.h"
#include "recSup.h"
#include "iocsh.h"
#include "dbAccess.h"
#include "dbConvert.h"
#include "dbStaticLib.h"
#include "registry.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "osiFileName.h"
#include "initHooks.h"
#include "devSup.h"
#include "errlog.h"

#include "aoRecord.h"
#include "waveformRecord.h"

#include "epicsExport.h"

static unsigned iran;

static
int checkGetString(DBENTRY *pent, const char *expect)
{
    dbCommon *prec = pent->precnode->precord;
    const char *actual = dbGetString(pent);
    int ret = strcmp(actual, expect);
    testOk(ret==0, "dbGetString(\"%s.%s\") -> '%s' == '%s'", prec->name,
           pent->pflddes->name, actual, expect);
    return ret;
}

static void hookPass0(initHookState state)
{
    DBENTRY entry;
    if(state!=initHookAfterInitDevSup)
        return;
    testDiag("initHookAfterInitDevSup");

    dbInitEntry(pdbbase, &entry);

    testDiag("restore integer pass0");
    /* rec0.VAL is initially 1, set it to 2 */
    if(dbFindRecord(&entry, "rec0.VAL")==0) {
        aoRecord *prec = entry.precnode->precord;
        testOk(prec->val==1, "VAL %d==1 (initial value from .db)", (int)prec->val);
        checkGetString(&entry, "1");
        testOk1(dbPutString(&entry, "2")==0);
        testOk(prec->val==2, "VAL %d==2", (int)prec->val);
        checkGetString(&entry, "2");
    } else {
        testFail("Missing rec0");
        testSkip(4, "missing record");
    }

    testDiag("restore string pass0");
    if(dbFindRecord(&entry, "rec0.DESC")==0) {
        aoRecord *prec = entry.precnode->precord;
        testOk1(strcmp(prec->desc, "foobar")==0);
        checkGetString(&entry, "foobar");
        testOk1(dbPutString(&entry, "hello")==0);
        testOk1(strcmp(prec->desc, "hello")==0);
        checkGetString(&entry, "hello");
    } else {
        testFail("Missing rec0");
        testSkip(4, "missing record");
    }

    if(dbFindRecord(&entry, "rec1.DESC")==0) {
        aoRecord *prec = entry.precnode->precord;
        testOk1(strcmp(prec->desc, "")==0);
        checkGetString(&entry, "");
        testOk1(dbPutString(&entry, "world")==0);
        testOk1(strcmp(prec->desc, "world")==0);
        checkGetString(&entry, "world");
    } else {
        testFail("Missing rec1");
        testSkip(4, "missing record");
    }

    testDiag("restore link pass0");
    /* rec0.OUT is initially "rec0.DISV", set it to "rec0.SEVR" */
    if(dbFindRecord(&entry, "rec0.OUT")==0) {
        aoRecord *prec = entry.precnode->precord;
        if(prec->out.type==CONSTANT)
            testOk(strcmp(prec->out.text,"rec0.DISV")==0,
                   "%s==rec0.DISV (initial value from .db)",
                   prec->out.text);
        else
            testFail("Wrong link type: %d", (int)prec->out.type);

        /* note that dbGetString() reads an empty string before links are initialized
         * should probably be considered a bug, but has been the case for so long
         * we call it a 'feature'.
         */
        checkGetString(&entry, "");

        testOk1(dbPutString(&entry, "rec0.SEVR")==0);
    } else{
        testFail("Missing rec0");
        testSkip(1, "missing record");
    }

    /* rec0.SDIS is initially NULL, set it to "rec0.STAT" */
    if(dbFindRecord(&entry, "rec0.SDIS")==0) {
        aoRecord *prec = entry.precnode->precord;
        if(prec->sdis.type==CONSTANT)
            testOk1(prec->sdis.value.constantStr==NULL);
        else
            testFail("Wrong link type: %d", (int)prec->sdis.type);

        testOk1(dbPutString(&entry, "rec0.STAT")==0);
    } else{
        testFail("Missing rec0");
        testSkip(1, "missing record");
    }

    /* can't restore array field in pass0 */

    dbFinishEntry(&entry);
}

static long initRec0(aoRecord *prec)
{
    DBLINK *plink = &prec->out;
    testDiag("init_record(%s)", prec->name);
    testOk(prec->val==2, "VAL %d==2 (pass0 value)", (int)prec->val);
    prec->val = 3;
    testOk(prec->val==3, "VAL %d==3", (int)prec->val);

    testOk1(plink->type==DB_LINK);
    if(plink->type==DB_LINK)
        testOk(strcmp(plink->value.pv_link.pvname,"rec0.SEVR")==0,
               "%s==rec0.SEVR (pass0 value)", plink->value.pv_link.pvname);
    else
        testFail("Wrong link type");

    plink = &prec->sdis;

    testOk1(plink->type==DB_LINK);
    if(plink->type==DB_LINK)
        testOk(strcmp(plink->value.pv_link.pvname,"rec0.STAT")==0,
               "%s==rec0.STAT (pass0 value)", plink->value.pv_link.pvname);
    else
        testFail("Wrong link type");

    iran |= 1;
    return 2; /* we set .VAL, so don't use RVAL */
}

static long initRec1(waveformRecord *prec)
{
    testDiag("init_record(%s)", prec->name);
    testOk(prec->nord==0, "NORD %d==0", (int)prec->nord);
    iran |= 2;
    return 0;
}

static double values[] = {1,2,3,4,5};

static void hookPass1(initHookState state)
{
    DBENTRY entry;
    DBADDR  addr;
    if(state!=initHookAfterInitDatabase)
        return;
    testDiag("initHookAfterInitDatabase");

    dbInitEntry(pdbbase, &entry);

    if(dbFindRecord(&entry, "rec0.VAL")==0) {
        aoRecord *prec = entry.precnode->precord;
        testOk(prec->val==3, "VAL %d==3 (init_record value)", (int)prec->val);
        testOk1(dbPutString(&entry, "4")==0);
        testOk(prec->val==4, "VAL %d==4", (int)prec->val);
    } else{
        testFail("Missing rec0");
        testSkip(1, "missing record");
    }

    /* Can't restore links in pass 1 */

    if(dbNameToAddr("rec1.VAL", &addr)) {
        testFail("missing rec1");
        testSkip(3, "missing record");
    } else {
        rset *prset = dbGetRset(&addr);
        dbfType ftype = addr.field_type;
        long count=-1, offset=-1, maxcount = addr.no_elements;
        testOk1(prset && prset->get_array_info && prset->put_array_info);
        testOk1((*prset->get_array_info)(&addr, &count, &offset)==0);
        /* count is ignored */
        testOk1((*dbPutConvertRoutine[DBF_DOUBLE][ftype])(&addr, values, NELEMENTS(values), maxcount,offset)==0);
        testOk1((*prset->put_array_info)(&addr, NELEMENTS(values))==0);
    }

    dbFinishEntry(&entry);
}

#if defined(__rtems__) || defined(vxWorks)
void asTestIoc_registerRecordDeviceDriver(struct dbBase *);
#endif

epicsShareFunc
void testRestore(void)
{
    aoRecord *rec0;
    waveformRecord *rec1;
    testDiag("test Restore");

    initHookRegister(hookPass0);
    initHookRegister(hookPass1);

    testdbPrepare();

    testdbReadDatabase("asTestIoc.dbd", NULL, NULL);

    /* since this test has device support it must appear in a
     * DLL for windows dynamic builds.
     * However, the rRDD function is in the executable,
     * and not accessible here.  So use iocsh.
     * For rtems/vxworks the test harness clears
     * iocsh registrations, so iocsh can't work here.
     */
#if defined(__rtems__) || defined(vxWorks)
    asTestIoc_registerRecordDeviceDriver(pdbbase);
#else
    iocshCmd("asTestIoc_registerRecordDeviceDriver(pdbbase)");
#endif

    testdbReadDatabase("asTest.db", NULL, NULL);

    rec0 = (aoRecord*)testdbRecordPtr("rec0");
    rec1 = (waveformRecord*)testdbRecordPtr("rec1");

    eltc(0);
    testIocInitOk();
    eltc(1);

    testDiag("Post initialization");

    testOk1(iran==3);

    testOk1(rec0->val==4);
    testOk1(rec1->nord==5);
    {
        double *buf = rec1->bptr;
        testOk(buf[0]==1, "buf[0]  %f==1", buf[0]);
        testOk1(buf[1]==2);
        testOk1(buf[2]==3);
        testOk1(buf[3]==4);
        testOk1(buf[4]==5);
    }

    testIocShutdownOk();

    /* recSup doesn't cleanup after itself */
    free(rec1->bptr);

    testdbCleanup();
}

struct dset6 {
    dset common;
    DEVSUPFUN proc;
    DEVSUPFUN linconv;
};

static long noop() {return 0;}

static struct dset6 devAOasTest = { {6, NULL, NULL, (DEVSUPFUN)initRec0, NULL}, (DEVSUPFUN)noop, NULL};
static struct dset6 devWFasTest = { {6, NULL, NULL, (DEVSUPFUN)initRec1, NULL}, (DEVSUPFUN)noop, NULL};

epicsExportAddress(dset, devAOasTest);
epicsExportAddress(dset, devWFasTest);
