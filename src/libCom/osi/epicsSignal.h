

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
 * The requests in this interface are typically ignored outside of a POSIX
 * contex.
 */

struct epicsThreadOSD;

epicsShareFunc void epicsShareAPI epicsSignalInstallSigPipeIgnore ( void );
epicsShareFunc void epicsShareAPI epicsSignalInstallSigUrgIgnore ( void );
epicsShareFunc void epicsShareAPI epicsSignalRaiseSigUrg ( struct epicsThreadOSD * );

#ifdef __cplusplus
}
#endif

