/* recBo.c */
/* share/src/rec   $Id$ */
  
/* recBo.c - Record Support Routines for Binary Output records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            7-14-89
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
 * .01  12-9-88         lrd     lock the record during processing
 * .02  12-15-88        lrd     process the forward scan link
 * .03  12-23-88        lrd     Alarm on locked MAX_LOCKED times
 * .04  01-13-89        lrd     delete db_read_bo and db_write_bo
 * .05  01-20-89        lrd     fixed vx includes
 * .06  03-29-89        lrd     make hardware errors MAJOR
 *                              remove hw severity spec from database
 *                              add continuous control
 * .07  04-06-89        lrd     add monitor detection
 * .08  05-03-89        lrd     removed process mask from arg list
 * .09  08-16-89        lrd     add ability to do softchannel momentary
 * .10  08-17-89        lrd     add soft channel support
 * .11  01-31-90        lrd     add the plc_flag arg to the ab_bodriver call
 * .12  03-21-90        lrd     add db_post_events for RVAL and RBV
 * .13  04-02-90        ba/lrd  add monitor handling for momentary outputs
 *                              remove rbv arg to ab_bodriver
 * .14  04-05-90        lrd     make momentary output handling a task
 *                              as the watchdog runs at interrupt level
 * .15  04-11-90        lrd     make locals static
 * .16  05-02-90        lrd     fix initial value set in the DOL field
 * .17  10-10-90	mrk	Changes for record and device support
 * .18  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .19  01-08-92        jba     Added cast in call to wdStart to remove compile warning message
 * .20  02-05-92	jba	Changed function arguments from paddr to precord 
 * .21  02-28-92	jba	ANSI C changes
 * .22  03-03-92	jba	Changed callback handling
 * .23  04-10-92        jba     pact now used to test for asyn processing, not status
 * .24  04-18-92        jba     removed process from dev init_record parms
 * .25  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .26  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .27  08-14-92        jba     Added simulation processing
 * .28  08-19-92        jba     Added code for invalid alarm output action
 * .29  11-01-93        jba     Added get_precision routine
 * .30  04-05-94	mrk	ANSI changes to callback routines
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>
#include	<stdlib.h>
#include	<wdLib.h>

#include	<alarm.h>
#include	<callback.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include	<boRecord.h>

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
static long get_precision();
static long get_enum_str();
static long get_enum_strs();
static long put_enum_str();
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

struct rset boRSET={
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

struct bodset { /* binary output dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;  /*returns:(0,2)=>(success,success no convert*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;/*returns: (-1,0)=>(failure,success)*/
};


/* control block for callback*/
struct callback {
        CALLBACK        callback;
        struct dbCommon *precord;
        WDOG_ID wd_id;
};

static void alarm();
static void monitor();
static long writeValue();

void callbackRequest();

static void myCallback(pcallback)
    struct callback *pcallback;
{

    struct boRecord *pbo=(struct boRecord *)pcallback->precord;
    struct rset     *prset=(struct rset *)(pbo->rset);

    dbScanLock((struct dbCommon *)pbo);
    pbo->val = 0;
    dbProcess((struct dbCommon *)pbo);
    dbScanUnlock((struct dbCommon *)pbo);
}

