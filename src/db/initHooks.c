/* initHooks.c	ioc initialization hooks */ 
/* share/src/db $Id$ */
/*
 *      Author:		Marty Kraimer
 *      Date:		06-01-91
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
 * .02  09-10-92	rcz	changed return from void to long
 *
 */


#include	<vxWorks.h>
#include	<initHooks.h>

extern long setMasterTimeToSelf();

/*
 * INITHOOKS
 *
 * called by iocInit at various points during initialization
 *
 */


/* If this function (initHooks) is loaded, iocInit calls this function
 * at certain defined points during IOC initialization */
long initHooks(callNumber)
int	callNumber;
{
long status = 0;

	switch (callNumber) {
	case SETMASTERTIMETOSELF:
		status = setMasterTimeToSelf();
	    break;
	case DBUSEREXIT:
		/* place holder */
		status = 0; /* call user routine here */
	    break;
	default:
	    break;
	}
	return(status);
}
