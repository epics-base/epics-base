/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devAiSoft.c */
/* base/src/dev $Id$ */
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
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<aiRecord.h>
/* Create the dset for devAiSoft */
static long init_record();
static long read_ai();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
}devAiSoft={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_ai,
	NULL
};

static long init_record(pai)
    struct aiRecord	*pai;
{

    /* ai.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pai->inp.type) {
    case (CONSTANT) :
        if(recGblInitConstantLink(&pai->inp,DBF_DOUBLE,&pai->val))
            pai->udf = FALSE;
	break;
    case (PV_LINK) :
    case (DB_LINK) :
    case (CA_LINK) :
	break;
    default :
	recGblRecordError(S_db_badField, (void *)pai,
		"devAiSoft (init_record) Illegal INP field");

	return(S_db_badField);
    }
    /* Make sure record processing routine does not perform any conversion*/
    pai->linr = 0;
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
    long status;

    status = dbGetLink(&(pai->inp),DBR_DOUBLE, &(pai->val),0,0);
    if (pai->inp.type!=CONSTANT && RTN_SUCCESS(status)) pai->udf = FALSE;
    return(2); /*don't convert*/
}
