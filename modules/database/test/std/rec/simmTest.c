/*************************************************************************\
* Copyright (c) 2017 ITER Organization
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <limits.h>
#include <string.h>

#include <dbUnitTest.h>
#include <testMain.h>
#include <dbAccess.h>
#include <epicsTime.h>
#include <epicsEvent.h>
#include <epicsThread.h>
#include <errlog.h>
#include <alarm.h>
#include <callback.h>

#include "recSup.h"
#include "aiRecord.h"
#include "aoRecord.h"
#include "aaiRecord.h"
#include "aaoRecord.h"
#include "biRecord.h"
#include "boRecord.h"
#include "mbbiRecord.h"
#include "mbboRecord.h"
#include "mbbiDirectRecord.h"
#include "mbboDirectRecord.h"
#include "longinRecord.h"
#include "longoutRecord.h"
#include "int64inRecord.h"
#include "int64outRecord.h"
#include "stringinRecord.h"
#include "stringoutRecord.h"
#include "lsiRecord.h"
#include "lsoRecord.h"
#include "eventRecord.h"
#include "histogramRecord.h"
#include "waveformRecord.h"

/*
 * Tests for simulation mode
 */

void simmTest_registerRecordDeviceDriver(struct dbBase *);

static
void startSimmTestIoc(const char *dbfile)
{
    testdbPrepare();
    testdbReadDatabase("simmTest.dbd", NULL, NULL);
    simmTest_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase(dbfile, NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);
}

static char *rawSupp[] = {
    "ai",
    "bi",
    "mbbi",
    "mbbiDirect",
};

static
int hasRawSimmSupport(const char *rectype) {
    int i;
    for (i = 0; i < (sizeof(rawSupp)/sizeof(rawSupp[0])); i++)
        if (strcmp(rectype, rawSupp[i]) == 0) return 1;
    return 0;
}

#define PVNAMELENGTH 60
static char nameVAL[PVNAMELENGTH];
static char nameB0[PVNAMELENGTH];
static char nameRVAL[PVNAMELENGTH];
static char nameSGNL[PVNAMELENGTH];
static char nameSIMM[PVNAMELENGTH];
static char nameSIML[PVNAMELENGTH];
static char nameSVAL[PVNAMELENGTH];
static char nameSIOL[PVNAMELENGTH];
static char nameSCAN[PVNAMELENGTH];
static char namePROC[PVNAMELENGTH];
static char namePACT[PVNAMELENGTH];
static char nameSTAT[PVNAMELENGTH];
static char nameSEVR[PVNAMELENGTH];
static char nameSIMS[PVNAMELENGTH];
static char nameTSE[PVNAMELENGTH];
static char nameSimmode[PVNAMELENGTH];
static char nameSimval[PVNAMELENGTH];
static char nameSimvalNORD[PVNAMELENGTH];
static char nameSimvalLEN[PVNAMELENGTH];

#define SETNAME(field) strcpy(name ## field, name); strcat(name ## field, "." #field)
static
void setNames(const char *name)
{
    SETNAME(VAL); SETNAME(B0); SETNAME(RVAL); SETNAME(SGNL);
    SETNAME(SVAL); SETNAME(SIMM); SETNAME(SIML); SETNAME(SIOL); SETNAME(SIMS);
    SETNAME(SCAN); SETNAME(PROC); SETNAME(PACT);
    SETNAME(STAT); SETNAME(SEVR); SETNAME(TSE);
    strcpy(nameSimmode, name); strcat(nameSimmode, ":simmode");
    strcpy(nameSimval, name); strcat(nameSimval, ":simval");
    strcpy(nameSimvalNORD, name); strcat(nameSimvalNORD, ":simval.NORD");
    strcpy(nameSimvalLEN, name); strcat(nameSimvalLEN, ":simval.LEN");
}

/*
 * Parsing of info items and xsimm structure setting
 */
