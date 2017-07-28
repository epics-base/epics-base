/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* os/vxWorks/osdMutex.h */

/* Author:  Marty Kraimer Date:    25AUG99 */

#include <vxWorks.h>
#include <semLib.h>

/* If the macro is replaced by inline it is necessary to say
   static __inline__
   but then a warning message appears everywhere osdMutex.h is included
*/

#define epicsMutexOsdUnlock(ID) semGive((SEM_ID)(ID))

#define epicsMutexOsdLock(ID) \
(semTake((SEM_ID)(ID),WAIT_FOREVER)==OK ? epicsMutexLockOK : epicsMutexLockError)
