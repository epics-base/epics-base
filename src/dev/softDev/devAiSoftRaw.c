/* devAiSoftRaw.c */
/* base/src/dev $Id$ */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
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
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02  03-04-92        jba     Added special_linconv 
 * .03	03-13-92	jba	ANSI C changes
 * .04  10-10-92        jba     replaced code with recGblGetLinkValue call
 * 	...
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "aiRecord.h"
/* Create the dset for devAiSoftRaw */
static long init_record();
static long read_ai();
static long special_linconv();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
}devAiSoftRaw={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_ai,
	special_linconv
};

static long init_record(aiRecord *pai)
{
    special_linconv(pai,1);
    /* ai.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	recGblInitConstantLink(&pai->inp,DBF_LONG,&pai->rval);
	break;
    case (PV_LINK) :
    case (DB_LINK) :
    case (CA_LINK) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiSoftRaw (init_record) Illegal INP field");
	return(S_db_badField);
    }
    special_linconv(pai,1);
    return(0);
}

static long read_ai( aiRecord *pai)
{
    long status;

    status = dbGetLink(&(pai->inp),DBR_LONG,&(pai->rval),0,0);
    return(0);
}

static long special_linconv(aiRecord *pai,int after)
{
    double eguf,egul,rawf,rawl;
    double eslo,eoff;

    if(!after) return(0);
    if(pai->rawf == pai->rawl) {
        errlogPrintf("%s devAiSoftRaw RAWF == RAWL\n",pai->name);
        return(0);
    }
    eguf = pai->eguf;
    egul = pai->egul;
    rawf = (double)pai->rawf;
    rawl = (double)pai->rawl;
    eslo = (eguf - egul)/(rawf - rawl);
    eoff = (rawf*egul - rawl*eguf)/(rawf - rawl);
    if(pai->eslo != eslo) {
        pai->eslo = eslo;
        db_post_events(pai,&pai->eslo,DBE_VALUE|DBE_LOG);
    }
    if(pai->eoff != eoff) {
        pai->eoff = eoff;
        db_post_events(pai,&pai->eoff,DBE_VALUE|DBE_LOG);
    }
    return(0);
}
