/* taskwd.h */
/* share/epicsH/taskwd.h  $Id$ */

/* includes for general purpose taskwd tasks		*/
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
*/

/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1991 Regents of the University of California,
and the University of Chicago Board of Governors.

This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
**********************************************************************/

#ifndef INCtaskwdh
#define INCtaskwdh 1

#include "osiThread.h"
#include "shareLib.h"

typedef void (*TASKWDFUNCPRR)(void *parm);
typedef void (*TASKWDANYFUNCPRR)(void *parm,threadId tid);

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void epicsShareAPI taskwdInit(void);
epicsShareFunc void epicsShareAPI taskwdInsert(
    threadId tid, TASKWDFUNCPRR callback,void *arg);
epicsShareFunc void epicsShareAPI taskwdAnyInsert(
    void *userpvt, TASKWDANYFUNCPRR callback,void *arg);
epicsShareFunc void epicsShareAPI taskwdRemove(threadId tid);
epicsShareFunc void epicsShareAPI taskwdAnyRemove(void *userpvt);

#ifdef __cplusplus
}
#endif

#endif /*INCtaskwdh*/
