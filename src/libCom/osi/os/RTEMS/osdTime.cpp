/*
 * $Id$
 *
 * Author: W. Eric Norum
 */
//

/*
 * ANSI C
 */
#include <time.h>
#include <limits.h>

/*
 * RTEMS
 */
#include <rtems.h>
#include <rtems/rtems_bsdnet.h>

/*
 * EPICS
 */
#define epicsExportSharedSymbols
#include "osiTime.h"
#include "errlog.h"

extern "C" {
/*
 * RTEMS clock rate
 */
rtems_interval rtemsTicksPerSecond;
double rtemsTicksPerSecond_double;

/*
 * RTEMS time begins January 1, 1988 (local time).
 * EPICS time begins January 1, 1990 (GMT).
 *
 * FIXME: How to handle daylight-savings time?
 */
#define EPICS_EPOCH_SEC_PAST_RTEMS_EPOCH ((366+365)*86400)
static unsigned long timeoffset;

/*
 * Get current time
 * Allow for this to be called before clockInit() and before
 * system time and date have been initialized.
 */
int tsStampGetCurrent (TS_STAMP *pDest)
{
    struct timeval curtime;
    rtems_interval t;
    rtems_status_code sc;

    for (;;) {
	sc = rtems_clock_get (RTEMS_CLOCK_GET_TIME_VALUE, &curtime);
	if (sc == RTEMS_SUCCESSFUL)
	    break;
	else if (sc != RTEMS_NOT_DEFINED)
	    return tsStampERROR;
	sc = rtems_clock_get (RTEMS_CLOCK_GET_TICKS_PER_SECOND, &t);
	if (sc != RTEMS_SUCCESSFUL)
	    return tsStampERROR;
	rtems_task_wake_after (t);
    }
    pDest->nsec = curtime.tv_usec * 1000;
    pDest->secPastEpoch = curtime.tv_sec - timeoffset;
   return tsStampOK;
}
	
/*
 * tsStampGetEvent ()
 */
int tsStampGetEvent (TS_STAMP *pDest, unsigned eventNumber)
{
    if (eventNumber==tsStampEventCurrentTime) {
        return tsStampGetCurrent (pDest);
    }
    else {
        return tsStampERROR;
    }
}

void clockInit(void)
{
	timeoffset = EPICS_EPOCH_SEC_PAST_RTEMS_EPOCH - rtems_bsdnet_timeoffset;
	rtems_clock_get (RTEMS_CLOCK_GET_TICKS_PER_SECOND, &rtemsTicksPerSecond);
	rtemsTicksPerSecond_double = rtemsTicksPerSecond;
}
}
