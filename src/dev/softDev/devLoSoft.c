/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devLoSoft.c */
/* base/src/dev $Id$ */
/*
 *      Author:		Janet Anderson
 *      Date:   	09-23-91
*/
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	"alarm.h"
#include	"dbDefs.h"
#include	"dbAccess.h"
#include	"recGbl.h"
#include        "recSup.h"
#include	"devSup.h"
#include	"longoutRecord.h"
#define epicsExportSharedSymbols
#include "shareLib.h"

/* Create the dset for devLoSoft */
static long init_record();
static long write_longout();
static struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_longout;
}devLoSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_longout
};
epicsShareDef dset *pdevLoSoft = (dset *)&devLoSoft;

static long init_record(longoutRecord *plongout)
{
    return(0);
} /* end init_record() */

static long write_longout(longoutRecord	*plongout)
{
    long status;

    status = dbPutLink(&plongout->out,DBR_LONG, &plongout->val,1);
    return(0);
}
