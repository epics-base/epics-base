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
    initHookAfterInterruptAccept,
    initHookAfterInitialProcess,
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
