/* os/vxWorks/osdSem.h */

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

INLINE semBinaryId semBinaryCreate(int initialState)
{
  return((semBinaryId)semBCreate(SEM_Q_FIFO,(semEmpty ? SEM_EMPTY : SEM_FULL)));
}

INLINE void semBinaryDestroy(semBinaryId id)
{
    semDelete((SEM_ID)id);
}

INLINE void semBinaryGive(semBinaryId id)
{
    semGive((SEM_ID)id);
}

INLINE semTakeStatus semBinaryTake(semBinaryId id)
{
    int status;
    status = semTake((SEM_ID)id,WAIT_FOREVER);
    return((status==OK ? semTakeOK : semTakeError));
}

INLINE semTakeStatus semBinaryTakeTimeout(
    semBinaryId id, double timeOut)
{
    int status;
    int ticks;
    ticks = (int)(timeOut * (double)sysClkRateGet());
    status = semTake((SEM_ID)id,ticks);
    if(status==OK) return(semTakeOK);
    if(errno==S_objLib_OBJ_TIMEOUT) return(semTakeTimeout);
    return(semTakeError);
}

INLINE semTakeStatus semBinaryTakeNoWait(semBinaryId id)
{
    int status;
    status = semTake((SEM_ID)id,NO_WAIT);
    if(status==OK) return(semTakeOK);
    if(errno==S_objLib_OBJ_UNAVAILABLE) return(semTakeTimeout);
    return(semTakeError);
}

INLINE void semBinaryShow(semBinaryId id)
{
    semShow((SEM_ID)id,1);
}

INLINE semMutexId semMutexCreate(void)
{
    return((semMutexId)
        semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY));
}

INLINE void semMutexDestroy(semMutexId id)
{
    semDelete((SEM_ID)id);
}

INLINE void semMutexGive(semMutexId id)
{
    semGive((SEM_ID)id);
}

INLINE semTakeStatus semMutexTake(semMutexId id)
{
    int status;
    status = semTake((SEM_ID)id,WAIT_FOREVER);
    return((status==OK ? semTakeOK : semTakeError));
}

INLINE semTakeStatus semMutexTakeTimeout(
    semMutexId id, double timeOut)
{
    int status;
    status = semTake((SEM_ID)id,NO_WAIT);
    if(status==OK) return(semTakeOK);
    if(errno==S_objLib_OBJ_UNAVAILABLE) return(semTakeTimeout);
    return(semTakeError);
}

INLINE semTakeStatus semMutexTakeNoWait(semMutexId id);
{
    int status;
    status = semTake((SEM_ID)id,NO_WAIT);
    if(status==OK) return(semTakeOK);
    if(errno==S_objLib_OBJ_UNAVAILABLE) return(semTakeTimeout);
    return(semTakeError);
}

INLINE void semMutexShow(semMutexId id);
{
    semShow((SEM_ID)id,1);
}
