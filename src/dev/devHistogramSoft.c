/* devHistogramSoft.c */
/* share/src/dev	$Id$   */ 

/* devHistogramSoft.c - Device Support Routines for soft Histogram Input */
/*
 *      Author:		Janet Anderson
 *      Date:		07/02/91
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
 * .01	mm-dd-yy	iii	Comment
 * .02	mm-dd-yy	iii	Comment
 * .03	03-13-92	jba	ANSI C changes
 * 	...
 */



#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<devSup.h>
#include	<link.h>
#include        <histogramRecord.h>

/* Create the dset for devHistogramSoft */
long init_record();
long read_histogram();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_histogram;
	DEVSUPFUN	special_linconv;
}devHistogramSoft={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_histogram,
	NULL};


static long init_record(phistogram)
    struct histogramRecord	*phistogram;
{
    char message[100];

    /* histogram.svl must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (phistogram->svl.type) {
    case (CONSTANT) :
	phistogram->sgnl = phistogram->svl.value.value;
	break;
    case (PV_LINK) :
	break;
    case (DB_LINK) :
	break;
    case (CA_LINK) :
	break;
    default :
	strcpy(message,phistogram->name);
	strcat(message,": devHistogramSoft (init_record) Illegal SVL field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_histogram(phistogram)
    struct histogramRecord	*phistogram;
{
    char message[100];
    long status,options,nRequest;

    /* histogram.svl must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (phistogram->svl.type) {
    case (CONSTANT) :
	break;
    case (DB_LINK) :
	options=0;
	nRequest=1;
	status = dbGetLink(&(phistogram->svl.value.db_link),(struct dbCommon *)phistogram,DBR_DOUBLE,
		&(phistogram->sgnl),&options,&nRequest);
	if(status!=0) {
		if(phistogram->nsev<VALID_ALARM) {
			phistogram->nsev = VALID_ALARM;
			phistogram->nsta = LINK_ALARM;
		}
	}
	break;
    case (CA_LINK) :
	break;
    default :
	if(phistogram->nsev<VALID_ALARM) {
		phistogram->nsev = VALID_ALARM;
		phistogram->nsta = SOFT_ALARM;
		if(phistogram->stat!=SOFT_ALARM) {
			strcpy(message,phistogram->name);
			strcat(message,": devHistogramSoft (read_histogram) Illegal SVL field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0); /*add count*/
}