static
void testSimmSetup(void)
{
    aiRecord *precai;

    testDiag("##### Simm initialization #####");

    /* no config */
    precai = (aiRecord*)testdbRecordPtr("ai-0");
    testOk(precai->simpvt == NULL, "ai-0.SIMPVT = %p == NULL [no callback]", precai->simpvt);
    testOk(precai->sscn == USHRT_MAX, "ai-0.SSCN = %u == USHRT_MAX (not set)", precai->sscn);
    testOk(precai->sdly < 0., "ai-0.SDLY = %g < 0.0 (not set)", precai->sdly);

    /* with SCAN */
    precai = (aiRecord*)testdbRecordPtr("ai-1");
    testOk(precai->sscn == 5, "ai-1.SSCN = %u == 5 (2 second)", precai->sscn);
    testOk(precai->sdly < 0., "ai-1.SDLY = %g < 0.0 (not set)", precai->sdly);

    /* with DELAY */
    precai = (aiRecord*)testdbRecordPtr("ai-2");
    testOk(precai->sscn == USHRT_MAX, "ai-2.SSCN = %u == USHRT_MAX (not set)", precai->sscn);
    testOk(precai->sdly == 0.234, "ai-2.SDLY = %g == 0.234", precai->sdly);

    /* with SCAN and DELAY */
    precai = (aiRecord*)testdbRecordPtr("ai-3");
    testOk(precai->sscn == 4, "ai-3.SSCN = %u == 4 (5 second)", precai->sscn);
    testOk(precai->sdly == 0.345, "ai-3.SDLY = %g == 0.345", precai->sdly);
}

/*
 * Invalid SIML link sets LINK/NO_ALARM if in NO_ALARM
 */
static
void testSimlFail(void)
{
    aoRecord *precao;

    testDiag("##### Behavior for failing SIML #####");

    precao = (aoRecord*)testdbRecordPtr("ao-0");
    /* before anything: UDF INVALID */
    testOk(precao->stat == UDF_ALARM, "ao-0.STAT = %u [%s] == %u [UDF]",
           precao->stat, epicsAlarmConditionStrings[precao->stat], UDF_ALARM);
    testOk(precao->sevr == INVALID_ALARM, "ao-0.SEVR = %u [%s] == %u [INVALID]",
           precao->sevr, epicsAlarmSeverityStrings[precao->sevr], INVALID_ALARM);

    /* legal value: LINK NO_ALARM */
    testdbPutFieldOk("ao-0.VAL", DBR_DOUBLE, 1.0);
    testOk(precao->stat == LINK_ALARM, "ao-0.STAT = %u [%s] == %u [LINK]",
    precao->stat, epicsAlarmConditionStrings[precao->stat], LINK_ALARM);
    testOk(precao->sevr == NO_ALARM, "ao-0.SEVR = %u [%s] == %u [NO_ALARM]",
           precao->sevr, epicsAlarmSeverityStrings[precao->sevr], NO_ALARM);

    /* HIGH/MINOR overrides */
    testdbPutFieldOk("ao-0.VAL", DBR_DOUBLE, 2.0);
    testOk(precao->stat == HIGH_ALARM, "ao-0.STAT = %u [%s] == %u [HIGH]",
    precao->stat, epicsAlarmConditionStrings[precao->stat], HIGH_ALARM);
    testOk(precao->sevr == MINOR_ALARM, "ao-0.SEVR = %u [%s] == %u [MINOR]",
           precao->sevr, epicsAlarmSeverityStrings[precao->sevr], MINOR_ALARM);
}

/*
 * SIMM triggered SCAN swapping, by writing to SIMM and through SIML
 */

