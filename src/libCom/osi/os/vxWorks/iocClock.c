/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocClock.c */

/* Author:  Marty Kraimer Date:  16JUN2000 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <vxWorks.h>
#include <bootLib.h>
#include <tickLib.h>
#include <sysLib.h>
#include <sntpcLib.h>
#include <time.h>
#include <errno.h>
#include <envLib.h>

#include "epicsTypes.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsTime.h"
#include "iocClock.h"
#include "envDefs.h"

#define BILLION 1000000000
#define iocClockSyncRate 10.0

static int iocClockGetCurrent(epicsTimeStamp *pDest);
static int iocClockGetEvent(epicsTimeStamp *pDest, int eventNumber);

typedef struct iocClockPvt {
    epicsMutexId  lock;
    epicsTimeStamp    clock;
    unsigned long lastTick;
    epicsUInt32 nanosecondsPerTick;
    int         tickRate;
    int         ticksToSkip;
    pepicsTimeGetCurrent getCurrent;
    pepicsTimeGetEvent getEvent;
    const char *pserverAddr;
}iocClockPvt;
static iocClockPvt *piocClockPvt = 0;
static int nConsecutiveBad = 0;

extern char* sysBootLine;

static void syncNTP(void)
{
    struct timespec Currtime;
    epicsTimeStamp epicsTime;
    STATUS status;
    int prevStatusBad = 0;
    int firstTime=1;

    while(1) {
        double diffTime;
        if(!firstTime)epicsThreadSleep(iocClockSyncRate);
        firstTime = 0;
        status = sntpcTimeGet((char *)piocClockPvt->pserverAddr,
            piocClockPvt->tickRate,&Currtime);
        if(status) {
            ++nConsecutiveBad;
            /*wait 1 minute before reporting failure*/
            if(nConsecutiveBad<(60/iocClockSyncRate)) continue;
            if(!prevStatusBad)
                errlogPrintf("iocClockSyncWithNTPserver: sntpcTimeGet %s\n",
                    strerror(errno));
            prevStatusBad = 1;
            continue;
        }
        nConsecutiveBad=0;
        if(prevStatusBad) {
            errlogPrintf("iocClockSyncWithNTPserver: sntpcTimeGet OK\n");
            prevStatusBad = 0;
        }
        epicsTimeFromTimespec(&epicsTime,&Currtime);
        epicsMutexMustLock(piocClockPvt->lock);
        diffTime = epicsTimeDiffInSeconds(&epicsTime,&piocClockPvt->clock);
        if(diffTime>=0.0) {
            piocClockPvt->clock = epicsTime;
        } else {/*dont go back in time*/
            piocClockPvt->ticksToSkip = (int) (diffTime*piocClockPvt->tickRate);
        }
        piocClockPvt->lastTick = tickGet();
        epicsMutexUnlock(piocClockPvt->lock);
    }
}

void iocClockInit()
{
    if(piocClockPvt) return;
    piocClockPvt = callocMustSucceed(1,sizeof(iocClockPvt),"iocClockInit");
    piocClockPvt->lock = epicsMutexCreate();
    piocClockPvt->nanosecondsPerTick = BILLION/sysClkRateGet();
    piocClockPvt->tickRate = sysClkRateGet();
    piocClockPvt->getCurrent = iocClockGetCurrent;
    piocClockPvt->getEvent = iocClockGetEvent;
    /* look first for environment variable or CONFIG_SITE_ENV default */
    piocClockPvt->pserverAddr = envGetConfigParamPtr(&EPICS_TS_NTP_INET);
    if(!piocClockPvt->pserverAddr) { /* if neither, use the boot host */
        BOOT_PARAMS bootParms;
        static char host_addr[BOOT_ADDR_LEN];
        bootStringToStruct(sysBootLine,&bootParms);
        /* bootParms.had = host IP address */
        strncpy(host_addr,bootParms.had,BOOT_ADDR_LEN);
        piocClockPvt->pserverAddr = host_addr;
    }
    if(!piocClockPvt->pserverAddr) {
        errlogPrintf("No NTP server is defined. Clock does not work\n");
        return;
    }
    /* if TIMEZONE not defined, set it from EPICS_TIMEZONE */
    if (getenv("TIMEZONE") == NULL) {
        const char *timezone = envGetConfigParamPtr(&EPICS_TIMEZONE);
        if(timezone == NULL) {
            printf("iocClockInit: No Time Zone Information\n");
        } else {
            char envtimezone[100];
            sprintf(envtimezone,"TIMEZONE=%s",timezone);
            if(putenv(envtimezone)==ERROR) {
                printf("iocClockInit: TIMEZONE putenv failed\n");
            }
        }
    }
    epicsThreadCreate("syncNTP",
        epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        (EPICSTHREADFUNC)syncNTP,0);
    return;
}

