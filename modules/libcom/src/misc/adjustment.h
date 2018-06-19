/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* src/libCom/adjustment.h */

#ifndef INCadjustmenth
#define INCadjustmenth
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc size_t adjustToWorstCaseAlignment(size_t size);

#ifdef __cplusplus
}
#endif


#endif /*INCadjustmenth*/

