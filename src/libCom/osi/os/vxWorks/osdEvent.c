/* os/vxWorks/osdEvent.c */

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

#include "epicsEvent.h"

epicsEventId epicsEventCreate(epicsEventInitialState initialState)
{
  return((epicsEventId)semBCreate(
    SEM_Q_FIFO,((initialState==epicsEventEmpty) ? SEM_EMPTY : SEM_FULL)));
}

epicsEventId epicsEventMustCreate(epicsEventInitialState initialState)
{
    epicsEventId id = epicsEventCreate (initialState);
    assert (id);
    return id;
}

void epicsEventDestroy(epicsEventId id)
{
    semDelete((SEM_ID)id);
}

epicsEventWaitStatus epicsEventWaitWithTimeout(
    epicsEventId id, double timeOut)
{
    int status;
    int ticks;
    ticks = (int)(timeOut * (double)sysClkRateGet());
    if(ticks<=0) ticks = 1;
    status = semTake((SEM_ID)id,ticks);
    if(status==OK) return(epicsEventWaitOK);
    if(errno==S_objLib_OBJ_TIMEOUT) return(epicsEventWaitTimeout);
    return(epicsEventWaitError);
}

epicsEventWaitStatus epicsEventTryWait(epicsEventId id)
{
    int status;
    status = semTake((SEM_ID)id,NO_WAIT);
    if(status==OK) return(epicsEventWaitOK);
    if(errno==S_objLib_OBJ_UNAVAILABLE) return(epicsEventWaitTimeout);
    return(epicsEventWaitError);
}

void epicsEventShow(epicsEventId id,unsigned int level)
{
    semShow((SEM_ID)id,level);
}
