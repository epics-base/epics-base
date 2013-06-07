/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <vxWorks.h>
#include <sntpcLib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "epicsTime.h"
#include "osiNTPTime.h"
#include "osiClockTime.h"
#include "generalTimeSup.h"
#include "envDefs.h"

#define NTP_REQUEST_TIMEOUT 4 /* seconds */

static const char *pserverAddr = NULL;
extern char* sysBootLine;

static int timeRegister(void)
{
    /* If TIMEZONE not defined, set it from EPICS_TIMEZONE */
    if (getenv("TIMEZONE") == NULL) {
        const char *timezone = envGetConfigParamPtr(&EPICS_TIMEZONE);
        if (timezone == NULL) {
            printf("NTPTime_Init: No Time Zone Information\n");
        } else {
            epicsEnvSet("TIMEZONE", timezone);
        }
    }

    NTPTime_Init(100); /* init NTP first so it can be used to sync SysTime */
    ClockTime_Init(CLOCKTIME_SYNC);
    return 1;
}
static int done = timeRegister();

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
    printf("NTP Server = %s\n", pserverAddr);
}


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
