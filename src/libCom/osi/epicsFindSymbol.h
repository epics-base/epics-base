/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef epicsFindSymbolh
#define epicsFindSymbolh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc void * epicsLoadLibrary(const char *name);
epicsShareFunc const char *epicsLoadError(void);
epicsShareFunc void * epicsShareAPI epicsFindSymbol(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* epicsFindSymbolh */
