/* recSub.c */
/* share/src/rec $Id$ */

/* recSub.c - Record Support Routines for Subroutine records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            01-25-90
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
 * .01  10-10-90	mrk	Made changes for new record support
 * .02  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<symLib.h>
#include        <sysSymTbl.h>   /* for sysSymTbl*/
#include        <a_out.h>       /* for N_TEXT */

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<subRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
long init_record();
long process();
#define special NULL
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
long get_units();
long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
long get_graphic_double();
long get_control_double();
long get_alarm_double();

struct rset subRSET={
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

#define ARG_MAX 12
void alarm();
void monitor();
long do_sub();
long fetch_values();

static long init_record(psub)
    struct subRecord	*psub;
{
    FUNCPTR	psubroutine;
    char	sub_type;
    char	temp[40];
    long	status;
    STATUS	ret;
    struct link *plink;
    int i;
    double *pvalue;

    plink = &psub->inpa;
    pvalue = &psub->a;
    for(i=0; i<ARG_MAX; i++, plink++, pvalue++) {
        if(plink->type==CONSTANT) *pvalue = plink->value.value;
    }

    /* convert the initialization subroutine name  */
    temp[0] = 0;			/* all global variables start with _ */
    if (psub->inam[0] != '_'){
	strcpy(temp,"_");
    }
    strcat(temp,psub->inam);
    ret = symFindByName(sysSymTbl,temp,&psub->sadr,&sub_type);
    if ((ret !=OK) || ((sub_type & N_TEXT) == 0)){
	recGblRecordError(S_db_BadSub,psub,"recSub(init_record)");
	return(S_db_BadSub);
    }

    /* invoke the initialization subroutine */
    psubroutine = (FUNCPTR)(psub->sadr);
    status = psubroutine(psub,process);

    /* convert the subroutine name to an address and type */
    /* convert the processing subroutine name  */
    temp[0] = 0;			/* all global variables start with _ */
    if (psub->snam[0] != '_'){
    	strcpy(temp,"_");
    }
    strcat(temp,psub->snam);
    ret = symFindByName(sysSymTbl,temp,&psub->sadr,&sub_type);
    if ((ret < 0) || ((sub_type & N_TEXT) == 0)){
	recGblRecordError(S_db_BadSub,psub,"recSub(init_record)");
	return(S_db_BadSub);
    }
    psub->styp = sub_type;
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
	struct subRecord *psub=(struct subRecord *)(paddr->precord);
	long		 status=0;

        if(!psub->pact){
		psub->pact = TRUE;
		status = fetch_values(psub);
		psub->pact = FALSE;
	}
        if(status==0) status = do_sub(psub);
        psub->pact = TRUE;
	if(status==1) return(0);
	tsLocalTime(&psub->time);
        /* check for alarms */
        alarm(psub);
        /* check event list */
        monitor(psub);
        /* process the forward scan link record */
        if (psub->flnk.type==DB_LINK) dbScanPassive(psub->flnk.value.db_link.pdbAddr);
        psub->pact = FALSE;
        return(0);
}

static long get_value(psub,pvdes)
    struct subRecord		*psub;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_DOUBLE;
    pvdes->no_elements=1;
    (double *)(pvdes->pvalue) = &psub->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    strncpy(units,psub->egu,sizeof(psub->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    *precision = psub->prec;
    return(0);
}


static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    pgd->upper_disp_limit = psub->hopr;
    pgd->lower_disp_limit = psub->lopr;
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    pcd->upper_ctrl_limit = psub->hopr;
    pcd->lower_ctrl_limit = psub->lopr;
    return(0);
}
static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    pad->upper_alarm_limit = psub->hihi;
    pad->upper_warning_limit = psub->high;
    pad->lower_warning_limit = psub->low;
    pad->lower_alarm_limit = psub->lolo;
    return(0);
}


static void alarm(psub)
    struct subRecord	*psub;
{
	double	ftemp;
	double	val=psub->val;

        /* undefined condition */
	if(psub->udf == TRUE) {
                if(recGblSetSevr(psub,UDF_ALARM,VALID_ALARM)) return;
        }
        /* if difference is not > hysterisis use lalm not val */
        ftemp = psub->lalm - psub->val;
        if(ftemp<0.0) ftemp = -ftemp;
        if (ftemp < psub->hyst) val=psub->lalm;

        /* alarm condition hihi */
        if (val > psub->hihi && recGblSetSevr(psub,HIHI_ALARM,psub->hhsv)){
                psub->lalm = val;
                return;
        }

        /* alarm condition lolo */
        if (val < psub->lolo && recGblSetSevr(psub,LOLO_ALARM,psub->llsv)){
                psub->lalm = val;
                return;
        }

        /* alarm condition high */
        if (val > psub->high && recGblSetSevr(psub,HIGH_ALARM,psub->hsv)){
                psub->lalm = val;
                return;
        }

        /* alarm condition low */
        if (val < psub->low && recGblSetSevr(psub,LOW_ALARM,psub->lsv)){
                psub->lalm = val;
                return;
        }
        return;
}

static void monitor(psub)
    struct subRecord	*psub;
{
	unsigned short	monitor_mask;
	double		delta;
        short           stat,sevr,nsta,nsev;
	double           *pnew;
	double           *pprev;
	int             i;

        /* get previous stat and sevr  and new stat and sevr*/
        recGblResetSevr(psub,stat,sevr,nsta,nsev);

        monitor_mask = 0;

        /* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev) {
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and nsev fields */
                db_post_events(psub,&psub->stat,DBE_VALUE);
                db_post_events(psub,&psub->sevr,DBE_VALUE);
        }
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
		/* check all input fields for changes*/
		for(i=0, pnew=&psub->a, pprev=&psub->la; i<ARG_MAX; i++, pnew++, pprev++) {
			if(*pnew != *pprev) {
				db_post_events(psub,pnew,monitor_mask|DBE_VALUE);
				*pprev = *pnew;
			}
		}
        }
        return;
}

static long fetch_values(psub)
struct subRecord *psub;
{
        struct link     *plink; /* structure of the link field  */
        double           *pvalue;
        long            options,nRequest;
        int             i;
	long		status;

        for(i=0, plink=&psub->inpa, pvalue=&psub->a; i<ARG_MAX; i++, plink++, pvalue++) {
                if(plink->type!=DB_LINK) continue;
                options=0;
                nRequest=1;
                status=dbGetLink(&plink->value.db_link,psub,DBR_DOUBLE,
                        pvalue,&options,&nRequest);
                if(status!=0) {
                	recGblSetSevr(psub,LINK_ALARM,VALID_ALARM);
                        return(-1);
                }
        }
        return(0);
}

static long do_sub(psub)
struct subRecord *psub;  /* pointer to subroutine record  */
{
	long	status;
	FUNCPTR	psubroutine;


	/* call the subroutine */
	psubroutine = (FUNCPTR)(psub->sadr);
	if(psubroutine==NULL) {
               	recGblSetSevr(psub,BAD_SUB_ALARM,VALID_ALARM);
		return(0);
	}
	status = psubroutine(psub);
	if(status < 0){
               	recGblSetSevr(psub,SOFT_ALARM,psub->brsv);
	} else psub->udf = FALSE;
	return(status);
}