static
void testSimmToggle(const char *name, epicsEnum16 *psscn)
{
    testDiag("## SIMM toggle and SCAN swapping ##");

    /* SIMM mode by setting the field */

    testdbGetFieldEqual(nameSCAN, DBR_USHORT, 0);
    testOk(*psscn == 1, "SSCN = %u == 1 (Event)", *psscn);

    testDiag("set SIMM to YES");
    testdbPutFieldOk(nameSIMM, DBR_STRING, "YES");
    testdbGetFieldEqual(nameSCAN, DBR_USHORT, 1);
    testOk(*psscn == 0, "SSCN = %u == 0 (Passive)", *psscn);

    /* Change simm:SCAN when simmYES */
    testdbPutFieldOk(nameSCAN, DBR_USHORT, 3);

    testDiag("set SIMM to NO");
    testdbPutFieldOk(nameSIMM, DBR_STRING, "NO");
    testdbGetFieldEqual(nameSCAN, DBR_USHORT, 0);
    testOk(*psscn == 3, "SSCN = %u == 3 (10 second)", *psscn);
    *psscn = 1;

    if (hasRawSimmSupport(name)) {
        testDiag("set SIMM to RAW");
        testdbPutFieldOk(nameSIMM, DBR_STRING, "RAW");
        testdbGetFieldEqual(nameSCAN, DBR_USHORT, 1);
        testOk(*psscn == 0, "SSCN = %u == 0 (Passive)", *psscn);

        testDiag("set SIMM to NO");
        testdbPutFieldOk(nameSIMM, DBR_STRING, "NO");
        testdbGetFieldEqual(nameSCAN, DBR_USHORT, 0);
        testOk(*psscn == 1, "SSCN = %u == 1 (Event)", *psscn);
    } else {
        testDiag("Record type %s has no support for simmRAW", name);
    }

    /* SIMM mode through SIML */

    testdbPutFieldOk(nameSIML, DBR_STRING, nameSimmode);

    testDiag("set SIMM (via SIML -> simmode) to YES");
    testdbPutFieldOk(nameSimmode, DBR_USHORT, 1);
    testdbPutFieldOk(namePROC, DBR_LONG, 0);

    testdbGetFieldEqual(nameSIMM, DBR_USHORT, 1);
    testdbGetFieldEqual(nameSCAN, DBR_USHORT, 1);
    testOk(*psscn == 0, "SSCN = %u == 0 (Passive)", *psscn);

    testDiag("set SIMM (via SIML -> simmode) to NO");
    testdbPutFieldOk(nameSimmode, DBR_USHORT, 0);
    testdbPutFieldOk(namePROC, DBR_LONG, 0);

    testdbGetFieldEqual(nameSIMM, DBR_USHORT, 0);
    testdbGetFieldEqual(nameSCAN, DBR_USHORT, 0);
    testOk(*psscn == 1, "SSCN = %u == 1 (Event)", *psscn);

    if (hasRawSimmSupport(name)) {
        testDiag("set SIMM (via SIML -> simmode) to RAW");
        testdbPutFieldOk(nameSimmode, DBR_USHORT, 2);
        testdbPutFieldOk(namePROC, DBR_LONG, 0);

        testdbGetFieldEqual(nameSIMM, DBR_USHORT, 2);
        testdbGetFieldEqual(nameSCAN, DBR_USHORT, 1);
        testOk(*psscn == 0, "SSCN = %u == 0 (Passive)", *psscn);

        testDiag("set SIMM (via SIML -> simmode) to NO");
        testdbPutFieldOk(nameSimmode, DBR_USHORT, 0);
        testdbPutFieldOk(namePROC, DBR_LONG, 0);

        testdbGetFieldEqual(nameSIMM, DBR_USHORT, 0);
        testdbGetFieldEqual(nameSCAN, DBR_USHORT, 0);
        testOk(*psscn == 1, "SSCN = %u == 1 (Event)", *psscn);
    } else {
        testDiag("Record type %s has no support for simmRAW", name);
    }
}

/*
 * Reading from SVAL (direct write or through SIOL link)
 */

