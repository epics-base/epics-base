/* recGsub.c */
/* base/src/rec  $Id$ */

/* recGsub.c - Record Support Routines for Subroutine records */
/*
 *      Original Author: Rozelle Wright and Deb Kerstien
 *
 *      Date:            01-07-93
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
 * .01  01-07-93	rmw simulates old gtacs subroutine record
 *					  currently does not support callback.
  */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>
#include	<symLib.h>
#include        <sysSymTbl.h>   /* for sysSymTbl*/
#include        <a_out.h>       /* for N_TEXT */

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<gsubRecord.h>

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
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();

struct rset gsubRSET={
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

static void alarm();
static long do_gsub();
static long fetch_values();
static void monitor();

#define ARG_MAX 6
 /* Fldnames should have as many as ARG_MAX */
 static char Fldnames[ARG_MAX][FLDNAME_SZ] =
     {"A", "B", "C", "D", "E", "F"};


static long init_record(psub,pass)
    struct gsubRecord	*psub;
    int pass;
{
    FUNCPTR	psubroutine;
    char	sub_type;
    char	temp[40];
    int	status;
    STATUS	ret;
    struct link *plink;
    int i;
    float *pvalue;
	int 	*pcallbackdummy = NULL;  /*  dummy callback arguments */

    if (pass==0) return(0);

    plink = &psub->inpa;
    pvalue = &psub->a;
    for(i=0; i<ARG_MAX; i++, plink++, pvalue++) {
        if(plink->type==CONSTANT) *pvalue = plink->value.value;
        if (plink->type == PV_LINK)
        {
            status = dbCaAddInlink(plink, (void *) psub, Fldnames[i]);
            if(status) return(status);
        } /* endif */
    }

    /* convert the initialization subroutine name  */
    temp[0] = 0;			/* all global variables start with _ */
    if (psub->inam[0] != '_'){
	strcpy(temp,"_");
    }
    strcat(temp,psub->inam);
    ret = symFindByName(sysSymTbl,temp,(void *)&psub->sadr,(void *)&sub_type);
    if ((ret !=OK) || ((sub_type & N_TEXT) == 0)){
	recGblRecordError(S_db_BadSub,(void *)psub,"recGsub(init_record)");
	return(S_db_BadSub);
    }

    /* invoke the initialization subroutine */
    psubroutine = (FUNCPTR)(psub->sadr);
    status = psubroutine(pcallbackdummy, pcallbackdummy,
		&(psub->a),&(psub->b),&(psub->c),&(psub->d),&(psub->e), &(psub->f),&(psub->val));

    /* convert the subroutine name to an address and type */
    /* convert the processing subroutine name  */
    temp[0] = 0;			/* all global variables start with _ */
    if (psub->snam[0] != '_'){
    	strcpy(temp,"_");
    }
    strcat(temp,psub->snam);
    ret = symFindByName(sysSymTbl,temp,(void *)&psub->sadr,(void *)&sub_type);
    if ((ret < 0) || ((sub_type & N_TEXT) == 0)){
	recGblRecordError(S_db_BadSub,(void *)psub,"recGsub(init_record)");
	return(S_db_BadSub);
    }
    psub->styp = sub_type;
    return(0);
}

static long process(psub)
	struct gsubRecord *psub;
{
	long		 status=0;

        if(!psub->pact){
		psub->pact = TRUE;
		status = fetch_values(psub);
		psub->pact = FALSE;
	}
        if(status==0) status = do_gsub(psub);
        psub->pact = TRUE;
	if(status==1) return(0);
	recGblGetTimeStamp(psub);
        /* check for alarms */
        alarm(psub);
        /* check event list */
        monitor(psub);
        /* process the forward scan link record */
        recGblFwdLink(psub);
        psub->pact = FALSE;
        return(0);
}

static long get_value(psub,pvdes)
    struct gsubRecord		*psub;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_FLOAT;
    pvdes->no_elements=1;
    (float *)(pvdes->pvalue) = &psub->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct gsubRecord	*psub=(struct gsubRecord *)paddr->precord;

