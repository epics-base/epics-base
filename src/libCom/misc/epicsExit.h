/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*epicsExit.h*/
#ifndef epicsExith
#define epicsExith
#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*epicsExitFunc)(void *arg);

epicsShareFunc void epicsExit(int status);
epicsShareFunc void epicsExitCallAtExits(void);
epicsShareFunc int epicsAtExit(epicsExitFunc func, void *arg);

epicsShareFunc void epicsExitCallAtThreadExits(void);
epicsShareFunc int epicsAtThreadExit(epicsExitFunc func, void *arg);


#ifdef __cplusplus
}
#endif

#endif /*epicsExith*/
