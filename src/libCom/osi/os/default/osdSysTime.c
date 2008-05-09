/* Sheng Peng @ SNS ORNL 07/2004 */
/* Peter Denison, Diamond Light Source, Apr 2008 */
/* Eric Norum, Argonne National Labs, Apr 2008 */
/* Version 1.3 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <errno.h>

#include "epicsTypes.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsTime.h"
#include "generalTimeSup.h"

#define BILLION 1000000000

#define SYS_TIME_DRV_VERSION "system Time Driver Version 1.3"
#define SYNC_PERIOD 60.0


static void SysTimeSyncTime(void *param);
static int SysTimeGetCurrent(epicsTimeStamp *pDest);

long SysTime_Report(int level);

typedef struct SysTimePvt
{
    int synced; /* if never synced, we can't use it */
    epicsMutexId lock;
    epicsTimeStamp lastReportedTS;
    epicsTimeStamp lastSyncedTS;
    struct timespec lastSyncedSysTime;
    int priority;
    int synced_priority;
} SysTimePvt;

static SysTimePvt *pSysTimePvt = 0;

static void SysTimeSyncTime(void *param)
{
    epicsTimeStamp now;
    struct timespec sys_time_now;

    for (;;) {
        epicsThreadSleep(SYNC_PERIOD);
        /* Ask for the best time available, not including ourselves */
        if (generalTimeGetExceptPriority(&now, &pSysTimePvt->synced_priority,
                pSysTimePvt->priority) == epicsTimeOK) {
            /* It's a good time, we unconditionally sync ourselves to it, as
             this driver is the bottom of the heap */
            clock_gettime(CLOCK_REALTIME, &sys_time_now);

            epicsMutexMustLock(pSysTimePvt->lock);
            pSysTimePvt->lastSyncedTS = now;
            pSysTimePvt->lastSyncedSysTime = sys_time_now;
            pSysTimePvt->synced = 1;
            epicsMutexUnlock(pSysTimePvt->lock);
        }
    }
}

long SysTime_InitOnce(int priority)
{
    pSysTimePvt = callocMustSucceed(1, sizeof(SysTimePvt), "SysTime_Init");
    pSysTimePvt->synced = 0;
    pSysTimePvt->lock = epicsMutexCreate();
    pSysTimePvt->priority = priority;

    /* register to link list */
    generalTimeCurrentTpRegister("system Time", priority, SysTimeGetCurrent);

    /* Create the synchronization thread */
    epicsThreadCreate("SysTimeSync", epicsThreadPriorityLow,
            epicsThreadGetStackSize(epicsThreadStackSmall), SysTimeSyncTime, 
            NULL);
    return 0;
}

struct InitInfo
{
    int priority;
    long retval;
};
static void SysTime_InitOnceWrapper(void *arg)
{
    struct InitInfo *pargs = (struct InitInfo *)arg;
    pargs->retval = SysTime_InitOnce(pargs->priority);
}

long SysTime_Init(int priority)
{
    struct InitInfo args;
    static epicsThreadOnceId onceId= EPICS_THREAD_ONCE_INIT;

    args.priority = priority;
    epicsThreadOnce(&onceId, SysTime_InitOnceWrapper, &args);
    return args.retval;
}

static int SysTimeGetCurrent(epicsTimeStamp *pDest)
{
    struct timespec cur_systime;
    unsigned long diff_sec, diff_nsec;
    epicsTimeStamp epicsTime;

    double diffTime;

    epicsMutexMustLock(pSysTimePvt->lock);

    clock_gettime(CLOCK_REALTIME, &cur_systime);

    if (!pSysTimePvt->synced) {
        if (cur_systime.tv_sec < POSIX_TIME_AT_EPICS_EPOCH) { /* Earlier than 1990 is clearly wrong. Set the time to 1/1/90
         + 1 day (to avoid timezone problems) and set the SysWorks
         clock to that point */
            cur_systime.tv_sec = POSIX_TIME_AT_EPICS_EPOCH + 86400;
            cur_systime.tv_nsec = 0;
            clock_settime(CLOCK_REALTIME, &cur_systime);
            printf("****************************"
                "***************************************************\n"
                "WARNING: System time not set. Initialised to 2/1/90\n"
                "*****************************************************"
                "**************************\n");
        }
        epicsTimeFromTimespec(&epicsTime, &cur_systime);
    }
    else/* synced, need calculation */
    {/* the system time is always monotonic */
        if (cur_systime.tv_nsec >= pSysTimePvt->lastSyncedSysTime.tv_nsec) {
            diff_sec = cur_systime.tv_sec
                    - pSysTimePvt->lastSyncedSysTime.tv_sec;
            diff_nsec = cur_systime.tv_nsec
                    - pSysTimePvt->lastSyncedSysTime.tv_nsec;
        } else {
            diff_sec = cur_systime.tv_sec
                    - pSysTimePvt->lastSyncedSysTime.tv_sec - 1;
            diff_nsec
                    = BILLION - pSysTimePvt->lastSyncedSysTime.tv_nsec + cur_systime.tv_nsec;
        }

        epicsTime.nsec = pSysTimePvt->lastSyncedTS.nsec + diff_nsec;
        epicsTime.secPastEpoch = pSysTimePvt->lastSyncedTS.secPastEpoch
                + diff_sec;
        if (epicsTime.nsec >= BILLION) {
            epicsTime.nsec -= BILLION;
            epicsTime.secPastEpoch ++;
        }
    }

    diffTime = epicsTimeDiffInSeconds(&epicsTime, &pSysTimePvt->lastReportedTS);
    if (diffTime >= 0.0) {/* time is monotonic */
        *pDest = epicsTime;
        pSysTimePvt->lastReportedTS = epicsTime;
    } else {/* time never goes back */
        *pDest = pSysTimePvt->lastReportedTS;
    }

    epicsMutexUnlock(pSysTimePvt->lock);
    return (0);
}

long SysTime_Report(int level)
{
    printf(SYS_TIME_DRV_VERSION"\n");

    if (!pSysTimePvt) {/* drvSysTime is not used, we just report version then quit */
        printf("system time driver is not initialized yet!\n\n");
    } else {
        printf("%synced to %d. Last time = %lds, %ldns. ",
                pSysTimePvt->synced ? "S" : "Not s",
                pSysTimePvt->synced_priority,
                pSysTimePvt->lastSyncedSysTime.tv_sec,
                pSysTimePvt->lastSyncedSysTime.tv_nsec);
    }
    return 0;
}
