/* os/vxWorks/osdSem.c */

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

#include "osiSem.h"

semBinaryId semBinaryCreate(semInitialState initialState)
{
  return((semBinaryId)semBCreate(SEM_Q_FIFO,(semEmpty ? SEM_EMPTY : SEM_FULL)));
}

semBinaryId semBinaryMustCreate(semInitialState initialState)
{
    semBinaryId id = semBinaryCreate (initialState);
    assert (id);
    return id;
}

void semBinaryDestroy(semBinaryId id)
{
    semDelete((SEM_ID)id);
}

void semBinaryGive(semBinaryId id)
{
    semGive((SEM_ID)id);
}

semTakeStatus semBinaryTake(semBinaryId id)
{
    int status;
    status = semTake((SEM_ID)id,WAIT_FOREVER);
    return((status==OK ? semTakeOK : semTakeError));
}

semTakeStatus semBinaryTakeTimeout(
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

semTakeStatus semBinaryTakeNoWait(semBinaryId id)
{
    int status;
    status = semTake((SEM_ID)id,NO_WAIT);
    if(status==OK) return(semTakeOK);
    if(errno==S_objLib_OBJ_UNAVAILABLE) return(semTakeTimeout);
    return(semTakeError);
}

void semBinaryShow(semBinaryId id,unsigned int level)
{
    semShow((SEM_ID)id,level);
}

semMutexId semMutexCreate(void)
{
    return((semMutexId)
        semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY));
}

semMutexId semMutexMustCreate(void)
{
    semMutexId id = semMutexCreate ();
    assert (id);
    return id;
}

void semMutexDestroy(semMutexId id)
{
    semDelete((SEM_ID)id);
}

void semMutexGive(semMutexId id)
{
    semGive((SEM_ID)id);
}

semTakeStatus semMutexTake(semMutexId id)
{
    int status;
    status = semTake((SEM_ID)id,WAIT_FOREVER);
    return((status==OK ? semTakeOK : semTakeError));
}

semTakeStatus semMutexTakeTimeout(
    semMutexId id, double timeOut)
{
    int status;
    status = semTake((SEM_ID)id,NO_WAIT);
    if(status==OK) return(semTakeOK);
    if(errno==S_objLib_OBJ_UNAVAILABLE) return(semTakeTimeout);
    return(semTakeError);
}

semTakeStatus semMutexTakeNoWait(semMutexId id)
{
    int status;
    status = semTake((SEM_ID)id,NO_WAIT);
    if(status==OK) return(semTakeOK);
    if(errno==S_objLib_OBJ_UNAVAILABLE) return(semTakeTimeout);
    return(semTakeError);
}

void semMutexShow(semMutexId id,unsigned int level)
{
    semShow((SEM_ID)id,level);
}