static
void testSvalRead(const char *name,
                  const epicsTimeStamp *mytime,
                  const epicsTimeStamp *svtime)
{
    epicsTimeStamp last;
    double diff;

    if (strcmp(name, "histogram") == 0)
        strcpy(nameVAL, nameSGNL);

    if (strcmp(name, "aai") != 0 &&
            strcmp(name, "waveform") != 0 &&
            strcmp(name, "lsi") != 0) {

        testDiag("## Reading from SVAL ##");

        testDiag("in simmNO, SVAL must be ignored");
        testdbPutFieldOk(nameSimmode, DBR_USHORT, 0);
        testdbPutFieldOk(nameVAL, DBR_LONG, 0);
        if (strcmp(name, "stringin") == 0)
            testdbPutFieldOk(nameSVAL, DBR_STRING, "1");
        else
            testdbPutFieldOk(nameSVAL, DBR_USHORT, 1);
        testdbPutFieldOk(namePROC, DBR_LONG, 0);
        testdbGetFieldEqual(nameVAL, DBR_USHORT, 0);

        testDiag("in simmYES, SVAL is used for VAL");
        testdbPutFieldOk(nameSIMS, DBR_USHORT, 0);
        testdbPutFieldOk(nameSimmode, DBR_USHORT, 1);
        testdbPutFieldOk(namePROC, DBR_LONG, 0);
        testdbGetFieldEqual(nameVAL, DBR_USHORT, 1);
        testDiag("No SIMS setting: STAT/SEVR == NO_ALARM");
        testdbGetFieldEqual(nameSTAT, DBR_STRING, "NO_ALARM");
        testdbGetFieldEqual(nameSEVR, DBR_USHORT, 0);

        if (hasRawSimmSupport(name)) {
            testDiag("in simmRAW, SVAL is used for RVAL");
            testdbPutFieldOk(nameSimmode, DBR_USHORT, 2);
            testdbPutFieldOk(namePROC, DBR_LONG, 0);
            testdbGetFieldEqual(nameRVAL, DBR_USHORT, 1);
        } else {
            testDiag("Record type %s has no support for simmRAW", name);
        }
    }

    testDiag("## Reading from SIOL->SVAL ##");

    /* Set SIOL link to simval */
    testdbPutFieldOk(nameSIOL, DBR_STRING, nameSimval);

    testDiag("in simmNO, SIOL->SVAL must be ignored");
    testdbPutFieldOk(nameSimmode, DBR_USHORT, 0);
    testdbPutFieldOk(nameVAL, DBR_LONG, 0);
    testdbPutFieldOk(nameSimval, DBR_LONG, 1);
    testdbPutFieldOk(namePROC, DBR_LONG, 0);
    testdbGetFieldEqual(nameVAL, DBR_USHORT, 0);

    testDiag("in simmYES, SIOL->SVAL is used for VAL");
    testdbPutFieldOk(nameSIMS, DBR_USHORT, 3);
    testdbPutFieldOk(nameSimmode, DBR_USHORT, 1);
    testdbPutFieldOk(namePROC, DBR_LONG, 0);
    testdbGetFieldEqual(nameVAL, DBR_USHORT, 1);
    testDiag("SIMS is INVALID: STAT/SEVR == SIMM/INVALID");
    testdbGetFieldEqual(nameSTAT, DBR_STRING, "SIMM");
    testdbGetFieldEqual(nameSEVR, DBR_USHORT, 3);
    testdbPutFieldOk(nameSIMS, DBR_USHORT, 0);

    if (hasRawSimmSupport(name)) {
        testDiag("in simmRAW, SIOL->SVAL is used for RVAL");
        testdbPutFieldOk(nameSimmode, DBR_USHORT, 2);
        testdbPutFieldOk(namePROC, DBR_LONG, 0);
        testdbGetFieldEqual(nameRVAL, DBR_USHORT, 1);
    } else {
        testDiag("Record type %s has no support for simmRAW", name);
    }

    /* My timestamp must be later than simval's */
    diff = epicsTimeDiffInSeconds(mytime, svtime);
    testOk(diff >= 0.0, "simval time <= my time [TSE = 0] (%.9f sec)", diff);

    testDiag("for TSE=-2 (from device) and simmYES, take time stamp from IOC or input link");

    /* Set simmYES */
    testdbPutFieldOk(nameSimmode, DBR_USHORT, 1);

    /* Set TSE to -2 (from device) and reprocess: timestamps is taken through SIOL from simval */
    testdbPutFieldOk(nameTSE, DBR_SHORT, -2);
    testdbPutFieldOk(namePROC, DBR_LONG, 0);
    testOk(epicsTimeEqual(svtime, mytime), "simval time == my time [TSE = -2]");
    last = *mytime;

    /* With TSE=-2 and no SIOL, timestamp is taken from IOC */
    testdbPutFieldOk(nameSIOL, DBR_STRING, "");
    testdbPutFieldOk(namePROC, DBR_LONG, 0);
    diff = epicsTimeDiffInSeconds(mytime, &last);
    testOk(diff >= 0.0, "new time stamp from IOC [TSE = -2, no SIOL] (%.9f sec)", diff);

    /* Reset TSE */
    testdbPutFieldOk(nameTSE, DBR_SHORT, 0);
}

