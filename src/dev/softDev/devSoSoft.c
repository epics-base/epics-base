/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devSoSoft.c */
/* base/src/dev $Id$ */
/*
 *      Author:		Janet Anderson
 *      Date:   	04-21-91
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
#include	"stringoutRecord.h"
#define epicsExportSharedSymbols
#include "shareLib.h"

/* Create the dset for devSoSoft */
static long init_record();
static long write_stringout();
static struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_stringout;
}devSoSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_stringout
};
epicsShareExtern dset *pdevSoSoft;
epicsShareDef dset *pdevSoSoft = (dset *)&devSoSoft;

static long init_record(stringoutRecord *pstringout)
{
    return 0;
} /* end init_record() */

static long write_stringout(pstringout)
    struct stringoutRecord	*pstringout;
{
    long status;

    status = dbPutLink(&pstringout->out,DBR_STRING,pstringout->val,1);
    return(status);
}
