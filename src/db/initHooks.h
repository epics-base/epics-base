/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* share/src/db/initHooks.h*/
/*
 *      Authors:	Benjamin Franksen (BESY) and Marty Kraimer
 *      Date:		06-01-91
 *      major Revision: 07JuL97
 */


#ifndef INCinitHooksh
#define INCinitHooksh 1

#include "shareLib.h"

typedef enum {
    initHookAtBeginning,
    initHookAfterCallbackInit,
    initHookAfterCaLinkInit,
    initHookAfterInitDrvSup,
    initHookAfterInitRecSup,
    initHookAfterInitDevSup,
    initHookAfterInitDatabase,
    initHookAfterFinishDevSup,
    initHookAfterScanInit,
    initHookAfterInitialProcess,
    initHookAfterInterruptAccept,
    initHookAtEnd
}initHookState;

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*initHookFunction)(initHookState state);
epicsShareFunc int epicsShareAPI initHookRegister(initHookFunction func);
epicsShareFunc void epicsShareAPI initHooks(initHookState state);

#ifdef __cplusplus
}
#endif

#endif /*INCinitHooksh*/
