/* devWfSoft.c */
/* share/src/dev $Id$ */

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
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<waveformRecord.h>
/* Added for Channel Access Links */
long dbCaAddInlink();
long dbCaGetLink();

/* Create the dset for devWfSoft */
long init_record();
long read_wf();
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
    long status;

    /* wf.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	pwf->nord = 0;
	break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pwf->inp), (void *) pwf, "VAL");
        if(status) return(status);
	break;
    case (DB_LINK) :
	break;
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
    long status,options=0,nRequest;

    nRequest=pwf->nelm;
    status = recGblGetLinkValue(&(pwf->inp),(void *)pwf,pwf->ftvl,pwf->bptr,
              &options,&nRequest);
    pwf->nord = nRequest;

    return(0);
}
