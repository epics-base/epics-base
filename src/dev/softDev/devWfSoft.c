/* devWfSoft.c */
/* base/src/dev $Id$ */

/* devWfSoft.c - Device Support Routines for soft Waveform Records*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02	03-13-92	jba	ANSI C changes
 * .03  10-10-92        jba     replaced code with recGblGetLinkValue call
 *      ...
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
#include	<link.h>
#include	<waveformRecord.h>

/* Create the dset for devWfSoft */
static long init_record();
static long read_wf();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_wf;
}devWfSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_wf};


static long init_record(pwf)
    struct waveformRecord	*pwf;
{

    /* wf.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	pwf->nord = 0;
	break;
    case (PV_LINK) :
    case (DB_LINK) :
    case (CA_LINK) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pwf,
		"devWfSoft (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_wf(pwf)
    struct waveformRecord	*pwf;
{
    long status,nRequest;

    nRequest=pwf->nelm;
    status = dbGetLink(&pwf->inp,pwf->ftvl,pwf->bptr, 0,&nRequest);
    /*If dbGetLink got no values leave things as they were*/
    if(nRequest>0) pwf->nord = nRequest;

    return(0);
}
