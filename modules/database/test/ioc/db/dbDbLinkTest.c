/*************************************************************************\
 * Copyright (c) 2020 Michael Davidsaver
 * SPDX-License-Identifier: EPICS
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 \*************************************************************************/

#include <string.h>

#include <dbUnitTest.h>
#include <testMain.h>

#include <errlog.h>
#include <epicsTime.h>

#include <dbLock.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <alarm.h>

#include "xRecord.h"

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

static
void checkTime(void)
{
    epicsTimeStamp stamp;
    epicsUTag tag;

    dbCommon* target = testdbRecordPtr("target");
    dbCommon* src = testdbRecordPtr("src");

    testDiag("checkTime()");

    dbScanLock(target);
    target->time.secPastEpoch = 0x12345678;
    target->time.nsec = 0x9abcdef0;
    target->utag = 0xdeadbeef;
    dbScanUnlock(target);

    dbScanLock(src);
    testOk1(0==dbGetTimeStamp(dbGetDevLink(src), &stamp));
    dbScanUnlock(src);

    testOk1(stamp.secPastEpoch==0x12345678);
    testOk1(stamp.nsec==0x9abcdef0);
    stamp.secPastEpoch = 0;
    stamp.nsec = 0;

    dbScanLock(src);
    testOk1(0==dbGetTimeStampTag(dbGetDevLink(src), &stamp, &tag));
    dbScanUnlock(src);

    testOk1(stamp.secPastEpoch==0x12345678);
    testOk1(stamp.nsec==0x9abcdef0);
    testOk1(tag==0xdeadbeef);
}

static
void alarmProc(xRecord *prec)
{
    recGblSetSevrMsg(prec, READ_ALARM, MAJOR_ALARM, "a %s", "message");
    prec->val = 0;
}

static
void checkAlarm(void)
{
    epicsEnum16 stat, sevr;

    xRecord* target = (xRecord*)testdbRecordPtr("target");
    dbCommon* src = testdbRecordPtr("src");

    char amsg[sizeof(src->amsg)];

    testDiag("checkAlarm()");

    dbScanLock((dbCommon*)target);
    target->clbk = &alarmProc;
    dbProcess((dbCommon*)target);
    target->clbk = NULL;
    dbScanUnlock((dbCommon*)target);

    dbScanLock(src);
    testOk1(0==dbGetAlarm(dbGetDevLink(src), &stat, &sevr));
    dbScanUnlock(src);

    testOk1(stat==READ_ALARM);
    testOk1(sevr==MAJOR_ALARM);
    stat = sevr = 0;

    dbScanLock(src);
    testOk1(0==dbGetAlarmMsg(dbGetDevLink(src), &stat, &sevr, amsg, sizeof(amsg)));
    dbScanUnlock(src);

    testOk1(stat==READ_ALARM);
    testOk1(sevr==MAJOR_ALARM);
    testOk1(strcmp(amsg, "a message")==0);
    stat = sevr = 0;
    memset(amsg, 0, sizeof(amsg));

    dbScanLock(src);
    testOk1(0==dbGetAlarmMsg(dbGetDevLink(src), &stat, &sevr, amsg, 5));
    dbScanUnlock(src);

    testOk1(stat==READ_ALARM);
    testOk1(sevr==MAJOR_ALARM);
    testOk1(strcmp(amsg, "a me")==0);
}

MAIN(dbDbLinkTest)
{
    testPlan(18);

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("dbDbLinkTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    checkTime();
    checkAlarm();

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
