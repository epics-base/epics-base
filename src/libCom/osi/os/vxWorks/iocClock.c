/* iocClock.c */

/* Author:  Marty Kraimer Date:  16JUN2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

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

#include "epicsTypes.h"
#include "cantProceed.h"
#include "errlog.h"
#include "osiThread.h"
#include "osiSem.h"
#include "tsStamp.h"
#include "iocClock.h"

#define BILLION 1000000000
#define iocClockSyncRate 10.0

static int iocClockGetCurrent(TS_STAMP *pDest);
static int iocClockGetEvent(TS_STAMP *pDest, unsigned eventNumber);

typedef struct iocClockPvt {
    semMutexId  lock;
    TS_STAMP    clock;
    unsigned long lastTick;
    epicsUInt32 nanosecondsPerTick;
    int         tickRate;
    int         ticksToSkip;
    ptsStampGetCurrent getCurrent;
    ptsStampGetEvent getEvent;
    char        *pserverAddr;
}iocClockPvt;
static iocClockPvt *piocClockPvt = 0;

extern char* sysBootLine;

static void syncNTP(void)
{
    struct timespec Currtime;
    TS_STAMP epicsTime;
    STATUS status;
    int prevStatusBad = 0;
    int firstTime=1;

    while(1) {
        double diffTime;
        if(!firstTime)threadSleep(iocClockSyncRate);
        firstTime = 0;
        status = sntpcTimeGet(piocClockPvt->pserverAddr,
            piocClockPvt->tickRate,&Currtime);
        if(status) {
            if(!prevStatusBad)
                errlogPrintf("iocClockSyncWithNTPserver: sntpcTimeGet %s\n",
                    strerror(errno));
            prevStatusBad = 1;
            continue;
        }
        if(prevStatusBad) {
            errlogPrintf("iocClockSyncWithNTPserver: sntpcTimeGet OK\n");
            prevStatusBad = 0;
        }
        tsStampFromTimespec(&epicsTime,&Currtime);
        semMutexMustTake(piocClockPvt->lock);
        diffTime = tsStampDiffInSeconds(&epicsTime,&piocClockPvt->clock);
        if(diffTime>=0.0) {
            piocClockPvt->clock = epicsTime;
        } else {/*dont go back in time*/
            piocClockPvt->ticksToSkip = (int) (diffTime*piocClockPvt->tickRate);
        }
        piocClockPvt->lastTick = tickGet();
        semMutexGive(piocClockPvt->lock);
    }
}

void iocClockInit()
{
    if(piocClockPvt) return;
    piocClockPvt = callocMustSucceed(1,sizeof(iocClockPvt),"iocClockInit");
    piocClockPvt->lock = semMutexMustCreate();
    piocClockPvt->nanosecondsPerTick = BILLION/sysClkRateGet();
    piocClockPvt->tickRate = sysClkRateGet();
    piocClockPvt->getCurrent = iocClockGetCurrent;
    piocClockPvt->getEvent = iocClockGetEvent;
    /* look first for environment variable */
    piocClockPvt->pserverAddr = getenv("EPICS_TS_NTP_INET");
    if(!piocClockPvt->pserverAddr) { /* next look for boot server*/
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
    threadCreate("syncNTP",
        threadPriorityHigh,threadGetStackSize(threadStackSmall),
        (THREADFUNC)syncNTP,0);
    return;
}

void iocClockRegister(ptsStampGetCurrent getCurrent,
    ptsStampGetEvent getEvent)
{
    if(piocClockPvt) {
        printf("iocClockRegister: iocClock already initialized\n");
        return;
    }
    piocClockPvt = callocMustSucceed(1,sizeof(iocClockPvt),"iocClockRegister");
    piocClockPvt->getCurrent = getCurrent;
    piocClockPvt->getEvent = getEvent;
}

int iocClockGetCurrent(TS_STAMP *pDest)
{
    unsigned long currentTick,nticks,nsecs;

    semMutexMustTake(piocClockPvt->lock);
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
    semMutexGive(piocClockPvt->lock);
    return(0);
}

int iocClockGetEvent(TS_STAMP *pDest, unsigned eventNumber)
{
     if (eventNumber==tsStampEventCurrentTime) {
         *pDest = piocClockPvt->clock;
         return(0);
     }
     return(tsStampERROR);
}

int tsStampGetCurrent (TS_STAMP *pDest)
{
    if(piocClockPvt->getCurrent) return((*piocClockPvt->getCurrent)(pDest));
    return(tsStampERROR);
}

int tsStampGetEvent (TS_STAMP *pDest, unsigned eventNumber)
{
    if(piocClockPvt->getEvent)
        return((*piocClockPvt->getEvent)(pDest,eventNumber));
    return(tsStampERROR);
}

/* Unless
   putenv("TIMEZONE=<name>::<minutesWest>:<start daylight>:<end daylight>")
   is executed before and epics software is loaded, UTC rather than local
   time will be displayed
*/
void date()
{
    TS_STAMP now;
    char nowText[40];
    size_t rtn;

    rtn = tsStampGetCurrent(&now);
    if(rtn) {
        printf("tsStampGetCurrent failed\n");
        return;
    }
    nowText[0] = 0;
    rtn = tsStampToStrftime(nowText,sizeof(nowText),
        "%Y/%m/%d %H:%M:%S.%06f",&now);
    printf("%s\n",nowText);
}
