/* recSub.c */
/* share/src/rec $Id$ */

/* recSub.c - Record Support Routines for Subroutine records
 *
 * Author:      Bob Dalesio
 * Date:        01-25-90
 *
 *	Control System Software for the GTA Project
 *
 *	Copyright 1988, 1989, the Regents of the University of California.
 *
 *	This software was produced under a U.S. Government contract
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *	operated by the University of California for the U.S. Department
 *	of Energy.
 *
 *	Developed by the Controls and Automation Group (AT-8)
 *	Accelerator Technology Division
 *	Los Alamos National Laboratory
 *
 *	Direct inqueries to:
 *	Bob Dalesio, AT-8, Mail Stop H820
 *	Los Alamos National Laboratory
 *	Los Alamos, New Mexico 87545
 *	Phone: (505) 667-3414
 *	E-mail: dalesio@luke.lanl.gov
 *
 * Modification Log:
 * -----------------
 * .01  10-10-90	mrk	Made changes for new record support
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
long get_precision();
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_enum_str NULL
long get_units();
long get_graphic_double();
long get_control_double();
#define get_enum_strs NULL

struct rset subRSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_precision,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_enum_str,
	get_units,
	get_graphic_double,
	get_control_double,
	get_enum_strs };

void alarm();
void monitor();
long do_sub();
void fetch_values();

static long init_record(psub)
    struct subRecord	*psub;
{
    FUNCPTR	psubroutine;
    char	sub_type;
    char	temp[40];
    long	status;
    STATUS	ret;

    if(psub->inpa.type==CONSTANT) psub->a = psub->inpa.value.value;
    if(psub->inpb.type==CONSTANT) psub->b = psub->inpb.value.value;
    if(psub->inpc.type==CONSTANT) psub->c = psub->inpc.value.value;
    if(psub->inpd.type==CONSTANT) psub->d = psub->inpd.value.value;
    if(psub->inpe.type==CONSTANT) psub->e = psub->inpe.value.value;
    if(psub->inpf.type==CONSTANT) psub->f = psub->inpf.value.value;


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
    /* convert the initialization subroutine name  */
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

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    *precision = psub->prec;
    return(0);
}

static long get_value(psub,pvdes)
    struct subRecord		*psub;
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
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    strncpy(units,psub->egu,sizeof(psub->egu));
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    pgd->upper_disp_limit = psub->hopr;
    pgd->lower_disp_limit = psub->lopr;
    pgd->upper_alarm_limit = psub->hihi;
    pgd->upper_warning_limit = psub->high;
    pgd->lower_warning_limit = psub->low;
    pgd->lower_alarm_limit = psub->lolo;
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

static long process(paddr)
    struct dbAddr	*paddr;
{
	struct subRecord *psub=(struct subRecord *)(paddr->precord);
	long		 status;

        if(!psub->pact){
		psub->pact = TRUE;
		fetch_values(psub);
		psub->pact = FALSE;
	}
        status = do_sub(psub);
        psub->pact = TRUE;
	if(status==1) return(0);
        /* check for alarms */
        alarm(psub);
        /* check event list */
        monitor(psub);
        /* process the forward scan link record */
        if (psub->flnk.type==DB_LINK) dbScanPassive(psub->flnk.value.db_link.pdbAddr);
        psub->pact = FALSE;
        return(0);
}

static void alarm(psub)
    struct subRecord	*psub;
{
	float	ftemp;

        /* if difference is not > hysterisis don't bother */
        ftemp = psub->lalm - psub->val;
        if(ftemp<0.0) ftemp = -ftemp;
        if (ftemp < psub->hyst) return;

        /* alarm condition hihi */
        if (psub->nsev<psub->hhsv){
                if (psub->val > psub->hihi){
                        psub->lalm = psub->val;
                        psub->nsta = HIHI_ALARM;
                        psub->nsev = psub->hhsv;
                        return;
                }
        }

        /* alarm condition lolo */
        if (psub->nsev<psub->llsv){
                if (psub->val < psub->lolo){
                        psub->lalm = psub->val;
                        psub->nsta = LOLO_ALARM;
                        psub->nsev = psub->llsv;
                        return;
                }
        }

        /* alarm condition high */
        if (psub->nsev<psub->hsv){
                if (psub->val > psub->high){
                        psub->lalm = psub->val;
                        psub->nsta = HIGH_ALARM;
                        psub->nsev =psub->hsv;
                        return;
                }
        }
        /* alarm condition lolo */
        if (psub->nsev<psub->lsv){
                if (psub->val < psub->low){
                        psub->lalm = psub->val;
                        psub->nsta = LOW_ALARM;
                        psub->nsev = psub->lsv;
                        return;
                }
        }
        return;
}

static void monitor(psub)
    struct subRecord	*psub;
{
	unsigned short	monitor_mask;
	float		delta;
        short           stat,sevr,nsta,nsev;
	float           *pnew;
	float           *pprev;
	int             i;

        /* get previous stat and sevr  and new stat and sevr*/
        stat=psub->stat;
        sevr=psub->sevr;
        nsta=psub->nsta;
        nsev=psub->nsev;
        /*set current stat and sevr*/
        psub->stat = nsta;
        psub->sevr = nsev;
        psub->nsta = 0;
        psub->nsev = 0;

        /* anyone waiting for an event on this record */
        if (psub->mlis.count == 0) return;

        /* Flags which events to fire on the value field */
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
        if(delta<0.0) delta = 0.0;
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
	for(i=0, pnew=&psub->a, pprev=&psub->la; i<6; i++, pnew++, pprev++) {
		if(*pnew != *pprev) {
			db_post_events(psub,pnew,monitor_mask|DBE_VALUE);
			*pprev = *pnew;
		}
	}
        return;
}

static void fetch_values(psub)
struct subRecord *psub;
{
        struct link     *plink; /* structure of the link field  */
        float           *pvalue;
        long            options,nRequest;
        int             i;

        for(i=0, plink=&psub->inpa, pvalue=&psub->a; i<6; i++, plink++, pvalue++) {
                if(plink->type!=DB_LINK) continue;
                options=0;
                nRequest=1;
                (void)dbGetLink(&plink->value.db_link,psub,DBR_FLOAT,
                        pvalue,&options,&nRequest);
        }
        return;
}

static long do_sub(psub)
struct subRecord *psub;  /* pointer to subroutine record  */
{
	long	status;
	FUNCPTR	psubroutine;


	/* call the subroutine */
	psubroutine = (FUNCPTR)(psub->sadr);
	if(psubroutine==NULL) {
		if(psub->nsev<MAJOR_ALARM) {
			psub->nsta = BAD_SUB_ALARM;
			psub->nsev = MAJOR_ALARM;
		}
		return(0);
	}
	status = psubroutine(psub);
	if(status < 0){
		if (psub->nsev<psub->brsv){
			psub->nsta = SOFT_ALARM;
			psub->nsev = psub->brsv;
		}
	}
	return(status);
}
