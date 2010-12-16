/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef caDiagnosticsh
#define caDiagnosticsh

#include "cadef.h"

#ifdef __cplusplus
extern "C" {
#endif

enum appendNumberFlag {appendNumber, dontAppendNumber};
int catime ( const char *channelName, unsigned channelCount, enum appendNumberFlag appNF );

int acctst ( const char *pname, unsigned logggingInterestLevel, 
            unsigned channelCount, unsigned repetitionCount, 
            enum ca_preemptive_callback_select select );

#define CATIME_OK 0
#define CATIME_ERROR -1

#ifdef __cplusplus
}
#endif

void caConnTest ( const char *pNameIn, unsigned channelCountIn, double delayIn );

#endif /* caDiagnosticsh */


