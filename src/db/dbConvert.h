/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbConvert.h */

#ifndef INCdbConverth
#define INCdbConverth

#include "dbFldTypes.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareExtern long (*dbGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1])
    (DBADDR *paddr, void *pbuffer,long nRequest, long no_elements, long offset);
epicsShareExtern long (*dbPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1])
    (DBADDR *paddr, const void *pbuffer,long nRequest, long no_elements,
    long offset);

#ifdef __cplusplus
}
#endif

#endif /*INCdbConverth*/
