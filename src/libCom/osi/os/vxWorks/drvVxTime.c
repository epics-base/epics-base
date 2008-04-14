/* Sheng Peng @ SNS ORNL 07/2004 */
/* Version 1.1 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <vxWorks.h>
#include <bootLib.h>
#include <tickLib.h>
#include <sysLib.h>
#include <logLib.h>
#include <sntpcLib.h>
#include <time.h>
#include <errno.h>
#include <envLib.h>
#include <taskLib.h>

#include "epicsTypes.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsThread.h"
#include "osdThread.h"
#include "epicsMutex.h"
#include "epicsTime.h"
#include "epicsTimer.h"
#include "epicsInterrupt.h"
#include "envDefs.h"

#include <dbDefs.h>
#include <devLib.h>
#include <module_types.h>

#define BILLION 1000000000
#define VXWORKS_TO_EPICS_EPOCH 631152000UL

#define VXTIME_DRV_VERSION "vxWorks Ticks Time Driver Version 1.2"
#define SYNC_PERIOD 60.0

#include "epicsGeneralTime.h"

static void VxTimeSyncTime(void *param);
static int  VxTimeGetCurrent(epicsTimeStamp *pDest);
static void VxTime_StartSync(int junk);

long    VxTime_Report(int level);

typedef struct VxTimePvt {
    int		synced;	/* if never synced, we can't use it */
    epicsMutexId	lock;
    epicsTimerId	sync_timer;
    epicsTimeStamp	lastReportedTS;
    epicsTimeStamp      lastSyncedTS;
    struct timespec	lastSyncedVxTime;
    int			priority;
    int			synced_priority;
}VxTimePvt;

static VxTimePvt *pVxTimePvt = 0;

static void VxTimeSyncTime(void *param)
{
    epicsTimeStamp	now;
    struct timespec	ticks_now;
    
    /* Ask for the best time available, not including ourselves */
    if (generalTimeGetExceptPriority(&now, &pVxTimePvt->synced_priority, pVxTimePvt->priority) == epicsTimeOK) {
    	/* It's a good time, we unconditionally sync ourselves to it, as
    	   this driver is the bottom of the heap */
    	clock_gettime( CLOCK_REALTIME,&ticks_now );

    	epicsMutexMustLock(pVxTimePvt->lock);
    	pVxTimePvt->lastSyncedTS = now;
    	pVxTimePvt->lastSyncedVxTime = ticks_now;
    	pVxTimePvt->synced = TRUE;
    	epicsMutexUnlock(pVxTimePvt->lock);
    }
    /* Restart the timer, to call us again in SYNC_PERIOD seconds */
    epicsTimerStartDelay(pVxTimePvt->sync_timer, SYNC_PERIOD);
}

long VxTime_InitOnce(int priority)
{
    pVxTimePvt = callocMustSucceed(1,sizeof(VxTimePvt),"VxTime_Init");
    memset(pVxTimePvt, 0, sizeof(VxTimePvt));
    pVxTimePvt->synced = FALSE;
    pVxTimePvt->lock = epicsMutexCreate();
    pVxTimePvt->priority = priority;
    
    /* register to link list */
    generalTimeCurrentTpRegister("vxWorks Ticks", priority, VxTimeGetCurrent);

    /* Don't start the syncing until the time system is up properly.
     * This cannot be done using EPICS initHooks, as they are external
     * to libCom, but this code is vxWorks-specific anyway, so use the
     * vxWorks primitives. wdLib (watchdogs cannot be used as the called
     * routines cannot take semaphores. */
    taskSpawn("VxTime Start Sync", 150, VX_FP_TASK,
    		epicsThreadGetStackSize(epicsThreadStackMedium),
    		(FUNCPTR) VxTime_StartSync, 0,0,0,0,0,0,0,0,0,0);

    return	0;
}

struct InitInfo {
    int  priority;
    long retval;
};
static void VxTime_InitOnceWrapper(void *arg)
{
    struct InitInfo *pargs = (struct InitInfo *)arg;
    pargs->retval = VxTime_InitOnce(pargs->priority);
}

