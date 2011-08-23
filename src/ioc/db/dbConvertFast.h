/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbConvertFast.h */

#ifndef INCdbConvertFasth
#define INCdbConvertFasth

#include "dbFldTypes.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareExtern long (*dbFastGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1])();
epicsShareExtern long (*dbFastPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1])();

#ifdef __cplusplus
}
#endif

#endif /*INCdbConvertFasth*/
