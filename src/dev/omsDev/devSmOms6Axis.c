/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devSmOms6Axis.c */
/* base/src/dev $Id$ */

/* devSmOms6Axis.c - Device Support Routines */
/*
 *      Original Author: Bob Dalesio
 *      Current Author: Marty Kraimer
 *      Date:  3/6/91
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
