
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
