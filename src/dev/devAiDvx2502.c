/* devAiDvx2502.c */
/* share/src/dev $Id$ */

/* devAiDvx2502.c - Device Support Routines */
/*
 *      Original Author: Bob Dalesio
 *      Current Author: Marty Kraimer
 *      Date:  3/6/91
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
 * .02  12-02-91        jba     Added cmd control to io-interrupt processing
 * .03  12-12-91        jba     Set cmd to zero in io-interrupt processing
 * .04	03-13-92	jba	ANSI C changes
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
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<aiRecord.h>

long init_record();
long get_ioint_info();
long read_ai();
long special_linconv();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
        DEVSUPFUN       get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
} devAiDvx2502={
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
    char message[100];
    unsigned short value;
    struct vmeio *pvmeio;
    long status;

    /* ai.inp must be an VME_IO */
    switch (pai->inp.type) {
    case (VME_IO) :
	break;
    default :
	strcpy(message,pai->name);
	strcat(message,": devAiDvx2502 (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/65535.0;

    /* call driver so that it configures card */
    pvmeio = (struct vmeio *)&(pai->inp.value);
    if(status=dvx_driver(pvmeio->card,pvmeio->signal,&value)) {
	strcpy(message,pai->name);
	strcat(message,": devAiDvx2502 (init_record) dvx_driver error");
	errMessage(status,message);
	return(status);
    }
    return(0);
}

static long get_ioint_info(cmd,pai,io_type,card_type,card_number)
    short               *cmd;
    struct aiRecord     *pai;
    short               *io_type;
    short               *card_type;
    short               *card_number;
{
    *cmd=-1;
    if(pai->inp.type != VME_IO) return(S_dev_badInpType);
    *io_type = IO_AI;
    *card_type = DVX2502;
    *card_number = pai->inp.value.vmeio.card;
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
	unsigned short value;
	struct vmeio *pvmeio;
	long status;

	
	pvmeio = (struct vmeio *)&(pai->inp.value);
	if(status=dvx_driver(pvmeio->card,pvmeio->signal,&value))
		return(status);
	*((unsigned short*)(&pai->rval))=value;
	if(status==0 || status==-2) pai->rval = value;
        if(status==-1) {
                recGblSetSevr(pai,READ_ALARM,VALID_ALARM);
		status=2; /*don't convert*/
        }else if(status==-2) {
                status=0;
                recGblSetSevr(pai,HW_LIMIT_ALARM,VALID_ALARM);
        }
	return(status);
}

static long special_linconv(pai,after)
    struct aiRecord	*pai;
    int after;
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/65535.0;
    return(0);
}