long VxTime_Init(int priority)
{
    struct InitInfo args;
    static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;

    args.priority = priority;
    epicsThreadOnce(&onceId, VxTime_InitOnceWrapper, &args);
    return args.retval;
}

static int VxTimeGetCurrent(epicsTimeStamp *pDest)
{
    struct timespec     cur_vwtime;
    unsigned long diff_sec, diff_nsec;
    epicsTimeStamp	epicsTime;

    double      diffTime;

    epicsMutexMustLock(pVxTimePvt->lock);

    clock_gettime( CLOCK_REALTIME,&cur_vwtime );

    if(!pVxTimePvt->synced)
    {
	    if( cur_vwtime.tv_sec < VXWORKS_TO_EPICS_EPOCH )
	    { /* Earlier than 1990 is clearly wrong. Set the time to 1/1/90
		 + 1 day (to avoid timezone problems) and set the VxWorks
		 clock to that point */
		    cur_vwtime.tv_sec = VXWORKS_TO_EPICS_EPOCH + 86400;
		    cur_vwtime.tv_nsec = 0;
		    clock_settime( CLOCK_REALTIME, &cur_vwtime);
		    epicsInterruptContextMessage("****************************"
		        "***************************************************\n"
			"WARNING: VxWorks time not set. Initialised to 2/1/90\n"
			"*****************************************************"
			"**************************\n");
	    }
	    epicsTimeFromTimespec(&epicsTime, &cur_vwtime);	  
    }
    else/* synced, need calculation */
    {/* the vxWorks time is always monotonic */
	    if( cur_vwtime.tv_nsec >= pVxTimePvt->lastSyncedVxTime.tv_nsec )
	    {
		    diff_sec = cur_vwtime.tv_sec - pVxTimePvt->lastSyncedVxTime.tv_sec;
		    diff_nsec = cur_vwtime.tv_nsec - pVxTimePvt->lastSyncedVxTime.tv_nsec;
	    }
	    else
	    {
		    diff_sec = cur_vwtime.tv_sec - pVxTimePvt->lastSyncedVxTime.tv_sec - 1;
		    diff_nsec = BILLION - pVxTimePvt->lastSyncedVxTime.tv_nsec + cur_vwtime.tv_nsec;
	    }

	    epicsTime.nsec = pVxTimePvt->lastSyncedTS.nsec + diff_nsec;
	    epicsTime.secPastEpoch = pVxTimePvt->lastSyncedTS.secPastEpoch + diff_sec;
	    if( epicsTime.nsec >= BILLION )
	    {
		    epicsTime.nsec -= BILLION;
		    epicsTime.secPastEpoch ++;
	    }
    }

    diffTime = epicsTimeDiffInSeconds(&epicsTime,&pVxTimePvt->lastReportedTS);
    if(diffTime >= 0.0)
    {/* time is monotonic */
	    *pDest = epicsTime;
	    pVxTimePvt->lastReportedTS = epicsTime;
    }
    else
    {/* time never goes back */
	    *pDest = pVxTimePvt->lastReportedTS;
    }

    epicsMutexUnlock(pVxTimePvt->lock);
    return(0);
}

long    VxTime_Report(int level)
{
	printf(VXTIME_DRV_VERSION"\n");

	if(!pVxTimePvt)
	{/* drvVxTime is not used, we just report version then quit */
		printf("vxWorks ticks time driver is not initialized yet!\n\n");
	}
	else
	{
		printf("%synced. Last time = %lds, %ldns. ",
		       pVxTimePvt->synced ?"S":"Not s",
		       pVxTimePvt->lastSyncedVxTime.tv_sec,
		       pVxTimePvt->lastSyncedVxTime.tv_nsec);
		printf("Timer = %p\n", pVxTimePvt->sync_timer);
	}
	return  0;
}

static void VxTime_StartSync(int junk)
{
	/* Wait some time before trying to start the timer up */
	taskDelay(10 * sysClkRateGet());
	pVxTimePvt->sync_timer = generalTimeCreateSyncTimer(VxTimeSyncTime, 0);
	/* Sync the first time in one second, then drop back to less frequently */
	epicsTimerStartDelay(pVxTimePvt->sync_timer, 1.0);
}
