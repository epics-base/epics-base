/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef epicsInterrupth
#define epicsInterrupth

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc int epicsInterruptLock(void);
epicsShareFunc void epicsInterruptUnlock(int key);
epicsShareFunc int epicsInterruptIsInterruptContext(void);
epicsShareFunc void epicsInterruptContextMessage(const char *message);

#ifdef __cplusplus
}
#endif

#include "osdInterrupt.h"

#endif /* epicsInterrupth */
