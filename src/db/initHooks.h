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
 *
 */


#ifndef INCinitHooksh
#define INCinitHooksh 1

typedef enum {
    initHookAtBeginning,
    initHookAfterGetResources,
    initHookAfterLogInit,
    initHookAfterCallbackInit,
    initHookAfterCaLinkInit,
    initHookAfterInitDrvSup,
    initHookAfterInitRecSup,
    initHookAfterInitDevSup,
    initHookAfterTS_init,
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

#ifdef __STDC__
typedef void (*initHookFunction)(initHookState state);
int initHookRegister(initHookFunction func);
void initHooks(initHookState state);
#else /*__STDC__*/
typedef void (*initHookFunction)();
int initHookRegister();
void initHooks();
#endif /*__STDC__*/

#ifdef __cplusplus
}
#endif

/*FOLLOWING IS OBSOLETE*/
/*The following are for compatibility with old initHooks.c functions*/
#define INITHOOKatBeginning		initHookAtBeginning
#define INITHOOKafterGetResources	initHookAfterGetResources
#define INITHOOKafterLogInit		initHookAfterLogInit
#define INITHOOKafterCallbackInit	initHookAfterCallbackInit
#define INITHOOKafterCaLinkInit		initHookAfterCaLinkInit
#define INITHOOKafterInitDrvSup		initHookAfterInitDrvSup
#define INITHOOKafterInitRecSup		initHookAfterInitRecSup
#define INITHOOKafterInitDevSup		initHookAfterInitDevSup
#define INITHOOKafterTS_init		initHookAfterTS_init
#define INITHOOKafterInitDatabase	initHookAfterInitDatabase
#define INITHOOKafterFinishDevSup	initHookAfterFinishDevSup
#define INITHOOKafterScanInit		initHookAfterScanInit
#define INITHOOKafterInterruptAccept	initHookAfterInterruptAccept
#define INITHOOKafterInitialProcess	initHookAfterInitialProcess
#define INITHOOKatEnd			initHookAtEnd
/*END OF OBSOLETE DEFINITIONS*/

#endif /*INCinitHooksh*/