static long init_record(pbo,pass)
    struct boRecord	*pbo;
    int pass;
{
    struct bodset *pdset;
    long status=0;
    struct callback *pcallback;

    if (pass==0) return(0);

    /* bo.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    switch (pbo->siml.type) {
    case (CONSTANT) :
        pbo->simm = pbo->siml.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pbo->siml), (void *) pbo, "SIMM");
	if(status) return(status);
	break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pbo,
                "bo: init_record Illegal SIML field");
        return(S_db_badField);
    }

    /* bo.siol may be a PV_LINK */
    if (pbo->siol.type == PV_LINK){
        status = dbCaAddOutlink(&(pbo->siol), (void *) pbo, "VAL");
	if(status) return(status);
    }

    if(!(pdset = (struct bodset *)(pbo->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pbo,"bo: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_bo functions defined */
    if( (pdset->number < 5) || (pdset->write_bo == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pbo,"bo: init_record");
	return(S_dev_missingSup);
    }
    /* get the initial value */
    if (pbo->dol.type == CONSTANT){
	if (pbo->dol.value.value == 0)  pbo->val = 0;
	else                            pbo->val = 1;
	pbo->udf = FALSE;
    }
    pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
    pbo->rpvt = (void *)pcallback;
    callbackSetCallback(myCallback,&pcallback->callback);
    pcallback->precord = (struct dbCommon *)pbo;

    if( pdset->init_record ) {
	status=(*pdset->init_record)(pbo);
	if(status==0) {
		if(pbo->rval==0) pbo->val = 0;
		else pbo->val = 1;
		pbo->udf = FALSE;
	} else if (status==2) status=0;
    }
    return(status);
}

static long process(pbo)
	struct boRecord     *pbo;
{
	struct bodset	*pdset = (struct bodset *)(pbo->dset);
	long		 status=0;
	int		wait_time;
	unsigned char    pact=pbo->pact;

	if( (pdset==NULL) || (pdset->write_bo==NULL) ) {
		pbo->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pbo,"write_bo");
		return(S_dev_missingSup);
	}
        if (!pbo->pact) {
		if((pbo->dol.type == DB_LINK) && (pbo->omsl == CLOSED_LOOP)){
			long options=0;
			long nRequest=1;
			unsigned short val;

			pbo->pact = TRUE;
			status = dbGetLink(&pbo->dol.value.db_link,(struct dbCommon *)pbo,
				DBR_USHORT,&val,&options,&nRequest);
			pbo->pact = FALSE;
			if(status==0){
				pbo->val = val;
				pbo->udf = FALSE;
			}else {
       				recGblSetSevr(pbo,LINK_ALARM,INVALID_ALARM);
			}
		}

		/* convert val to rval */
		if ( pbo->mask != 0 ) {
			if(pbo->val==0) pbo->rval = 0;
			else pbo->rval = pbo->mask;
		} else pbo->rval = (unsigned long)pbo->val;
	}

	/* check for alarms */
	alarm(pbo);

        if (pbo->nsev < INVALID_ALARM )
                status=writeValue(pbo); /* write the new value */
        else {
                switch (pbo->ivoa) {
                    case (IVOA_CONTINUE) :
                        status=writeValue(pbo); /* write the new value */
                        break;
                    case (IVOA_NO_OUTPUT) :
                        break;
                    case (IVOA_OUTPUT_IVOV) :
                        if(pbo->pact == FALSE){
				/* convert val to rval */
                                pbo->val=pbo->ivov;
				if ( pbo->mask != 0 ) {
					if(pbo->val==0) pbo->rval = 0;
					else pbo->rval = pbo->mask;
				} else pbo->rval = (unsigned long)pbo->val;
			}
                        status=writeValue(pbo); /* write the new value */
                        break;
                    default :
                        status=-1;
                        recGblRecordError(S_db_badField,(void *)pbo,
                                "bo:process Illegal IVOA field");
                }
        }

	/* check if device support set pact */
	if ( !pact && pbo->pact ) return(0);
	pbo->pact = TRUE;

	recGblGetTimeStamp(pbo);
	wait_time = (int)((pbo->high) * vxTicksPerSecond); /* seconds to ticks */
	if(pbo->val==1 && wait_time>0) {
		struct callback *pcallback;
		pcallback = (struct callback *)(pbo->rpvt);
        	if(pcallback->wd_id==NULL) pcallback->wd_id = wdCreate();
                callbackSetPriority(pbo->prio,&pcallback->callback);
               	wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
	}
	/* check event list */
	monitor(pbo);
	/* process the forward scan link record */
	recGblFwdLink(pbo);

	pbo->pact=FALSE;
	return(status);
}

static long get_value(pbo,pvdes)
    struct boRecord		*pbo;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_ENUM;
    pvdes->no_elements=1;
    (unsigned short *)(pvdes->pvalue) = &pbo->val;
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct boRecord	*pbo=(struct boRecord *)paddr->precord;

    if(paddr->pfield == (void *)&pbo->high) *precision=2;
    else recGblGetPrec(paddr,precision);
    return(0);
}

static long get_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char	  *pstring;
{
    struct boRecord	*pbo=(struct boRecord *)paddr->precord;

    if(pbo->val==0) {
	strncpy(pstring,pbo->znam,sizeof(pbo->znam));
	pstring[sizeof(pbo->znam)] = 0;
    } else if(pbo->val==1) {
	strncpy(pstring,pbo->onam,sizeof(pbo->onam));
	pstring[sizeof(pbo->onam)] = 0;
    } else {
	strcpy(pstring,"Illegal_Value");
    }
    return(0);
}

static long get_enum_strs(paddr,pes)
    struct dbAddr *paddr;
    struct dbr_enumStrs *pes;
{
    struct boRecord	*pbo=(struct boRecord *)paddr->precord;

    pes->no_str = 2;
    memset(pes->strs,'\0',sizeof(pes->strs));
    strncpy(pes->strs[0],pbo->znam,sizeof(pbo->znam));
    strncpy(pes->strs[1],pbo->onam,sizeof(pbo->onam));
    return(0);
}
static long put_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char          *pstring;
{
    struct boRecord     *pbo=(struct boRecord *)paddr->precord;

    if(strncmp(pstring,pbo->znam,sizeof(pbo->znam))==0) pbo->val = 0;
    else  if(strncmp(pstring,pbo->onam,sizeof(pbo->onam))==0) pbo->val = 1;
    else return(S_db_badChoice);
    return(0);
}


static void alarm(pbo)
    struct boRecord	*pbo;
{
	unsigned short val = pbo->val;

        /* check for udf alarm */
        if(pbo->udf == TRUE ){
			recGblSetSevr(pbo,UDF_ALARM,INVALID_ALARM);
        }

        /* check for  state alarm */
        if (val == 0){
		recGblSetSevr(pbo,STATE_ALARM,pbo->zsv);
        }else{
		recGblSetSevr(pbo,STATE_ALARM,pbo->osv);
        }

        /* check for cos alarm */
	if(val == pbo->lalm) return;
	recGblSetSevr(pbo,COS_ALARM,pbo->cosv);
	pbo->lalm = val;
        return;
}

static void monitor(pbo)
    struct boRecord	*pbo;
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(pbo);
        /* check for value change */
        if (pbo->mlst != pbo->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                pbo->mlst = pbo->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pbo,&pbo->val,monitor_mask);
        }
	if(pbo->oraw!=pbo->rval) {
		db_post_events(pbo,&pbo->rval,monitor_mask|DBE_VALUE);
		pbo->oraw = pbo->rval;
	}
	if(pbo->orbv!=pbo->rbv) {
		db_post_events(pbo,&pbo->rbv,monitor_mask|DBE_VALUE);
		pbo->orbv = pbo->rbv;
	}
        return;
}

static long writeValue(pbo)
	struct boRecord	*pbo;
{
	long		status;
        struct bodset 	*pdset = (struct bodset *) (pbo->dset);
	long            nRequest=1;
	long            options=0;

	if (pbo->pact == TRUE){
		status=(*pdset->write_bo)(pbo);
		return(status);
	}

	status=recGblGetLinkValue(&(pbo->siml),
		(void *)pbo,DBR_ENUM,&(pbo->simm),&options,&nRequest);
	if (status)
		return(status);

	if (pbo->simm == NO){
		status=(*pdset->write_bo)(pbo);
		return(status);
	}
	if (pbo->simm == YES){
		status=recGblPutLinkValue(&(pbo->siol),
				(void *)pbo,DBR_USHORT,&(pbo->val),&nRequest);
	} else {
		status=-1;
		recGblSetSevr(pbo,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pbo,SIMM_ALARM,pbo->sims);

	return(status);
}