/*
 * Writing through SIOL link
 */

static
void testSiolWrite(const char *name,
                   const epicsTimeStamp *mytime)
{
    epicsTimeStamp now;
    double diff;

    testDiag("## Writing through SIOL ##");

    /* Set SIOL link to simval */
    testdbPutFieldOk(nameSIOL, DBR_STRING, nameSimval);

    testDiag("in simmNO, SIOL must be ignored");
    testdbPutFieldOk(nameSimmode, DBR_USHORT, 0);
    if (strcmp(name, "mbboDirect") == 0)
        testdbPutFieldOk(nameB0, DBR_LONG, 1);
    else
        testdbPutFieldOk(nameVAL, DBR_LONG, 1);

    if (strcmp(name, "aao") == 0)
        testdbGetFieldEqual(nameSimvalNORD, DBR_USHORT, 0);
    else if (strcmp(name, "lso") == 0)
        testdbGetFieldEqual(nameSimvalLEN, DBR_USHORT, 0);
    else
        testdbGetFieldEqual(nameSimval, DBR_USHORT, 0);

    testDiag("in simmYES, SIOL is used to write VAL");
    testdbPutFieldOk(nameSimmode, DBR_USHORT, 1);
    if (strcmp(name, "mbboDirect") == 0)
        testdbPutFieldOk(nameB0, DBR_LONG, 1);
    else
        testdbPutFieldOk(nameVAL, DBR_LONG, 1);
    testdbGetFieldEqual(nameSimval, DBR_USHORT, 1);

    /* Set TSE to -2 (from device) and reprocess: timestamp is taken from IOC */
    epicsTimeGetCurrent(&now);
    testdbPutFieldOk(nameTSE, DBR_SHORT, -2);
    testdbPutFieldOk(namePROC, DBR_LONG, 0);
    diff = epicsTimeDiffInSeconds(mytime, &now);
    testOk(diff >= 0.0, "new time stamp from IOC [TSE = -2] (%.9f sec)", diff);

    /* Reset TSE */
    testdbPutFieldOk(nameTSE, DBR_SHORT, 0);
}

/*
 * Asynchronous processing using simm:DELAY
 */

static void
ping(CALLBACK *pcb)
{
    epicsEventId ev;
    callbackGetUser(ev, pcb);

    epicsEventMustTrigger(ev);
}

