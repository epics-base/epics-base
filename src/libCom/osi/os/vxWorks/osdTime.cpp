/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "epicsTime.h"

// epicsTimeGetCurrent and epicsTimeGetEvent are implemented in iocClock.c

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