void iocClockRegister(pepicsTimeGetCurrent getCurrent,
    pepicsTimeGetEvent getEvent)
{
    if(piocClockPvt) {
        printf("iocClockRegister: iocClock already initialized\n");
        return;
    }
    piocClockPvt = callocMustSucceed(1,sizeof(iocClockPvt),"iocClockRegister");
    piocClockPvt->getCurrent = getCurrent;
    piocClockPvt->getEvent = getEvent;
}

int iocClockGetCurrent(epicsTimeStamp *pDest)
{
    unsigned long currentTick,nticks,nsecs;

    epicsMutexMustLock(piocClockPvt->lock);
    currentTick = tickGet();
    while(currentTick!=piocClockPvt->lastTick) {
        nticks = (currentTick>piocClockPvt->lastTick)
            ? (currentTick - piocClockPvt->lastTick)
            : (currentTick + (ULONG_MAX - piocClockPvt->lastTick));
        if(piocClockPvt->ticksToSkip>0) {/*dont go back in time*/
            if(nticks<piocClockPvt->ticksToSkip) {
                piocClockPvt->ticksToSkip -= nticks;
                break;
            }
            nticks -= piocClockPvt->ticksToSkip;
        }
        piocClockPvt->lastTick = currentTick;
        nsecs = nticks/piocClockPvt->tickRate;
        nticks = nticks - nsecs*piocClockPvt->tickRate;
        piocClockPvt->clock.nsec += nticks * piocClockPvt->nanosecondsPerTick;
        if(piocClockPvt->clock.nsec>=BILLION) {
            ++nsecs;
            piocClockPvt->clock.nsec -= BILLION;
        }
        piocClockPvt->clock.secPastEpoch += nsecs;
    }
    *pDest = piocClockPvt->clock;
    epicsMutexUnlock(piocClockPvt->lock);
    return(0);
}

int iocClockGetEvent(epicsTimeStamp *pDest, int eventNumber)
{
     if (eventNumber==epicsTimeEventCurrentTime) {
         *pDest = piocClockPvt->clock;
         return(0);
     }
     return(epicsTimeERROR);
}

int epicsTimeGetCurrent (epicsTimeStamp *pDest)
{
    if(!piocClockPvt) {
        iocClockInit();
        /*wait two seconds for syncNTP to contact network time server*/
        epicsThreadSleep(2.0); 
    }
    if(piocClockPvt->getCurrent) return((*piocClockPvt->getCurrent)(pDest));
    return(epicsTimeERROR);
}

int epicsTimeGetEvent (epicsTimeStamp *pDest, int eventNumber)
{
    if(!piocClockPvt) {
        iocClockInit();
        /*wait two seconds for syncNTP to contact network time server*/
        epicsThreadSleep(2.0); 
    }
    if(piocClockPvt->getEvent)
        return((*piocClockPvt->getEvent)(pDest,eventNumber));
    return(epicsTimeERROR);
}

/* Unless
   putenv("TIMEZONE=<name>::<minutesWest>:<start daylight>:<end daylight>")
   is executed before and epics software is loaded, UTC rather than local
   time will be displayed
*/
void date()
{
    epicsTimeStamp now;
    char nowText[40];
    size_t rtn;

    rtn = epicsTimeGetCurrent(&now);
    if(rtn) {
        printf("epicsTimeGetCurrent failed\n");
        return;
    }
    nowText[0] = 0;
    rtn = epicsTimeToStrftime(nowText,sizeof(nowText),
        "%Y/%m/%d %H:%M:%S.%06f",&now);
    printf("%s\n",nowText);
}
