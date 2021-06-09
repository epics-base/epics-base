/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbConvert.h */

#ifndef INCdbConverth
#define INCdbConverth

#include "dbFldTypes.h"
#include "dbAddr.h"
#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef long (*GETCONVERTFUNC)(const DBADDR *paddr, void *pbuffer,
    long nRequest, long no_elements, long offset);
typedef long (*PUTCONVERTFUNC)(DBADDR *paddr, const void *pbuffer,
    long nRequest, long no_elements, long offset);

DBCORE_API extern GETCONVERTFUNC dbGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1];
DBCORE_API extern PUTCONVERTFUNC dbPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1];

#ifdef __cplusplus
}
#endif

#endif /*INCdbConverth*/
