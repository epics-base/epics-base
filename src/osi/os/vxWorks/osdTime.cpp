/*************************************************************************\
* Copyright (c) 2015 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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

#include "epicsTime.h"
#include "osiNTPTime.h"
#include "osiClockTime.h"
#include "generalTimeSup.h"
#include "envDefs.h"

#define NTP_REQUEST_TIMEOUT 4 /* seconds */

static char sntp_sync_task[] = "ipsntps";
static char ntp_daemon[] = "ipntpd";

static const char *pserverAddr = NULL;
extern char* sysBootLine;

static int timeRegister(void)
{
    /* If TIMEZONE not defined, set it from EPICS_TIMEZONE */
    if (getenv("TIMEZONE") == NULL) {
        const char *timezone = envGetConfigParamPtr(&EPICS_TIMEZONE);
        if (timezone == NULL) {
            printf("timeRegister: No Time Zone Information\n");
        } else {
            epicsEnvSet("TIMEZONE", timezone);
        }
    }

    // Define EPICS_TS_FORCE_NTPTIME to force use of NTPTime provider
    bool useNTP = getenv("EPICS_TS_FORCE_NTPTIME") != NULL;

    if (!useNTP &&
        (taskNameToId(sntp_sync_task) != ERROR ||
         taskNameToId(ntp_daemon) != ERROR)) {
        // A VxWorks 6 SNTP/NTP sync task is running
        struct timespec clockNow;

        useNTP = clock_gettime(CLOCK_REALTIME, &clockNow) != OK ||
            clockNow.tv_sec < BUILD_TIME;
            // Assumes VxWorks and the host OS have the same epoch
    }

    if (useNTP) {
        // Start NTP first so it can be used to sync SysTime
        NTPTime_Init(100);
        ClockTime_Init(CLOCKTIME_SYNC);
    } else {
        ClockTime_Init(CLOCKTIME_NOSYNC);
    }
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
