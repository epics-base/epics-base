/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devBoSoftRaw.c */
/* base/src/dev $Id$ */

/* devBoSoftRaw.c - Device Support Routines for  SoftRaw Binary Output*/
/*
 *      Author:		Janet Anderson
 *      Date:		3-28-92
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
#include	<boRecord.h>

/* added for Channel Access Links */
static long init_record();

/* Create the dset for devBoSoftRaw */
static long write_bo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoSoftRaw={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_bo};



static long init_record(pbo)
struct boRecord *pbo;
{
    long status;
    
    /*Don't convert*/
    status = 2;
    return status;
 
} /* end init_record() */

static long write_bo(pbo)
    struct boRecord	*pbo;
{
    long status;

    status = dbPutLink(&pbo->out,DBR_LONG, &pbo->rval,1);

    return(status);
}
