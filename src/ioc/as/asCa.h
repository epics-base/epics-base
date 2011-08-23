/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* asCa.h */

#ifndef INCasCah
#define INCasCah

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void epicsShareAPI asCaStart(void);
epicsShareFunc void epicsShareAPI asCaStop(void);
epicsShareFunc int epicsShareAPI ascar(int level);
epicsShareFunc int epicsShareAPI ascarFP(FILE *fp, int level);
epicsShareFunc void epicsShareAPI ascaStats(int *pchans, int *pdiscon);

#ifdef __cplusplus
}
#endif

#endif /*INCasCah*/
