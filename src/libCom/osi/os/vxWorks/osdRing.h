/* osdRing.h */

/* Author:  Marty Kraimer Date:    09SEP99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <vxWorks.h>
#include <rngLib.h>

epicsShareFunc INLINE ringId ringCreate(int nbytes)
{
    return((ringId)rngCreate(nbytes));
}

epicsShareFunc INLINE void ringDelete(ringId id)
{
    rngDelete((RING_ID)id);
}

epicsShareFunc INLINE int ringGet(ringId id, char *value,int nbytes)
{
    return(rngBufGet((RING_ID)id,value,nbytes);
}

epicsShareFunc INLINE int ringPut(ringId id, char *value,int nbytes)
{
    return(rngBufPut((RING_ID)id,value,nbytes));
}

epicsShareFunc INLINE void ringFlush(ringId id)
{
    ringFlush((RING_ID)id);
}

epicsShareFunc INLINE int ringFreeBytes(ringId id)
{
    return(rngFreeBytes((RING_ID)id));
}

epicsShareFunc INLINE int ringUsedBytes(ringId id)
{
    return(rngNBytes((RING_ID)id));
}

epicsShareFunc INLINE int ringSize(ringId id)
{
    return((rngFreeBytes((RING_ID)id) + rngNBytes((RING_ID)id)));
}

epicsShareFunc INLINE int ringIsEmpty(ringId id)
{
    return(rngIsEmpty((RING_ID)id));
}

epicsShareFunc INLINE int ringIsFull(ringId id)
{
    return(rngIsFull((RING_ID)id));
}
