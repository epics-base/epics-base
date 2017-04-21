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
 * Very thin shims around vxWorks routines
 */
#include <msgQLib.h>
#include <limits.h>

#define epicsMessageQueueCreate(c,s) ((epicsMessageQueueId)msgQCreate((c),(s),MSG_Q_FIFO))
#define epicsMessageQueueDestroy(q) (msgQDelete((MSG_Q_ID)(q)))

#define epicsMessageQueueTrySend(q,m,l) (msgQSend((MSG_Q_ID)(q), (char*)(m), (l), NO_WAIT, MSG_PRI_NORMAL))
#define epicsMessageQueueSend(q,m,l) (msgQSend((MSG_Q_ID)(q), (char*)(m), (l), WAIT_FOREVER, MSG_PRI_NORMAL))

#define epicsMessageQueueTryReceive(q,m,s) (msgQReceive((MSG_Q_ID)(q), (char*)(m), (s), NO_WAIT))
#define epicsMessageQueueReceive(q,m,s) (msgQReceive((MSG_Q_ID)(q), (char*)(m), (s), WAIT_FOREVER))

#define epicsMessageQueuePending(q) (msgQNumMsgs((MSG_Q_ID)(q)))
#define epicsMessageQueueShow(q,l) (msgQShow((MSG_Q_ID)(q),(l)))