static
void testSimmDelay(const char *name,
                   epicsFloat64 *psdly,
                   const epicsTimeStamp *mytime)
{
    epicsTimeStamp now;
    const double delay = 0.01;  /* 10 ms */
    double diff;
    epicsEventId poked;
    CALLBACK cb;

    memset(&cb, 0, sizeof(CALLBACK));
    poked = epicsEventMustCreate(epicsEventEmpty);
    callbackSetCallback(ping, &cb);
    callbackSetPriority(priorityLow, &cb);
    callbackSetUser(poked, &cb);

    testDiag("## Asynchronous processing with simm:DELAY ##");

    /* Set delay to something just long enough */
    *psdly = delay;

    /* Process in simmNO: synchronous */
    testDiag("simm:DELAY and simmNO processes synchronously");
    testdbPutFieldOk(nameSimmode, DBR_USHORT, 0);
    epicsTimeGetCurrent(&now);
    testdbPutFieldOk(namePROC, DBR_LONG, 0);
    testdbGetFieldEqual(namePACT, DBR_USHORT, 0);
    diff = epicsTimeDiffInSeconds(mytime, &now);
    testOk(diff >= 0.0, "time stamp is recent (%.9f sec)", diff);

    /* Process in simmYES: asynchronous */
    testDiag("simm:DELAY and simmYES processes asynchronously");
    testdbPutFieldOk(nameSimmode, DBR_USHORT, 1);
    testdbPutFieldOk(namePROC, DBR_LONG, 0);
    testdbGetFieldEqual(namePACT, DBR_USHORT, 1);
    epicsTimeGetCurrent(&now);
    callbackRequestDelayed(&cb, 1.5 * delay);
    epicsEventWait(poked);
    testdbGetFieldEqual(namePACT, DBR_USHORT, 0);
    diff = epicsTimeDiffInSeconds(mytime, &now);
    testOk(diff >= 0.0, "time stamp is recent (%.9f sec)", diff);

    /* Reset delay */
    *psdly = -1.;
}

#define RUNALLTESTSREAD(type) \
    testDiag("################################################### Record Type " #type); \
    setNames(#type); \
    testSimmToggle(#type, &((type ## Record*)testdbRecordPtr(#type))->sscn); \
    testSvalRead(#type, &((type ## Record*)testdbRecordPtr(#type))->time, \
        &((type ## Record*)testdbRecordPtr(#type ":simval"))->time); \
    testSimmDelay(#type, &((type ## Record*)testdbRecordPtr(#type))->sdly, \
        &((type ## Record*)testdbRecordPtr(#type))->time)

#define RUNALLTESTSWRITE(type) \
    testDiag("################################################### Record Type " #type); \
    setNames(#type); \
    testSimmToggle(#type, &((type ## Record*)testdbRecordPtr(#type))->sscn); \
    testSiolWrite(#type, &((type ## Record*)testdbRecordPtr(#type))->time); \
    testSimmDelay(#type, &((type ## Record*)testdbRecordPtr(#type))->sdly, \
        &((type ## Record*)testdbRecordPtr(#type))->time)

static
void testAllRecTypes(void)
{
    RUNALLTESTSREAD(ai);
    RUNALLTESTSWRITE(ao);
    RUNALLTESTSREAD(aai);
    RUNALLTESTSWRITE(aao);
    RUNALLTESTSREAD(bi);
    RUNALLTESTSWRITE(bo);
    RUNALLTESTSREAD(mbbi);
    RUNALLTESTSWRITE(mbbo);
    RUNALLTESTSREAD(mbbiDirect);
    RUNALLTESTSWRITE(mbboDirect);
    RUNALLTESTSREAD(longin);
    RUNALLTESTSWRITE(longout);
    RUNALLTESTSREAD(int64in);
    RUNALLTESTSWRITE(int64out);
    RUNALLTESTSREAD(stringin);
    RUNALLTESTSWRITE(stringout);
    RUNALLTESTSREAD(lsi);
    RUNALLTESTSWRITE(lso);
    RUNALLTESTSREAD(event);
    RUNALLTESTSREAD(waveform);
    RUNALLTESTSREAD(histogram);
}


MAIN(simmTest)
{
    testPlan(1176);
    startSimmTestIoc("simmTest.db");

    testSimmSetup();
    testSimlFail();
    testAllRecTypes();

    testIocShutdownOk();
    testdbCleanup();
    return testDone();
}
