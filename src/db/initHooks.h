/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* src/db/initHooks.h */
/*
 *      Authors:        Benjamin Franksen (BESY) and Marty Kraimer
 *      Date:           06-01-91
 *      major Revision: 07JuL97
 */

#ifndef INC_initHooks_H
#define INC_initHooks_H

#include "shareLib.h"

/* This enum must agree with the array of names defined in initHookName() */
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
epicsShareFunc int initHookRegister(initHookFunction func);
epicsShareFunc void initHooks(initHookState state);
epicsShareFunc const char *initHookName(initHookState state);
#ifdef __cplusplus
}
#endif

#endif /* INC_initHooks_H */
