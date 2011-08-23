/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* os/vxWorks/osdEvent.h */

/* Author:  Marty Kraimer Date:    25AUG99 */

#include <vxWorks.h>
#include <semLib.h>

#define epicsEventTrigger(ID) \
    (semGive((SEM_ID)(ID)) == OK ? epicsEventOK : epicsEventError)

#define epicsEventWait(ID) \
    (semTake((SEM_ID)(ID), WAIT_FOREVER) == OK ? epicsEventOK : epicsEventError)
