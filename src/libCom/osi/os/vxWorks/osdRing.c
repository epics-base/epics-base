/* osiRing.c */

/* Author:  Marty Kraimer Date:    09SEP99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <vxWorks.h>
#include <rngLib.h>

ringId ringCreate(int nbytes)
{
    return((ringId)rngCreate(nbytes));
}

void ringDelete(ringId id)
{
    rngDelete((RING_ID)id);
}

int ringGet(ringId id, char *value,int nbytes)
{
    return(rngBufGet((RING_ID)id,value,nbytes);
}

int ringPut(ringId id, char *value,int nbytes)
{
    return(rngBufPut((RING_ID)id,value,nbytes));
}

void ringFlush(ringId id)
{
    ringFlush((RING_ID)id);
}

int ringFreeBytes(ringId id)
{
    return(rngFreeBytes((RING_ID)id));
}

int ringUsedBytes(ringId id)
{
    return(rngNBytes((RING_ID)id));
}

int ringSize(ringId id)
{
    return((rngFreeBytes((RING_ID)id) + rngNBytes((RING_ID)id)));
}

int ringIsEmpty(ringId id)
{
    return(rngIsEmpty((RING_ID)id));
}

int ringIsFull(ringId id)
{
    return(rngIsFull((RING_ID)id));
}
