/* recStringin.c */
/* base/src/rec  $Id$ */

/* recStringin.c - Record Support Routines for Stringin records */
/*
 *      Author: 	Janet Anderson
 *      Date:   	4/23/91
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
 * .01  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .02  02-05-92	jba	Changed function arguments from paddr to precord 
 * .03  02-28-92	jba	ANSI C changes
 * .04  04-10-92        jba     pact now used to test for asyn processing, not status
 * .05  04-18-92        jba     removed process from dev init_record parms
 * .06  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .07  08-13-92        jba     Added simulation processing
 * .08  03-29-94        mcn     Added fast links
 */


#include        <vxWorks.h>
#include        <types.h>
#include        <stdioLib.h>
#include        <lstLib.h>
#include	<strLib.h>

#include        <alarm.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include        <dbFldTypes.h>
#include        <devSup.h>
#include        <errMdef.h>
#include        <recSup.h>
#include	<stringinRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
static long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

struct rset stringinRSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_units,
	get_precision,
	get_enum_str,
	get_enum_strs,
	put_enum_str,
	get_graphic_double,
	get_control_double,
	get_alarm_double };

struct stringindset { /* stringin input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_stringin; /*returns: (-1,0)=>(failure,success)*/
};
static void monitor();
static long readValue();


static long init_record(pstringin,pass)
    struct stringinRecord	*pstringin;
    int pass;
{
    struct stringindset *pdset;
    long status;

    if (pass==0) return(0);

    if (pstringin->siml.type == CONSTANT) {
        pstringin->simm = pstringin->siml.value.value;
    }
    else  {
        status = recGblInitFastInLink(&(pstringin->siml), (void *) pstringin, DBR_ENUM, "SIMM");
	if (status)
           return(status);
    }

    /* stringin.siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pstringin->siol.type == CONSTANT) {
        if (pstringin->siol.value.value!=0.0 ){
                 sprintf(pstringin->sval,"%-14.7g",pstringin->siol.value.value);
        }
    } 
    else {
        status = recGblInitFastInLink(&(pstringin->siol), (void *) pstringin, DBR_STRING, "SVAL");

	if (status)
           return(status);
    }

    if(!(pdset = (struct stringindset *)(pstringin->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pstringin,"stringin: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_stringin function defined */
    if( (pdset->number < 5) || (pdset->read_stringin == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pstringin,"stringin: init_record");
	return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pstringin))) return(status);
    }
    return(0);
}

static long process(pstringin)
	struct stringinRecord	*pstringin;
{
	struct stringindset	*pdset = (struct stringindset *)(pstringin->dset);
	long		 status;
	unsigned char    pact=pstringin->pact;

	if( (pdset==NULL) || (pdset->read_stringin==NULL) ) {
		pstringin->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pstringin,"read_stringin");
		return(S_dev_missingSup);
	}

	status=readValue(pstringin); /* read the new value */
	/* check if device support set pact */
	if ( !pact && pstringin->pact ) return(0);
	pstringin->pact = TRUE;

	recGblGetTimeStamp(pstringin);

	/* check event list */
	monitor(pstringin);
	/* process the forward scan link record */
	recGblFwdLink(pstringin);

	pstringin->pact=FALSE;
	return(status);
}


static long get_value(pstringin,pvdes)
    struct stringinRecord             *pstringin;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_STRING;
    pvdes->no_elements=1;
    pvdes->pvalue = (void *)(&pstringin->val[0]);
    return(0);
}


static void monitor(pstringin)
    struct stringinRecord             *pstringin;
{
    unsigned short  monitor_mask;

    monitor_mask = recGblResetAlarms(pstringin);
    if(strncmp(pstringin->oval,pstringin->val,sizeof(pstringin->val))) {
        db_post_events(pstringin,&(pstringin->val[0]),monitor_mask|DBE_VALUE);
	strncpy(pstringin->oval,pstringin->val,sizeof(pstringin->val));
    }
    return;
}

static long readValue(pstringin)
	struct stringinRecord	*pstringin;
{
	long		status;
        struct stringindset 	*pdset = (struct stringindset *) (pstringin->dset);

	if (pstringin->pact == TRUE){
		status=(*pdset->read_stringin)(pstringin);
		return(status);
	}

	status=recGblGetFastLink(&(pstringin->siml), (void *)pstringin, &(pstringin->simm));
	if (status)
		return(status);

	if (pstringin->simm == NO){
		status=(*pdset->read_stringin)(pstringin);
		return(status);
	}
	if (pstringin->simm == YES){
		status=recGblGetFastLink(&(pstringin->siol), (void *)pstringin, pstringin->sval);
		if (status==0) {
			strcpy(pstringin->val,pstringin->sval);
			pstringin->udf=FALSE;
		}
	} else {
		status=-1;
		recGblSetSevr(pstringin,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pstringin,SIMM_ALARM,pstringin->sims);

	return(status);
}
