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
 *      $Id$
 *
 *      Author  W. Eric Norum
 *              norume@aps.anl.gov
 *              630 252 4793
 */

/*
 * Very thin shims around vxWorks routines
 */
#include <msgQLib.h>
#include <limits.h>

struct epicsMessageQueueOSD {
    MSG_Q_ID     msgq;
    unsigned int nBytes;
};

#define epicsMessageQueueTrySend(q,m,l) (msgQSend((q)->msgq, (char*)(m), (l), NO_WAIT, MSG_PRI_NORMAL))
#define epicsMessageQueueSend(q,m,l) (msgQSend((q)->msgq, (char*)(m), (l), WAIT_FOREVER, MSG_PRI_NORMAL))

#define epicsMessageQueueTryReceive(q,m) (msgQReceive((q)->msgq, (char*)(m), (q)->nBytes, NO_WAIT))
#define epicsMessageQueueReceive(q,m) (msgQReceive((q)->msgq, (char*)(m), (q)->nBytes, WAIT_FOREVER))

#define epicsMessageQueuePending(q) (msgQNumMsgs((q)->msgq))
#define epicsMessageQueueShow(q,l) (msgQShow((q)->msgq,(l)))
