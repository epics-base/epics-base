/* os/vxWorks/osdMutex.c */

/* Author:  Marty Kraimer Date:    25AUG99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <vxWorks.h>
#include <semLib.h>
#include <time.h>
#include <objLib.h>
#include <sysLib.h>
/* The following not defined in an vxWorks header */
int sysClkRateGet(void);


#include "epicsMutex.h"

epicsMutexId epicsMutexOsdCreate(void)
{
    return((epicsMutexId)
        semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY));
}

void epicsMutexOsdDestroy(epicsMutexId id)
{
    semDelete((SEM_ID)id);
}

epicsMutexLockStatus epicsMutexLockWithTimeout(
    epicsMutexId id, double timeOut)
{
    int status;
    int ticks;
    ticks = (int)(timeOut * (double)sysClkRateGet());
    if(ticks<=0) ticks = 1;
    status = semTake((SEM_ID)id,ticks);
    if(status==OK) return(epicsMutexLockOK);
    if(errno==S_objLib_OBJ_TIMEOUT) return(epicsMutexLockTimeout);
    return(epicsMutexLockError);
}

epicsMutexLockStatus epicsMutexTryLock(epicsMutexId id)
{
    int status;
    status = semTake((SEM_ID)id,NO_WAIT);
    if(status==OK) return(epicsMutexLockOK);
    if(errno==S_objLib_OBJ_UNAVAILABLE) return(epicsMutexLockTimeout);
    return(epicsMutexLockError);
}

void epicsMutexShow(epicsMutexId id,unsigned int level)
{
    semShow((SEM_ID)id,level);
}
