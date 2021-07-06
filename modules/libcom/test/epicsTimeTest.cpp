/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Authors: Jeff Hill, Marty Kraimer and Andrew Johnson
 */
#include <iostream>

#include <cstdlib>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <climits>
#include <cstring>

#include "envDefs.h"
#include "epicsTime.h"
#include "epicsThread.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

using namespace std;

/* The functionality of the old invalidFormatTest () and badNanosecTest ()
 * routines is incorporated into epicsTimeTest () below.
 */

static const unsigned mSecPerSec = 1000u;
static const unsigned uSecPerSec = 1000u * mSecPerSec;
static const unsigned nSecPerSec = 1000u * uSecPerSec;
static const double precisionEPICS = 1.0 / nSecPerSec;

static void testAdd(epicsUInt32 lhsSec, epicsUInt32 lhsNS,
                    double rhs,
                    epicsUInt32 expectSec, epicsUInt32 expectNS)
{
    epicsTimeStamp lhs = {lhsSec, lhsNS};
    epicsTimeStamp expect = {expectSec, expectNS};
    epicsTimeStamp actual = lhs;


    epicsTimeAddSeconds(&actual, rhs);
    testOk(epicsTimeEqual(&actual, &expect),
           "testAdd(%u:%u + %.9f -> %u:%u == %u:%u)",
           unsigned(lhs.secPastEpoch), unsigned(lhs.nsec),
           rhs,
           unsigned(actual.secPastEpoch), unsigned(actual.nsec),
           unsigned(expect.secPastEpoch), unsigned(expect.nsec));
}

static void testDiff(epicsUInt32 lhsSec, epicsUInt32 lhsNS,
                     epicsUInt32 rhsSec, epicsUInt32 rhsNS,
                     double expect)
{
    epicsTimeStamp lhs = {lhsSec, lhsNS};
    epicsTimeStamp rhs = {rhsSec, rhsNS};
    double actual = epicsTimeDiffInSeconds(&lhs, &rhs);
    double diff = actual - expect;

    testOk(fabs(diff)<precisionEPICS,
           "testDiff(%u:%u - %u:%u) -> %.9f ~= %.9f (%g)",
           unsigned(lhs.secPastEpoch), unsigned(lhs.nsec),
           unsigned(rhs.secPastEpoch), unsigned(rhs.nsec),
           actual, expect, diff);
}

static void crossCheck(double delay)
{
    double mindelta = 2*epicsMonotonicResolution()*1e-9,
            tres = epicsThreadSleepQuantum();
    epicsUInt64 A = epicsMonotonicGet();
    epicsThreadSleep(delay);
    epicsUInt64 B = epicsMonotonicGet();

    double actual = (B-A)*1e-9, percent;

    if(mindelta<tres*2)
        mindelta = tres*2;
    if(delay<mindelta)
        delay = mindelta;

    percent = (delay-actual)/delay*100.0;

    testOk(fabs(percent)<1000.0, "crossCheck(%f) actual %f (%f %%)",
           delay, actual, percent);
}

static void testMonotonic()
{
    crossCheck(2.1); /* greater than 2 so that seconds value is different */
    crossCheck(0.1);
    crossCheck(0.01);
    crossCheck(0.001);
    crossCheck(epicsThreadSleepQuantum());

    testDiag("Resolution %u ns", (unsigned)epicsMonotonicResolution());

    epicsUInt64 A = epicsMonotonicGet();
    epicsThreadSleep(0.0);
    epicsUInt64 B = epicsMonotonicGet();
    testDiag("epicsThreadSleep(0.0) Delta %u ns", (unsigned)(B-A));

    A = epicsMonotonicGet();
    B = epicsMonotonicGet();
    testDiag("Small Delta %u ns", (unsigned)(B-A));
}

