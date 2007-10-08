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
/*l         Modified by Eric Norum to work with RTEMS */

#include <time.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <rtems.h>
#include <epicsStdio.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <epicsTime.h>
#include <cantProceed.h>
#include <iocClock.h>
#include <errlog.h>

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
}iocClockPvt;
static iocClockPvt *piocClockPvt = 0;
static int nConsecutiveBad = 0;

static int
tickGet(void)
{
    rtems_interval t;
    rtems_clock_get (RTEMS_CLOCK_GET_TICKS_SINCE_BOOT, &t);
    return t;
}
static int
sysClkRateGet(void)
{
    rtems_interval t;
    rtems_clock_get (RTEMS_CLOCK_GET_TICKS_PER_SECOND, &t);
    return t;
}

static void syncNTP(void)
{
    struct timespec Currtime;
    epicsTimeStamp epicsTime;
    int status;
    int prevStatusBad = 0;
    int firstTime=1;

    while(1) {
        double diffTime;
        extern int rtems_bsdnet_get_ntp(int, int(*)(), struct timespec *);
        if(!firstTime)epicsThreadSleep(iocClockSyncRate);
        firstTime = 0;
        if(piocClockPvt->getCurrent != iocClockGetCurrent) {
            errlogPrintf("syncNTP: NTP client terminating.\n");
            return;
        }
        status = rtems_bsdnet_get_ntp(-1, NULL, &Currtime);
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
    epicsThreadCreate("syncNTP",
        epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        (EPICSTHREADFUNC)syncNTP,0);
    return;
}

void iocClockRegister(pepicsTimeGetCurrent getCurrent,
    pepicsTimeGetEvent getEvent)
{
    if(piocClockPvt)
        printf("iocClockRegister: iocClock already initialized -- overriding\n");
    else
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

