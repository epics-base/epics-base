/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * $Id$
 *
 * Author: W. Eric Norum
 */

/*
 * ANSI C
 */
#include <time.h>
#include <limits.h>

/*
 * RTEMS
 */
#include <rtems.h>

/*
 * EPICS
 */
#define epicsExportSharedSymbols
#include <epicsStdio.h>
#include <cantProceed.h>
#include <iocClock.h>
#include <errlog.h>

/*
 * RTEMS time begins January 1, 1988 (local time).
 * EPICS time begins January 1, 1990 (GMT).
 */
#define EPICS_EPOCH_SEC_PAST_RTEMS_EPOCH ((366+365)*86400)

static int iocClockGetCurrent(epicsTimeStamp *pDest);
static int iocClockGetEvent(epicsTimeStamp *pDest, int eventNumber);

typedef struct iocClockPvt {
    pepicsTimeGetCurrent getCurrent;
    pepicsTimeGetEvent getEvent;
}iocClockPvt;
static iocClockPvt *piocClockPvt = 0;

void
iocClockInit(void)
{
    if(piocClockPvt) return;
    piocClockPvt = (iocClockPvt *)callocMustSucceed(1,sizeof(iocClockPvt),"iocClockInit");
    piocClockPvt->getCurrent = iocClockGetCurrent;
    piocClockPvt->getEvent = iocClockGetEvent;
}

void
iocClockRegister(pepicsTimeGetCurrent getCurrent, pepicsTimeGetEvent getEvent)
{
    if(piocClockPvt)
        printf("iocClockRegister: iocClock already initialized -- overriding --\n");
    else
        piocClockPvt = (iocClockPvt *)callocMustSucceed(1,sizeof(iocClockPvt),"iocClockRegister");
    piocClockPvt->getCurrent = getCurrent;
    piocClockPvt->getEvent = getEvent;
}

static int
iocClockGetCurrent(epicsTimeStamp *pDest)
{
    struct timeval curtime;
    rtems_interval t;
    rtems_status_code sc;

    for (;;) {
        sc = rtems_clock_get (RTEMS_CLOCK_GET_TIME_VALUE, &curtime);
        if (sc == RTEMS_SUCCESSFUL)
            break;
        else if (sc != RTEMS_NOT_DEFINED)
            return epicsTimeERROR;
        sc = rtems_clock_get (RTEMS_CLOCK_GET_TICKS_PER_SECOND, &t);
        if (sc != RTEMS_SUCCESSFUL)
            return epicsTimeERROR;
        rtems_task_wake_after (t);
    }
    pDest->nsec = curtime.tv_usec * 1000;
    pDest->secPastEpoch = curtime.tv_sec - EPICS_EPOCH_SEC_PAST_RTEMS_EPOCH;
   return epicsTimeOK;
}

static int
iocClockGetEvent(epicsTimeStamp *pDest, int eventNumber)
{
     if (eventNumber==epicsTimeEventCurrentTime)
        return iocClockGetCurrent(pDest);
     return(epicsTimeERROR);
}

int
epicsTimeGetCurrent (epicsTimeStamp *pDest)
{
    if(!piocClockPvt)
        iocClockInit();
    if(piocClockPvt->getCurrent) return((*piocClockPvt->getCurrent)(pDest));
    return(epicsTimeERROR);
}

int
epicsTimeGetEvent (epicsTimeStamp *pDest, int eventNumber)
{
    if(!piocClockPvt)
        iocClockInit();
    if(piocClockPvt->getEvent)
        return((*piocClockPvt->getEvent)(pDest,eventNumber));
    return(epicsTimeERROR);
}
