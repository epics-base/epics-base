/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * $Id$
 *
 * Author: W. Eric Norum
 */
//

#include <rtems.h>
#include "epicsTime.h"
#include "osiNTPTime.h"
#include "osdSysTime.h"
#include "generalTimeSup.h"

extern "C" {

int rtems_bsdnet_get_ntp(int, int(*)(), struct timespec *);


void osdTimeRegister(void)
{
    NTPTime_Init(100); /* init NTP first so it can be used to sync SysTime */
    SysTime_Init(LAST_RESORT_PRIORITY);
}

int osdNTPGet(struct timespec *ts)
{
    return rtems_bsdnet_get_ntp(-1, NULL, ts);
}

void osdNTPInit(void)
{
}

void osdNTPReport(void)
{
}


int
osdTickGet(void)
{
    rtems_interval t;
    rtems_clock_get (RTEMS_CLOCK_GET_TICKS_SINCE_BOOT, &t);
    return t;
}

int
osdTickRateGet(void)
{
    extern rtems_interval rtemsTicksPerSecond;

    return rtemsTicksPerSecond;
}

/*
 * Use reentrant versions of time access
 */
int epicsTime_gmtime ( const time_t *pAnsiTime, struct tm *pTM )
{
    struct tm * pRet = gmtime_r ( pAnsiTime, pTM );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return epicsTimeERROR;
    }
}

int epicsTime_localtime ( const time_t *clock, struct tm *result )
{
    struct tm * pRet = localtime_r ( clock, result );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return epicsTimeERROR;
    }
}

} // extern "C"

/*
 * Register local time providers if EPICS is running (i.e. if this
 * code has been dynamically loaded into a running system).
 */
class osdTimeReg {
  public:
    osdTimeReg() {
        extern rtems_interval rtemsTicksPerSecond;
        if (rtemsTicksPerSecond != 0) osdTimeRegister();
    }
};
static osdTimeReg osdTimeRegObj;
