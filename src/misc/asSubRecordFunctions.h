/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* asSubRecordFunctions.h */

/* Author:  Marty Kraimer Date:    01MAY2000 */

#ifndef INCasSubRecordFunctionsh
#define INCasSubRecordFunctionsh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct subRecord;

epicsShareFunc long epicsShareAPI asSubInit(struct subRecord *precord,int pass);
epicsShareFunc long epicsShareAPI asSubProcess(struct subRecord *precord);
epicsShareFunc void epicsShareAPI asSubRecordFunctionsRegister(void);

#ifdef __cplusplus
}
#endif

#endif /* INCasSubRecordFunctionsh */
