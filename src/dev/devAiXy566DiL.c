/* devAiXy566DiL.c */
/* base/src/dev $Id$ */

/* devAiXy566DiL.c - Device Support Routines */
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
 * .02  12-02-91        jba     Added cmd control to io-interrupt processing
 * .03  12-12-91        jba     Set cmd to zero in io-interrupt processing
 * .04	03-13-92	jba	ANSI C changes
 * .05	02-08-94	mrk	Issue Hardware Errors BUT prevent Error Message Storms
 *      ...
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<dbScan.h>
#include	<module_types.h>
#include	<aiRecord.h>

static long init_record();
static long get_ioint_info();
static long read_ai();
static long special_linconv();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
        DEVSUPFUN       get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
} devAiXy566DiL={
	6,
	NULL,
	NULL,
	init_record,
	get_ioint_info,
	read_ai,
	special_linconv};


static long init_record(pai)
    struct aiRecord	*pai;
{
    unsigned short value;
    struct vmeio *pvmeio;
    long status;

    /* ai.inp must be an VME_IO */
    switch (pai->inp.type) {
    case (VME_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiXy566DiL (init_record) Illegal INP field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;

    /* call driver so that it configures card */
    pvmeio = (struct vmeio *)&(pai->inp.value);
    if(status=ai_xy566_driver(pvmeio->card,pvmeio->signal,XY566DIL,&value)) {
	recGblRecordError(status,(void *)pai,
		"devAiXy566DiL (init_record) ai_xy566_driver error");
	return(status);
    }
    return(0);
}

static long get_ioint_info(int cmd, struct aiRecord *pai,IOSCANPVT *ppvt)
{
    ai_xy566_getioscanpvt(pai->inp.value.vmeio.card,ppvt);
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
	unsigned short value;
	struct vmeio *pvmeio;
	long status;

	
	pvmeio = (struct vmeio *)&(pai->inp.value);
	status=ai_xy566_driver(pvmeio->card,pvmeio->signal,XY566DIL,&value);
        if(status==-1) {
		status = 2; /*don't convert*/
                if(recGblSetSevr(pai,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pai->stat!=READ_ALARM || pai->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pai,"ai_xy566_driver Error");
		return(status);
        }else if(status==-2) {
                status=0;
                recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
        }
	if(status!=0) return(status);
	pai->rval = value;
	return(status);
}

static long special_linconv(pai,after)
    struct aiRecord	*pai;
    int after;
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;
    return(0);
}
