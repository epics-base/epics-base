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
static long init_record();
static long read_histogram();
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
	recGblRecordError(S_db_badField,(void *)phistogram,
		"devHistogramSoft (init_record) Illegal SVL field");
	return(S_db_badField);
    }
    return(0);
}

static long read_histogram(phistogram)
    struct histogramRecord	*phistogram;
{
    long status,options=0,nRequest=1;

    status = recGblGetLinkValue(&(phistogram->svl),(void *)phistogram,DBR_DOUBLE,&(phistogram->sgnl),
              &options,&nRequest);

    return(0); /*add count*/
}
