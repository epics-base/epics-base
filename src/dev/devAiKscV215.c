/* devAiKscV215.c */
/* share/src/dev $Id$ */

/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            09-02-92
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
 * .01  08-02-92	mrk	Original version
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
#include	<dbScan.h>
#include	<link.h>
#include	<module_types.h>
#include	<aiRecord.h>

static long init_ai();
static long ai_ioinfo();
static long read_ai();
static long ai_lincvt();


typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	DEVSUPFUN	special_linconv;} AIDSET;


AIDSET devAiKscV215={6,NULL,NULL,init_ai,ai_ioinfo,read_ai,ai_lincvt};

static long init_ai( struct aiRecord	*pai)
{
    unsigned short value;
    struct vmeio *pvmeio;
    long  status;

    /* ai.inp must be an VME_IO */
    switch (pai->inp.type) {
    case (VME_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiAt5Vxi (init_record) Illegal INP field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;

    /* call driver so that it configures card */
    pvmeio = (struct vmeio *)&(pai->inp.value);
    if(status=KscV215_ai_driver(pvmeio->card,pvmeio->signal,&value)) {
	recGblRecordError(status,(void *)pai,
		"devAiKscV215 (init_record) KscV215_ai_driver error");
	return(status);
    }
    return(0);
}

static long ai_ioinfo(
    int               cmd,
    struct aiRecord     *pai,
    IOSCANPVT		*ppvt)
{
    KscV215_getioscanpvt(pai->inp.value.vmeio.card,ppvt);
    return(0);
}

static long read_ai(struct aiRecord	*pai)
{
	struct vmeio *pvmeio;
	int	     status;
	unsigned short value;

	
	pvmeio = (struct vmeio *)&(pai->inp.value);
	status = KscV215_ai_driver(pvmeio->card,pvmeio->signal,&value);
	if(status==0 || status==-2) pai->rval = value & 0xfff;
        if(status==-1) {
		status = 2; /*don't convert*/
                recGblSetSevr(pai,READ_ALARM,INVALID_ALARM);
        }else if(status==-2) {
                status=0;
                recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
        }
	return(status);
}

static long ai_lincvt(struct aiRecord	*pai, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;
    return(0);
}
