/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <tickLib.h>
#include <sysLib.h>
#include <sntpcLib.h>
#include <string.h>

#include "epicsTime.h"
#include "errlog.h"
#include "osiNTPTime.h"
#include "osdSysTime.h"
#include "generalTimeSup.h"
#include "envDefs.h"

static const char *pserverAddr = NULL;
extern char* sysBootLine;

static int timeRegister(void)
{
    NTPTime_Init(100); /* init NTP first so it can be used to sync SysTime */
    SysTime_Init(LAST_RESORT_PRIORITY);
    return 1;
}
static int done = timeRegister();

int osdNTPGet(struct timespec *ts)
{
    return sntpcTimeGet((char *)pserverAddr, sysClkRateGet() ,ts);
}

void osdNTPInit(void)
{
    pserverAddr = envGetConfigParamPtr(&EPICS_TS_NTP_INET);
    if(!pserverAddr) { /* if neither, use the boot host */
        BOOT_PARAMS bootParms;
        static char host_addr[BOOT_ADDR_LEN];
        bootStringToStruct(sysBootLine,&bootParms);
        /* bootParms.had = host IP address */
        strncpy(host_addr,bootParms.had,BOOT_ADDR_LEN);
        pserverAddr = host_addr;
    }
    if(!pserverAddr) {
        errlogPrintf("No NTP server is defined. Clock does not work\n");
    }
}

// vxWorks localtime_r interface does not match POSIX standards
int epicsTime_localtime ( const time_t *clock, struct tm *result )
{
    int status = localtime_r ( clock, result );
    if ( status == OK ) {
        return epicsTimeOK;
    }
    else {
        return epicsTimeERROR;
    }
}

// vxWorks gmtime_r interface does not match POSIX standards
int epicsTime_gmtime ( const time_t *pAnsiTime, struct tm *pTM )
{
    int status = gmtime_r ( pAnsiTime, pTM );
    if ( status == OK ) {
        return epicsTimeOK;
    }
    else {
        return epicsTimeERROR;
    }
}
