/* drvSup.h	Driver Support	*/
/* share/epicsH $Id$ */

/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
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
 * .01  05-21-92        rcz     changed drvetName to papDrvName
 * .02  05-18-92        rcz     New database access (removed extern)
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
