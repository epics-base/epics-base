/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

/*
 * needs to be consolodated with other os independent stuff ...
 */
epicsShareFunc void epicsShareAPI osiSleep (unsigned sec, unsigned uSec);

#ifdef __cplusplus
}
#endif
