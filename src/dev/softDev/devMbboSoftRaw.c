/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devMbboSoftRaw.c */
/*
 *      Author:		Janet Anderson
 *      Date:		3-28-92
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
#include "mbboRecord.h"
#include "epicsExport.h"

/* Create the dset for devMbboSoftRaw */
static long init_record(mbboRecord *prec);
static long write_mbbo(mbboRecord *prec);
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboSoftRaw={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_mbbo
};
epicsExportAddress(dset,devMbboSoftRaw);

static long init_record(mbboRecord *prec)
{
 
    long status;
 
    /*to preserve old functionality*/
    if(prec->nobt == 0) prec->mask = 0xffffffff;
    prec->mask <<= prec->shft;
    /*dont convert*/
    status = 2;
    return status;
 
} /* end init_record() */

static long write_mbbo(mbboRecord *prec)
{
    long status;
    unsigned long data;

    data = prec->rval & prec->mask;
    status = dbPutLink(&prec->out,DBR_LONG, &data,1);
    return(0);
}
