/* devSmOms6Axis.c */
/* base/src/dev $Id$ */

/* devSmOms6Axis.c - Device Support Routines */
/*
 *      Original Author: Bob Dalesio
 *      Current Author: Marty Kraimer
 *      Date:  3/6/91
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  04-08092	mrk	Moved from record support
 */


#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include        <steppermotorRecord.h>
#include        <steppermotor.h>

/* Create the dset for  */
static long sm_command();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	sm_command;
}devSmOms6Axis={
	6,
	NULL,
	NULL,
	NULL,
	NULL,
	sm_command};

static long sm_command(psm,command,arg1,arg2)
    struct steppermotorRecord   *psm;
    short command;
    int arg1;
    int arg2;
{
	short	card,channel;

	card = psm->out.value.vmeio.card;
	channel = psm->out.value.vmeio.signal;
        oms_driver(card,channel,command,arg1,arg2);
	return(0);
}
