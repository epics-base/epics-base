/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* taskwd.h */
/* share/epicsH/taskwd.h  $Id$ */

/* includes for general purpose taskwd tasks		*/
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
*/


#ifndef INCtaskwdh
#define INCtaskwdh 1

#include "epicsThread.h"
#include "shareLib.h"

typedef void (*TASKWDFUNCPRR)(void *parm);
typedef void (*TASKWDANYFUNCPRR)(void *parm,epicsThreadId tid);

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void epicsShareAPI taskwdInit(void);
epicsShareFunc void epicsShareAPI taskwdInsert(
    epicsThreadId tid, TASKWDFUNCPRR callback,void *arg);
epicsShareFunc void epicsShareAPI taskwdAnyInsert(
    void *userpvt, TASKWDANYFUNCPRR callback,void *arg);
epicsShareFunc void epicsShareAPI taskwdRemove(epicsThreadId tid);
epicsShareFunc void epicsShareAPI taskwdAnyRemove(void *userpvt);

#ifdef __cplusplus
}
#endif

#endif /*INCtaskwdh*/
