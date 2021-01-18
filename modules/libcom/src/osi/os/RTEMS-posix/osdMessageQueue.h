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
#include <mqueue.h>

struct epicsMessageQueueOSD {
    mqd_t   id;
    char    name[24];
};
#define epicsMessageQueueSend(q,m,l) (mq_send((q)->id, (const char*)(m), (l), 0))
#define epicsMessageQueueReceive(q,m,s) (mq_receive((q)->id, (char*)(m), (s), NULL))

