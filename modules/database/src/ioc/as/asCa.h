/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* asCa.h */

#ifndef INCasCah
#define INCasCah

#include <stdio.h>

#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

DBCORE_API void asCaStart(void);
DBCORE_API void asCaStop(void);
DBCORE_API int ascar(int level);
DBCORE_API int ascarFP(FILE *fp, int level);
DBCORE_API void ascaStats(int *pchans, int *pdiscon);

#ifdef __cplusplus
}
#endif

#endif /*INCasCah*/
