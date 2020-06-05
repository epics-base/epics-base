/*************************************************************************\
* Copyright (c) 2012 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2012 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef epicsSpinh
#define epicsSpinh

#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct epicsSpin *epicsSpinId;

LIBCOM_API epicsSpinId epicsSpinCreate(void);
LIBCOM_API epicsSpinId epicsSpinMustCreate(void);
LIBCOM_API void epicsSpinDestroy(epicsSpinId);

LIBCOM_API void epicsSpinLock(epicsSpinId);
LIBCOM_API int epicsSpinTryLock(epicsSpinId);
LIBCOM_API void epicsSpinUnlock(epicsSpinId);

#ifdef __cplusplus
}
#endif

#endif /* epicsSpinh */
