/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devPtSoft.c */
/* base/src/dev $Id$ */
/*
 *      Author:		Janet Anderson
 *      Date:		6-1-91
 */
#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<module_types.h>
#include	<pulseTrainRecord.h>

/* Create the dset for devPtSoft */
static long init_record();
static long write_pt();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_pt;
}devPtSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_pt
};

static long init_record(ppt)
struct pulseTrainRecord *ppt;
{
    return 0;
} /* end init_record() */

static long write_pt(ppt)
    struct pulseTrainRecord	*ppt;
{
    long status;

    status = dbPutLink(&(ppt->out),DBR_SHORT,&(ppt->val),1);
    return(0);
}
