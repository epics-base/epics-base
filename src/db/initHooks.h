/* share/src/db/initHooks.h*/
/*
 *      Authors:	Benjamin Franksen (BESY) and Marty Kraimer
 *      Date:		06-01-91
 *      major Revision: 07JuL97
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  09-05-92	rcz	initial version
 * .02  09-10-92	rcz	changed completely
 * .03  07-15-97	mrk	Benjamin Franksen allow multiple functions
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
    initHookAfterInterruptAccept,
    initHookAfterInitialProcess,
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
