/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* drvSup.h	Driver Support	*/
/* share/epicsH $Id$ */

/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 *
 */

#ifndef INCdrvSuph
#define INCdrvSuph 1

typedef long (*DRVSUPFUN) ();	/* ptr to driver support function*/

struct drvet {	/* driver entry table */
	long		number;		/*number of support routines*/
	DRVSUPFUN	report;		/*print report*/
	DRVSUPFUN	init;		/*init support*/
	DRVSUPFUN	reboot;		/*reboot support*/
	/*other functions are device dependent*/
	};

struct drvSup {
	long		number;		/*number of dset		*/
	char		**papDrvName;	/*ptr to arr of ptr to drvetName*/
	struct drvet	**papDrvet;	/*ptr to arr ptr to drvet	*/
	};
#define DRVETNUMBER ( (sizeof(struct drvet) -sizeof(long))/sizeof(DRVSUPFUN) )

#define S_drv_noDrvSup   (M_drvSup| 1) /*SDR_DRVSUP: Driver support missing*/
#define S_drv_noDrvet    (M_drvSup| 3) /*Missing driver support entry table*/

/**************************************************************************
 *
 *The following are valid references
 *
 * pdrvSup->drvetName[i]  This is address of name string
 * pdrvSup->papDrvet[i]->report(<args>);
 *
 * The memory layout is
 *
 * pdrvSup->drvSup
 *            number
 *            papDrvName->[0]->"name"
 *            papDrvet-->[0]
 *                       [1]->drvet
 *                               number
 *                               report->report()
 *                                ...
 *************************************************************************/

/**************************************************************************
 * The following macro returns either the addr of drvet or NULL
 * A typical usage is:
 *
 *	struct drvet *pdrvet;
 *	if(!(pdrvet=GET_PDRVET(pdrvSup,type))) {<null}
 ***********************************************************************/
#define GET_PDRVET(PDRVSUP,TYPE)\
(\
    PDRVSUP\
    ?(\
	(((TYPE) < 1) || ((TYPE) >= PDRVSUP->number))\
	?   NULL\
	:   (PDRVSUP->papDrvet[(TYPE)])\
    )\
    : NULL\
)

/**************************************************************************
 * The following macro returns either the addr of drvName or NULL
 * A typical usage is:
 *
 *	char	*pdrvName;
 *	if(!(pdrvName=GET_PDRVNAME(pdrvSup,type))) {<null}
 ***********************************************************************/
#define GET_PDRVNAME(PDRVSUP,TYPE)\
(\
    PDRVSUP\
    ?(\
	(((TYPE) < 1) || ((TYPE) >= PDRVSUP->number))\
	?   NULL\
	:   (PDRVSUP->papDrvName[(TYPE)])\
    )\
    : NULL\
)
#endif
