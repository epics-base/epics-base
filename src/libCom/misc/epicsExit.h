/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsExit.h*/
#ifndef epicsExith
#define epicsExith
#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void epicsExit(int status);
epicsShareFunc void epicsExitCallAtExits(void);
epicsShareFunc int epicsAtExit(void (*epicsExitFunc)(void *arg),void *arg);

epicsShareFunc void epicsExitCallAtThreadExits(void);
epicsShareFunc int epicsAtThreadExit(
                 void (*epicsExitFunc)(void *arg),void *arg);


#ifdef __cplusplus
}
#endif

#endif /*epicsExith*/
