/* recStringout.c */
/* base/src/rec  $Id$ */

/* recStringout.c - Record Support Routines for Stringout records */
/*
 * Author: 	Janet Anderson
 * Date:	4/23/91
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
 * .01  10-24-91        jba     Removed unused code
 * .02  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .03  02-05-92	jba	Changed function arguments from paddr to precord 
 * .04  02-28-92	jba	ANSI C changes
 * .05  04-10-92        jba     pact now used to test for asyn processing, not status
 * .06  04-18-92        jba     removed process from dev init_record parms
 * .07  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .08  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .09  08-14-92        jba     Added simulation processing
 * .10  08-19-92        jba     Added code for invalid alarm output action
 * .11  10-10-92        jba     replaced code for get of VAL from DOL with recGblGetLinkValue call
 * .12  10-18-92        jba     pact now set in recGblGetLinkValue
 * .13  03-30-94        mcn     converted to fast links
 */ 


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<strLib.h>

#include        <alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#define GEN_SIZE_OFFSET
#include	<stringoutRecord.h>
#undef  GEN_SIZE_OFFSET
#include      <menuIvoa.h>

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

struct rset stringoutRSET={
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

struct stringoutdset { /* stringout input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_stringout;/*(-1,0)=>(failure,success)*/
};
static void monitor();
static long writeValue();


static long init_record(pstringout,pass)
    struct stringoutRecord	*pstringout;
    int pass;
{
    struct stringoutdset *pdset;
    long status=0;

    if (pass==0) return(0);

    if (pstringout->siml.type == CONSTANT) {
	recGblInitConstantLink(&pstringout->siml,DBF_USHORT,&pstringout->simm);
    }

    if(!(pdset = (struct stringoutdset *)(pstringout->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pstringout,"stringout: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_stringout functions defined */
    if( (pdset->number < 5) || (pdset->write_stringout == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pstringout,"stringout: init_record");
	return(S_dev_missingSup);
    }
    /* get the initial value dol is a constant*/
    if (pstringout->dol.type == CONSTANT){
	if(recGblInitConstantLink(&pstringout->dol,DBF_STRING,pstringout->val))
	    pstringout->udf=FALSE;
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pstringout))) return(status);
    }
    return(0);
}

static long process(pstringout)
	struct stringoutRecord	*pstringout;
{
	struct stringoutdset	*pdset = (struct stringoutdset *)(pstringout->dset);
	long		 status=0;
	unsigned char    pact=pstringout->pact;

	if( (pdset==NULL) || (pdset->write_stringout==NULL) ) {
		pstringout->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pstringout,"write_stringout");
		return(S_dev_missingSup);
	}
        if (!pstringout->pact && pstringout->omsl == CLOSED_LOOP){
		status = dbGetLink(&(pstringout->dol),
			DBR_STRING,pstringout->val,0,0);
		if(RTN_SUCCESS(status)) pstringout->udf=FALSE;
	}

        if(pstringout->udf == TRUE ){
                recGblSetSevr(pstringout,UDF_ALARM,INVALID_ALARM);
                goto finish;
        }

        if (pstringout->nsev < INVALID_ALARM )
                status=writeValue(pstringout); /* write the new value */
        else {
                switch (pstringout->ivoa) {
                    case (menuIvoaContinue_normally) :
                        status=writeValue(pstringout); /* write the new value */
                        break;
                    case (menuIvoaDon_t_drive_outputs) :
                        break;
                    case (menuIvoaSet_output_to_IVOV) :
                        if(pstringout->pact == FALSE){
                                strcpy(pstringout->val,pstringout->ivov);
                        }
                        status=writeValue(pstringout); /* write the new value */
                        break;
                    default :
                        status=-1;
                        recGblRecordError(S_db_badField,(void *)pstringout,
                                "stringout:process Illegal IVOA field");
                }
        }

	/* check if device support set pact */
	if ( !pact && pstringout->pact ) return(0);
finish:
	pstringout->pact = TRUE;
	recGblGetTimeStamp(pstringout);
	monitor(pstringout);
	recGblFwdLink(pstringout);
	pstringout->pact=FALSE;
	return(status);
}

static long get_value(pstringout,pvdes)
    struct stringoutRecord             *pstringout;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_STRING;
    pvdes->no_elements=1;
    pvdes->pvalue = (void *)(&pstringout->val[0]);
    return(0);
}


static void monitor(pstringout)
    struct stringoutRecord             *pstringout;
{
    unsigned short  monitor_mask;

    monitor_mask = recGblResetAlarms(pstringout);
    if(strncmp(pstringout->oval,pstringout->val,sizeof(pstringout->val))) {
	monitor_mask |= DBE_VALUE|DBE_LOG;
	strncpy(pstringout->oval,pstringout->val,sizeof(pstringout->val));
    }
    if(monitor_mask)
	db_post_events(pstringout,&(pstringout->val[0]),monitor_mask);
    return;
}

static long writeValue(pstringout)
	struct stringoutRecord	*pstringout;
{
	long		status;
        struct stringoutdset 	*pdset = (struct stringoutdset *) (pstringout->dset);

	if (pstringout->pact == TRUE){
		status=(*pdset->write_stringout)(pstringout);
		return(status);
	}

	status=dbGetLink(&(pstringout->siml),DBR_USHORT,
		&(pstringout->simm),0,0);
	if (status)
		return(status);

	if (pstringout->simm == NO){
		status=(*pdset->write_stringout)(pstringout);
		return(status);
	}
	if (pstringout->simm == YES){
		status=dbPutLink(&pstringout->siol,DBR_STRING,
			pstringout->val,1);
	} else {
		status=-1;
		recGblSetSevr(pstringout,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pstringout,SIMM_ALARM,pstringout->sims);

	return(status);
}
