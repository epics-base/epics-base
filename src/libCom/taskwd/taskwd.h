/* taskwd.h */
/* share/epicsH/taskwd.h  $Id$ */

/* includes for general purpose taskwd tasks		*/
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01	12-12-91	mrk	Initial version
*/

#ifndef INCtaskwdh
#define INCtaskwdh 1

#include "osiThread.h"
#include "shareLib.h"

typedef void (*TASKWDFUNCPRR)(void *parm);
typedef void (*TASKWDANYFUNCPRR)(void *parm,threadId tid);
#if defined(__STDC__) || defined(__cplusplus)
#ifdef __cplusplus
extern "C" {
#endif
epicsShareFunc void taskwdInit(void);
epicsShareFunc void taskwdInsert(threadId tid, TASKWDFUNCPRR callback,void *arg);
epicsShareFunc void taskwdAnyInsert(void *userpvt, TASKWDANYFUNCPRR callback,void *arg);
epicsShareFunc void taskwdRemove(threadId tid);
epicsShareFunc void taskwdAnyRemove(void *userpvt);
#ifdef __cplusplus
}
#endif
#else
epicsShareFunc void taskwdInit();
epicsShareFunc void taskwdInsert();
epicsShareFunc void taskwdAnyInsert();
epicsShareFunc void taskwdRemove();
epicsShareFunc void taskwdAnyRemove();
#endif /*__STDC__*/
#endif /*INCtaskwdh*/