static void testTMGames()
{
    testDiag("testTMGames()");

    epicsTimeStamp now;
    testOk1(!epicsTimeGetCurrent(&now));
    now.nsec = 0; // not relevant

    tm gtm, ltm;
    epicsTimeToTM(&ltm, 0, &now);
    epicsTimeToGMTM(&gtm, 0, &now);

    // we can't do any tests on the decomposed time without knowing the current TZ
    testDiag("LTM mday=%u hour=%u min=%u sec=%u", ltm.tm_mday, ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
    testDiag("GTM mday=%u hour=%u min=%u sec=%u", gtm.tm_mday, gtm.tm_hour, gtm.tm_min, gtm.tm_sec);

    epicsTimeStamp gtime, ltime;
    epicsTimeFromTM(&ltime, &ltm, 0);
    epicsTimeFromGMTM(&gtime, &gtm, 0);

    testOk(now.secPastEpoch==ltime.secPastEpoch, "localtime %u == %u",
           unsigned(now.secPastEpoch), unsigned(ltime.secPastEpoch));

    testOk(now.secPastEpoch==gtime.secPastEpoch, "gmtime %u == %u",
           unsigned(now.secPastEpoch), unsigned(ltime.secPastEpoch));
}

MAIN(epicsTimeTest)
{
    const int wasteTime = 100000;
    const int nTimes = 10;

    testPlan(52 + nTimes * 19);

    testDiag("$TZ = \"%s\"", getenv("TZ"));
    testDiag("EPICS_TZ = \"%s\"", envGetConfigParamPtr(&EPICS_TZ));

#if !defined(_WIN32) && !defined(vxWorks)
    {
        // at least glibc doesn't initialize tzname[2] until some time.h function needs the time zone
        time_t junk = 0;
        (void)localtime(&junk);
        testDiag("Local TZ names \"%s\", \"%s\"", tzname[0], tzname[1]);
    }
#endif

    // sec:ns + double == sec:ns
    testAdd(0,0, 0.0, 0,0);
    testAdd(1,1, 0.0, 1,1);
    testAdd(1,999999999, 0.000000001, 2,0);
    testAdd(1,1, 2.000000002, 3,3);
    testAdd(1,0, -1.0, 0,0);
    testAdd(0,1, -0.000000001, 0,0);
    testAdd(1,1, -1.000000001, 0,0);
    testAdd(0xffffffff,0, -1.0, 0xfffffffe,0);
    testAdd(0x7fffffff,0, 1.0, 0x80000000,0);
    testAdd(0x7fffffff,999999999, 0.000000001, 0x80000000,0);

    // sec:ns - sec:ns == double
    testDiff(0,0, 0,0, 0.0);

    testDiff(0,1, 0,1, 0.0);
    testDiff(1,0, 1,0, 0.0);
    testDiff(1,1, 1,1, 0.0);

    testDiff(2,0, 1,999999999, 0.000000001);
    testDiff(1,999999999, 2,0, -0.000000001);

    testDiff(1,0, 0xffffffff,0, 2.0);
    testDiff(0xffffffff,0, 1,0, -2.0);

    testDiff(1,999999999, 0xffffffff,999999999, 2.0);
    testDiff(0xffffffff,999999999, 1,999999999, -2.0);

    testDiff(0,999999999, 0xffffffff,0, 1.999999999);  // 0.99999.. - -1.0
    testDiff(0xffffffff,0, 0,999999999, -1.999999999); // -1.0 - 0.999..

    testDiff(0x80000000,0, 0x7fffffff,0, 1.0);
    testDiff(0x7fffffff,0, 0x80000000,0, -1.0);

    testDiff(0x80000000,0, 0x7fffffff,999999999, 0.000000001);
    testDiff(0x7fffffff,999999999, 0x80000000,0, -0.000000001);

    testDiff(0x80000000,999999999, 0x7fffffff,0, 1.999999999);
    testDiff(0x7fffffff,0, 0x80000000,999999999, -1.999999999);

    try {
        const epicsTimeStamp epochTS = {0, 0};
        epicsTime epochET = epochTS;
        struct gm_tm_nano_sec epicsEpoch = epochET;

        testOk(epicsEpoch.ansi_tm.tm_sec == 0 &&
               epicsEpoch.ansi_tm.tm_min == 0 &&
               epicsEpoch.ansi_tm.tm_hour == 0 &&
               epicsEpoch.ansi_tm.tm_yday == 0 &&
               epicsEpoch.ansi_tm.tm_year == 90,
               "epicsTime_gmtime() for EPICS epoch");
    }
    catch ( ... ) {
        testFail("epicsTime_gmtime() failed");
        testAbort("Can't continue, check your OS!");
    }

    {   // badNanosecTest
        static const char * pFormat = "%a %b %d %Y %H:%M:%S.%4f";
        try {
            const epicsTimeStamp badTS = {1, 1000000000};
            epicsTime ts(badTS);
            char buf [32];
            ts.strftime(buf, sizeof(buf), pFormat);
            testFail("nanosecond overflow returned \"%s\"", buf);
        }
        catch ( std::exception& ) {
            testPass("nanosecond overflow throws");
        }
    }

    {   // strftime() output
        char buf[80];
        epicsTime et;

        const char * pFormat = "%Y-%m-%d %H:%M:%S.%f";
        et.strftime(buf, sizeof(buf), pFormat);
        testOk(strcmp(buf, "<undefined>") == 0, "undefined => '%s'", buf);

        // This is Noon GMT, when all timezones have the same date
        const epicsTimeStamp tTS = {12*60*60, 98765432};
        et = tTS;
        pFormat = "%Y-%m-%d %S.%09f";   // %H and %M change with timezone
        et.strftime(buf, sizeof(buf), pFormat);
        testOk(strcmp(buf, "1990-01-01 00.098765432") == 0, "'%s' => '%s'", pFormat, buf);

        pFormat = "%S.%03f";
        et.strftime(buf, sizeof(buf), pFormat);
        testOk(strcmp(buf, "00.099") == 0, "'%s' => '%s'", pFormat, buf);

        pFormat = "%S.%04f";
        et.strftime(buf, sizeof(buf), pFormat);
        testOk(strcmp(buf, "00.0988") == 0, "'%s' => '%s'", pFormat, buf);

        pFormat = "%S.%05f";
        et.strftime(buf, sizeof(buf), pFormat);
        testOk(strcmp(buf, "00.09877") == 0, "'%s' => '%s'", pFormat, buf);

        pFormat = "%S.%05f %S.%05f";
        et.strftime(buf, sizeof(buf), pFormat);
        testOk(strcmp(buf, "00.09877 00.09877") == 0, "'%s' => '%s'", pFormat, buf);

        char smbuf[5];
        pFormat = "%S.%05f";
        et.strftime(smbuf, sizeof(smbuf), pFormat);
        testOk(strcmp(smbuf, "00.*") == 0, "'%s' => '%s'", pFormat, smbuf);

        pFormat = "%S.%03f";
        (et + 0.9).strftime(buf, sizeof(buf), pFormat);
        testOk(strcmp(buf, "00.999") == 0, "0.998765 => '%s'", buf);

        pFormat = "%S.%03f";
        (et + 0.901).strftime(buf, sizeof(buf), pFormat);
        testOk(strcmp(buf, "00.999") == 0, "0.999765 => '%s'", buf);

        pFormat = "%%S.%%05f";
        et.strftime(buf, sizeof(buf), pFormat);
        testOk(strcmp(buf, "%S.%05f") == 0, "'%s' => '%s'", pFormat, buf);

        char bigBuf [512];
        memset(bigBuf, '\a', sizeof(bigBuf));
        bigBuf[ sizeof(bigBuf) - 1] = '\0';
        et.strftime(buf, sizeof(buf), bigBuf);
        testOk(strcmp(buf, "<invalid format>") == 0, "bad format => '%s'", buf);
    }

    epicsTime now;
    try {
        now = epicsTime::getCurrent();
        testPass("default time provider");
    }
    catch ( std::exception& ) {
        testFail("epicsTime::getCurrent() throws");
        testAbort("Can't continue, check your time provider");
    }

    testDiag("Running %d loops", nTimes);

    const epicsTime begin = epicsTime::getCurrent();

    for (int loop = 0; loop < nTimes; ++loop) {
        for (int i = 0; i < wasteTime; ++i) {
            now = epicsTime::getCurrent();
        }

        const double diff = now - begin;

        if (loop == 0) {
            testDiag ("%d calls to epicsTime::getCurrent() "
                "averaged %6.3f usec each", wasteTime,
                diff * 1e6 / wasteTime);
        }

        epicsTime copy = now;
        testOk1(copy == now);
        testOk1(copy <= now);
        testOk1(copy >= now);

        testOk1(now > begin);
        testOk1(now >= begin);
        testOk1(begin != now);
        testOk1(begin < now);
        testOk1(begin <= now);

        testOk1(now - now == 0);
        testOk(fabs((now - begin) - diff) < precisionEPICS * 0.01,
               "now - begin ~= diff");

        testOk1(begin + 0 == begin);
        std::cout<<"# begin + diff ("<<(begin + diff)<<") == now ("<<now<<")\n";
        testOk1(begin + diff == now);
        testOk1(now - 0 == now);
        testOk1(now - diff == begin);

        epicsTime end = begin;
        end += diff;
        testOk(end == now, "(begin += diff) == now");

        end = now;
        end -= diff;
        testOk(end == begin, "(now -= diff) == begin");

        // test struct tm round-trip conversion
        local_tm_nano_sec ansiDate = begin;
        epicsTime beginANSI = ansiDate;
        testOk1(beginANSI + diff == now);

        // test struct gmtm round-trip conversion
        gm_tm_nano_sec ansiGmDate = begin;
        epicsTime beginGMANSI = ansiGmDate;
        testOk1(beginGMANSI + diff == now);

        // test struct timespec round-trip conversion
        struct timespec ts = begin;
        epicsTime beginTS = ts;
        testOk1(beginTS + diff == now);
    }

    epicsTime ten_years_hence;
    try {
        now = epicsTime::getCurrent();
        ten_years_hence = now + 60 * 60 * 24 * 3652.5;
        testPass("epicsTime can represent 10 years hence");
    }
    catch ( ... ) {
        testFail("epicsTime exception for value 10 years hence");
    }

    try {
        /* This test exists because in libCom/osi/os/posix/osdTime.cpp
         * the convertDoubleToWakeTime() routine limits the timeout delay
         * to 10 years. libCom/timer/timerQueue.cpp returns DBL_MAX for
         * queues with no timers present, and convertDoubleToWakeTime()
         * has to return an absolute Posix timestamp. On 2028-01-19 any
         * systems that still implement time_t as a signed 32-bit integer
         * will be unable to represent that timestamp, so this will fail.
         */
        time_t_wrapper os_time_t = ten_years_hence;
        epicsTime then = os_time_t; // No fractional seconds
        double delta = ten_years_hence - then;

        testOk(delta >= 0 && delta < 1.0,
            "OS time_t can represent 10 years hence");
    }
    catch ( ... ) {
        testFail("OS time_t conversion exception for value 10 years hence");
    }

    testMonotonic();
    testTMGames();

    return testDone();
}
