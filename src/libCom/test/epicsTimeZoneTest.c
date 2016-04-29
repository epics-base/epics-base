/*************************************************************************\
* Copyright (c) 2015 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdio.h>

#include "envDefs.h"
#include "epicsTime.h"
#include "epicsUnitTest.h"

#include "testMain.h"

#if defined(_WIN32)
#  define tzset _tzset
#endif

static
void setTZ(const char *tz)
{
    testDiag("TZ = \"%s\"", tz);
    epicsEnvSet("TZ", tz);
    tzset();
}

static
void test_localtime(time_t T, int sec, int min, int hour,
                    int mday, int mon, int year,
                    int wday, int yday, int isdst)
{
    struct tm B;
    testDiag("test_localtime(%ld, ...)", (long)T);
    if(epicsTime_localtime(&T, &B)!=epicsTimeOK) {
        testFail("epicsTime_localtime() error");
        testSkip(9, "epicsTime_localtime() failed");
    } else {
        B.tm_year += 1900; /* for readability */
        testPass("epicsTime_localtime() success");
#define TEST(FLD) testOk(B.tm_##FLD==FLD, "%s %d==%d", #FLD, B.tm_##FLD, FLD)
        TEST(sec);
        TEST(min);
        TEST(hour);
        TEST(mday);
        TEST(mon);
        TEST(year);
        TEST(wday);
        TEST(yday);
        TEST(isdst);
#undef TEST
    }
}

static
void test_gmtime(time_t T, int sec, int min, int hour,
                    int mday, int mon, int year,
                    int wday, int yday, int isdst)
{
    struct tm B;
    testDiag("test_gmtime(%ld, ...)", (long)T);
    if(epicsTime_gmtime(&T, &B)!=epicsTimeOK) {
        testFail("epicsTime_localtime() error");
        testSkip(9, "epicsTime_localtime() failed");
    } else {
        B.tm_year += 1900; /* for readability */
        testPass("epicsTime_localtime() success");
#define TEST(FLD) testOk(B.tm_##FLD==FLD, "%s %d==%d", #FLD, B.tm_##FLD, FLD)
        TEST(sec);
        TEST(min);
        TEST(hour);
        TEST(mday);
        TEST(mon);
        TEST(year);
        TEST(wday);
        TEST(yday);
        TEST(isdst);
#undef TEST
    }
}

MAIN(epicsTimeZoneTest)
{
    testPlan(160);
    /* 1445259616
     *  Mon Oct 19 09:00:16 2015 EDT
     *  Mon Oct 19 08:00:16 2015 CDT
     *  Mon Oct 19 03:00:16 2015 HST  (no dst)
     *  Mon Oct 19 13:00:16 2015 UTC
     */
    testDiag("POSIX 1445259616");
    setTZ("EST5EDT");
    test_localtime(1445259616ul, 16, 0, 9,  19, 9, 2015, 1, 291, 1);
    test_gmtime   (1445259616ul, 16, 0, 13, 19, 9, 2015, 1, 291, 0);
    setTZ("CST6CDT");
    test_localtime(1445259616ul, 16, 0, 8,  19, 9, 2015, 1, 291, 1);
    test_gmtime   (1445259616ul, 16, 0, 13, 19, 9, 2015, 1, 291, 0);
#if defined(__rtems__)
    setTZ("HST10HST10");
#else
    setTZ("HST10");
#endif
    test_localtime(1445259616ul, 16, 0, 3,  19, 9, 2015, 1, 291, 0);
    test_gmtime   (1445259616ul, 16, 0, 13, 19, 9, 2015, 1, 291, 0);
    setTZ("UTC0");
    test_localtime(1445259616ul, 16, 0, 13, 19, 9, 2015, 1, 291, 0);
    test_gmtime   (1445259616ul, 16, 0, 13, 19, 9, 2015, 1, 291, 0);
    /* 1421244931
     *  Wed Jan 14 09:15:31 2015 EST
     *  Wed Jan 14 08:15:31 2015 CST
     *  Wed Jan 14 04:15:31 2015 HST
     *  Wed Jan 14 14:15:31 2015 UTC
     */
    testDiag("POSIX 1421244931");
    setTZ("EST5EDT");
    test_localtime(1421244931ul, 31, 15,  9, 14, 0, 2015, 3, 13, 0);
    test_gmtime   (1421244931ul, 31, 15, 14, 14, 0, 2015, 3, 13, 0);
    setTZ("CST6CDT");
    test_localtime(1421244931ul, 31, 15,  8, 14, 0, 2015, 3, 13, 0);
    test_gmtime   (1421244931ul, 31, 15, 14, 14, 0, 2015, 3, 13, 0);
#if defined(__rtems__)
    setTZ("HST10HST10");
#else
    setTZ("HST10");
#endif
    test_localtime(1421244931ul, 31, 15,  4, 14, 0, 2015, 3, 13, 0);
    test_gmtime   (1421244931ul, 31, 15, 14, 14, 0, 2015, 3, 13, 0);
    setTZ("UTC0");
    test_localtime(1421244931ul, 31, 15, 14, 14, 0, 2015, 3, 13, 0);
    test_gmtime   (1421244931ul, 31, 15, 14, 14, 0, 2015, 3, 13, 0);
    return testDone();
}
