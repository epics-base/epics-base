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
/*
 *      Author:		Janet Anderson
 *      Date:   	09-23-91
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "longoutRecord.h"
#include "epicsExport.h"

/* Create the dset for devLoSoft */
static long init_record(longoutRecord *prec);
static long write_longout(longoutRecord *prec);
struct {
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
epicsExportAddress(dset,devLoSoft);

static long init_record(longoutRecord *prec)
{
    return 0;
} /* end init_record() */

static long write_longout(longoutRecord	*prec)
{
    dbPutLink(&prec->out,DBR_LONG, &prec->val,1);
    return 0;
}
