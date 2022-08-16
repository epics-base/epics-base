/*************************************************************************\
* Copyright (c) 2015 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#define _VSB_CONFIG_FILE <../lib/h/config/vsbConfig.h>

#include <vxWorks.h>
#include <sntpcLib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <taskLib.h>

#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include "epicsTime.h"
#include "osiNTPTime.h"
#include "osiClockTime.h"
#include "generalTimeSup.h"
#include "envDefs.h"
#include "epicsFindSymbol.h"

#define NTP_REQUEST_TIMEOUT 4 /* seconds */

extern "C" {
    int tz2timezone(void);
}

static char sntp_sync_task[] = "ipsntps";
typedef int (*ipcom_ipd_kill_t)(const char *);

static const char *pserverAddr = NULL;
static CLOCKTIME_SYNCHOOK prevHook;

extern char* sysBootLine;

static void timeSync(int synchronized) {
    if (prevHook)
        prevHook(synchronized);

    if (synchronized) {
        struct timespec tsNow;
        if (clock_gettime(CLOCK_REALTIME, &tsNow) != OK)
            return;

        struct tm tmNow;
        localtime_r(&tsNow.tv_sec, &tmNow);

        static int lastYear = 0;
        if (tmNow.tm_year > lastYear && !tz2timezone())
            lastYear = tmNow.tm_year;
    }
}

static int timeRegister(void)
{
    /* If TZ not defined, set it from EPICS_TZ */
    if (getenv("TZ") == NULL) {
        const char *tz = envGetConfigParamPtr(&EPICS_TZ);

        if (tz && *tz) {
            epicsEnvSet("TZ", tz);

            // Call tz2timezone() from the sync thread, needs the year
            prevHook = ClockTime_syncHook;
            ClockTime_syncHook = timeSync;
        }
        else if (getenv("TIMEZONE") == NULL)
            printf("timeRegister: No Time Zone Information available\n");
    }

    // Define EPICS_TS_FORCE_NTPTIME to force use of NTPTime provider
    bool useNTP = getenv("EPICS_TS_FORCE_NTPTIME") != NULL;

    // VxWorks 6 may have a clock sync task running
    int tid = taskNameToId(sntp_sync_task);
    struct timespec clockNow;
    if (tid != ERROR &&
        clock_gettime(CLOCK_REALTIME, &clockNow) == OK &&
        clockNow.tv_sec > BUILD_TIME) {
        // Clock appears set
        tz2timezone();
    }
    else {
        // No clock sync task or it's broken, start our own
        useNTP = 1;
    }

    if (useNTP && tid != ERROR) {
        // EPICS_TS_FORCE_NTPTIME was set, stop the OS sync task
        ipcom_ipd_kill_t ipcom_ipd_kill =
            (ipcom_ipd_kill_t) epicsFindSymbol("ipcom_ipd_kill");

        if (ipcom_ipd_kill && !taskIsSuspended(tid)) {
            printf("timeRegister: Stopping VxWorks clock sync task\n");
            ipcom_ipd_kill("ipsntp");
        }
    }

    if (useNTP) {
        // Start NTP first so it can be used to sync ClockTime
        NTPTime_Init(100);
        ClockTime_Init(CLOCKTIME_SYNC);
    }
    else {
        ClockTime_Init(CLOCKTIME_NOSYNC);
    }
    osdMonotonicInit();
    return 1;
}
static int done = timeRegister();


// Routines for NTPTime provider

int osdNTPGet(struct timespec *ts)
{
    return sntpcTimeGet(const_cast<char *>(pserverAddr),
        NTP_REQUEST_TIMEOUT * sysClkRateGet(), ts);
}

void osdNTPInit(void)
{
    pserverAddr = envGetConfigParamPtr(&EPICS_TS_NTP_INET);
    if (!pserverAddr) { /* use the boot host */
        BOOT_PARAMS bootParms;
        static char host_addr[BOOT_ADDR_LEN];

        bootStringToStruct(sysBootLine, &bootParms);
        /* bootParms.had = host IP address */
        strncpy(host_addr, bootParms.had, BOOT_ADDR_LEN);
        pserverAddr = host_addr;
    }
}

void osdNTPReport(void)
{
    if (pserverAddr)
        printf("NTP Server = %s\n", pserverAddr);
}

void osdClockReport(int level)
{
    int tid = taskNameToId(sntp_sync_task);

    printf("VxWorks clock sync task '%s' is %s\n", sntp_sync_task,
        (tid == ERROR) ? "not running" :
        taskIsSuspended(tid) ? "suspended" : "running");
}

// Other Time Routines

// vxWorks localtime_r returns different things in different versions.
// It can't fail though, so we just ignore the return value.
int epicsTime_localtime(const time_t *clock, struct tm *result)
{
    localtime_r(clock, result);
    return epicsTimeOK;
}

// vxWorks gmtime_r returns different things in different versions.
// It can't fail though, so we just ignore the return value.
int epicsTime_gmtime ( const time_t *pAnsiTime, struct tm *pTM )
{
    gmtime_r(pAnsiTime, pTM);
    return epicsTimeOK;
}
