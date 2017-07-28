/*************************************************************************\
* Copyright (c) 2012 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2012 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef epicsSpinh
#define epicsSpinh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct epicsSpin *epicsSpinId;

epicsShareFunc epicsSpinId epicsSpinCreate(void);
epicsShareFunc epicsSpinId epicsSpinMustCreate(void);
epicsShareFunc void epicsSpinDestroy(epicsSpinId);

epicsShareFunc void epicsSpinLock(epicsSpinId);
epicsShareFunc int epicsSpinTryLock(epicsSpinId);
epicsShareFunc void epicsSpinUnlock(epicsSpinId);

#ifdef __cplusplus
}
#endif

#endif /* epicsSpinh */
