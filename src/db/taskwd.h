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
 *
*/

#ifndef INCtaskwdh
#define INCtaskwdh 1

#ifdef __STDC__
void taskwdInit();
void taskwdInsert(int tid, VOIDFUNCPTR callback,void *arg);
void taskwdAnyInsert(void *userpvt, VOIDFUNCPTR callback,void *arg);
void taskwdRemove(int tid);
void taskwdAnyRemove(void *userpvt);
#else
void taskwdInit();
void taskwdInsert();
void taskwdAnyInsert();
void taskwdRemove();
void taskwdAnyRemove();
#endif /*__STDC__*/
#endif /*INCtaskwdh*/