    strncpy(units,psub->egu,sizeof(psub->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct gsubRecord	*psub=(struct gsubRecord *)paddr->precord;

    *precision = psub->prec;
    if(paddr->pfield==(void *)&psub->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}


static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct gsubRecord	*psub=(struct gsubRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psub->val
    || paddr->pfield==(void *)&psub->hihi
    || paddr->pfield==(void *)&psub->high
    || paddr->pfield==(void *)&psub->low
    || paddr->pfield==(void *)&psub->lolo){
        pgd->upper_disp_limit = psub->hopr;
        pgd->lower_disp_limit = psub->lopr;
        return(0);
    }

    if(paddr->pfield>=(void *)&psub->a && paddr->pfield<=(void *)&psub->f){
        pgd->upper_disp_limit = psub->hopr;
        pgd->lower_disp_limit = psub->lopr;
        return(0);
    }
    if(paddr->pfield>=(void *)&psub->la && paddr->pfield<=(void *)&psub->lf){
        pgd->upper_disp_limit = psub->hopr;
        pgd->lower_disp_limit = psub->lopr;
        return(0);
    }
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct gsubRecord	*psub=(struct gsubRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psub->val
    || paddr->pfield==(void *)&psub->hihi
    || paddr->pfield==(void *)&psub->high
    || paddr->pfield==(void *)&psub->low
    || paddr->pfield==(void *)&psub->lolo){
        pcd->upper_ctrl_limit = psub->hopr;
        pcd->lower_ctrl_limit = psub->lopr;
       return(0);
    } 

    if(paddr->pfield>=(void *)&psub->a && paddr->pfield<=(void *)&psub->f){
        pcd->upper_ctrl_limit = psub->hopr;
        pcd->lower_ctrl_limit = psub->lopr;
        return(0);
    }
    if(paddr->pfield>=(void *)&psub->la && paddr->pfield<=(void *)&psub->lf){
        pcd->upper_ctrl_limit = psub->hopr;
        pcd->lower_ctrl_limit = psub->lopr;
        return(0);
    }
    return(0);
}

static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct gsubRecord	*psub=(struct gsubRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psub->val){
         pad->upper_alarm_limit = psub->hihi;
         pad->upper_warning_limit = psub->high;
         pad->lower_warning_limit = psub->low;
         pad->lower_alarm_limit = psub->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void alarm(psub)
    struct gsubRecord	*psub;
{
	double		val;
	float		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if(psub->udf == TRUE ){
 		recGblSetSevr(psub,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = psub->hihi; lolo = psub->lolo; high = psub->high; low = psub->low;
	hhsv = psub->hhsv; llsv = psub->llsv; hsv = psub->hsv; lsv = psub->lsv;
	val = psub->val; hyst = psub->hyst; lalm = psub->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(psub,HIHI_ALARM,psub->hhsv)) psub->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(psub,LOLO_ALARM,psub->llsv)) psub->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(psub,HIGH_ALARM,psub->hsv)) psub->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(psub,LOW_ALARM,psub->lsv)) psub->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	psub->lalm = val;
	return;
}

static void monitor(psub)
    struct gsubRecord	*psub;
{
	unsigned short	monitor_mask;
	double		delta;
	float           *pnew;
	float          *pprev;
	int             i;

        monitor_mask = recGblResetAlarms(psub);
        /* check for value change */
        delta = psub->mlst - psub->val;
        if(delta<0.0) delta = -delta;
        if (delta > psub->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                psub->mlst = psub->val;
        }
        /* check for archive change */
        delta = psub->alst - psub->val;
        if(delta<0.0) delta = -delta;
        if (delta > psub->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                psub->alst = psub->val;
        }
        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(psub,&psub->val,monitor_mask);
        }
	/* check all input fields for changes*/
	for(i=0, pnew=&psub->a, pprev=&psub->la; i<ARG_MAX; i++, pnew++, pprev++) {
		if(*pnew != *pprev) {
			db_post_events(psub,pnew,monitor_mask|DBE_VALUE);
			*pprev = *pnew;
		}
	}
        return;
}

static long fetch_values(psub)
struct gsubRecord *psub;
{
        struct link     *plink; /* structure of the link field  */
        float           *pvalue;
        long            options=0,nRequest=1;
        int             i;
	long		status;

        for(i=0, plink=&psub->inpa, pvalue=&psub->a; i<ARG_MAX; i++, plink++, pvalue++) {
		nRequest=1;
		status=recGblGetLinkValue(plink,(void *)psub,DBR_FLOAT,
			pvalue,&options,&nRequest);
		if (!RTN_SUCCESS(status)) return(-1);
        }
        return(0);
}

static long do_gsub(psub)
struct gsubRecord *psub;  /* pointer to subroutine record  */
{
	int	status;
	FUNCPTR	psubroutine;
	int 	*pcallbackdummy = NULL;  /*  dummy callback arguments */

	/* call the subroutine */
	psubroutine = (FUNCPTR)(psub->sadr);
	if(psubroutine==NULL) {
               	recGblSetSevr(psub,BAD_SUB_ALARM,INVALID_ALARM);
		return(0);
	}
	status = psubroutine(pcallbackdummy, pcallbackdummy,
		&(psub->a),&(psub->b),&(psub->c),&(psub->d),&(psub->e), 
		&(psub->f), &(psub->val));
	if(status < 0){
               	recGblSetSevr(psub,SOFT_ALARM,psub->brsv);
	} else psub->udf = FALSE;
	return(status);
}
