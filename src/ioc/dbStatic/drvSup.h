/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* drvSup.h	Driver Support	*/

/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */

#ifndef INCdrvSuph
#define INCdrvSuph 1

#include "errMdef.h"

typedef long (*DRVSUPFUN) ();	/* ptr to driver support function*/

typedef struct drvet {	/* driver entry table */
	long		number;		/*number of support routines*/
	DRVSUPFUN	report;		/*print report*/
	DRVSUPFUN	init;		/*init support*/
	/*other functions are device dependent*/
}drvet;
#define DRVETNUMBER ( (sizeof(struct drvet) -sizeof(long))/sizeof(DRVSUPFUN) )

typedef struct typed_drvet {
    /** Number of function pointers which follow.  Must be >=2 */
    long number;
    /** Called from dbior() */
    long (*report)(int lvl);
    /** Called during iocInit() */
#ifdef __cplusplus
    long (*init)();
#else
    long (*init)(void);
#endif
    /*other functions are device dependent*/
} typed_drvet;

#define S_drv_noDrvSup   (M_drvSup| 1) /*SDR_DRVSUP: Driver support missing*/
#define S_drv_noDrvet    (M_drvSup| 3) /*Missing driver support entry table*/

#endif
