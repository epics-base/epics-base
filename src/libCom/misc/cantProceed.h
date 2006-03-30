/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef INCcantProceedh
#define INCcantProceedh

#include <stdlib.h>

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void cantProceed(const char *errorMessage, ...);
epicsShareFunc void * callocMustSucceed(size_t count, size_t size, const char *errorMessage);
epicsShareFunc void * mallocMustSucceed(size_t size, const char *errorMessage);

#ifdef __cplusplus
}
#endif

#endif /* cantProceedh */
