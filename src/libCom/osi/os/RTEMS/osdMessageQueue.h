/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author  W. Eric Norum
 *              norume@aps.anl.gov
 *              630 252 4793
 */

/*
 * Very thin shims around RTEMS routines
 */
#include <rtems.h>

struct epicsMessageQueueOSD {
    rtems_id     id;
    unsigned int maxSize;
    void        *localBuf;

};
#define epicsMessageQueueDestroy(q) (rtems_message_queue_delete((q)->id))

#define epicsMessageQueueTrySend(q,m,l) (rtems_message_queue_send((q)->id, (m), (l)) == RTEMS_SUCCESSFUL ? 0 : -1)
