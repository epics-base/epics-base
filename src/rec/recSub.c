
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

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<subRecord.h>

/* Create RSET - Record Support Entry Table*/
long report();
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

static long report(fp,paddr)
    FILE	  *fp;
    struct dbAddr *paddr;
{
    struct subRecord	*psub=(struct subRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    if(fprintf(fp,"VAL  %-12.4G\n",psub->val)) return(-1);
    if(recGblReportLink(fp,"INPA ",&(psub->inpa))) return(-1);
    if(recGblReportLink(fp,"INPB ",&(psub->inpb))) return(-1);
    if(recGblReportLink(fp,"INPC ",&(psub->inpc))) return(-1);
    if(recGblReportLink(fp,"INPD ",&(psub->inpd))) return(-1);
    if(recGblReportLink(fp,"INPE ",&(psub->inpe))) return(-1);
    if(recGblReportLink(fp,"INPF ",&(psub->inpf))) return(-1);
    if(recGblReportLink(fp,"FLNK",&(psub->flnk))) return(-1);
    if(fprintf(fp,"A  %-12.4G\n",psub->a)) return(-1);
    if(fprintf(fp,"B  %-12.4G\n",psub->b)) return(-1);
    if(fprintf(fp,"C  %-12.4G\n",psub->c)) return(-1);
    if(fprintf(fp,"D  %-12.4G\n",psub->d)) return(-1);
    if(fprintf(fp,"E  %-12.4G\n",psub->e)) return(-1);
    if(fprintf(fp,"F  %-12.4G\n",psub->f)) return(-1);
    if(fprintf(fp,"PREC %d  EGU %-8s\n",psub->prec,psub->egu)) return(-1);
    if(fprintf(fp,"HOPR %-12.4G LOPR %-12.4G\n",
	psub->hopr,psub->lopr)) return(-1);
    if(fprintf(fp,"HIHI %-12.4G HIGH %-12.4G  LOW %-12.4G LOLO %-12.4G\n",
	psub->hihi,psub->high,psub->low,psub->lolo)) return(-1);
    if(recGblReportGblChoice(fp,psub,"HHSV",psub->hhsv)) return(-1);
    if(recGblReportGblChoice(fp,psub,"HSV ",psub->hsv)) return(-1);
    if(recGblReportGblChoice(fp,psub,"LSV ",psub->lsv)) return(-1);
    if(recGblReportGblChoice(fp,psub,"LLSV",psub->llsv)) return(-1);
    if(fprintf(fp,"HYST %-12.4G ADEL %-12.4G MDEL %-12.4G\n",
	psub->hyst,psub->adel,psub->mdel)) return(-1);
    if(fprintf(fp,"ACHN %d\n",psub->achn)) return(-1);
    if(fprintf(fp,"LALM %-12.4G ALST %-12.4G MLST %-12.4G\n",
	psub->lalm,psub->alst,psub->mlst)) return(-1);
    if(fprintf(fp,"CALC %s\n",psub->sub)) return(-1);
    return(0);
}

static long init_record(psub)
    struct subRecord	*psub;
{
    long status;
    FUNCPTR	psubroutine;
    char	sub_type;
    char	temp[40];
    short	ret;

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
    if ((ret < 0) || ((sub_type & N_TEXT) == 0)){
	psub->stat = BAD_SUB_ALARM;
	psub->sevr = MAJOR;
	psub->achn = 1;
	monitor_sub(psub);
	return(-1);
    }

    /* invoke the initialization subroutine */
    (long)psubroutine = psub->sadr;
    if (psubroutine(psub,sub_callback,
      &psub->a,&psub->b,&psub->c,&psub->d,&psub->e,&psub->f,
      &psub->val) < 0){
    	if (psub->brsv != NO_ALARM){
    		psub->stat = RETURN_ALARM;
    		psub->sevr = psub->brsv;
    		psub->achn = 1;
    		monitor_sub(psub);
    		return(-1);
    	}
    }

    /* convert the subroutine name to an address and type */
    /* convert the initialization subroutine name  */
    temp[0] = 0;			/* all global variables start with _ */
    if (psub->snam[0] != '_'){
    	strcpy(temp,"_");
    }
    strcat(temp,psub->snam);
    ret = symFindByName(sysSymTbl,temp,&psub->sadr,&sub_type);
    if ((ret < 0) || ((sub_type & N_TEXT) == 0)){
    	psub->styp = sub_type;
    	psub->stat = BAD_SUB_ALARM;
    	psub->sevr = MAJOR;
    	psub->achn = 1;
    	monitor_sub(psub);
    	return(-1);
    }else if (psub->stat != NO_ALARM){
    	psub->sevr = psub->stat = NO_ALARM;
    	psub->achn = 1;
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
    return(0L);
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
    return(0L);
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
    return(0L);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    pcd->upper_ctrl_limit = psub->hopr;
    pcd->lower_ctrl_limit = psub->lopr;
    return(0L);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct subRecord	*psub=(struct subRecord *)(paddr->precord);
	struct subdset	*pdset = (struct subdset *)(psub->dset);
	long		 status;

	psub->achn = 0;
	/* read inputs  */
	if (fetch_values(psub) < 0){
		if (psub->stat != READ_ALARM){
			psub->stat = READ_ALARM;
			psub->sevr = MAJOR_ALARM;
			psub->achn = 1;
			monitor_sub(psub);
		}
		psub->pact = 0;
		return(0);
	}else{
		if (psub->stat == READ_ALARM){
			psub->stat = NO_ALARM;
			psub->sevr = NO_ALARM;
			psub->achn = 1;
		}
	}

	/* perform subulation */
	if (do_sub(psub) < 0){
		if (psub->stat != CALC_ALARM){
			psub->stat = CALC_ALARM;
			psub->sevr = MAJOR_ALARM;
			psub->achn = 1;
			monitor_sub(psub);
		}
		psub->pact = 0;
		return;
	}else{
		if (psub->stat == CALC_ALARM){
			psub->stat = NO_ALARM;
			psub->sevr = NO_ALARM;
			psub->achn = 1;
		}
	}

	/* check for alarms */
	alarm(psub);


	/* check event list */
	if(!psub->disa) status = monitor(psub);

	/* process the forward scan link record */
	if (psub->flnk.type==DB_LINK) dbScanPassive(&psub->flnk.value);

	psub->pact=FALSE;
	return(status);
}

/*
 * FETCH_VALUES
 *
 * fetch the values for the variables in the subulation
 */
static fetch_values(psub)
struct subRecord *psub;
{
	short	status;

	/* note - currently not using alarm status */
	status = 0;
	status |= get_sub_inp(&psub->inpa,&psub->a);
	status |= get_sub_inp(&psub->inpb,&psub->b);
	status |= get_sub_inp(&psub->inpc,&psub->c);
	status |= get_sub_inp(&psub->inpd,&psub->d);
	status |= get_sub_inp(&psub->inpe,&psub->e);
	status |= get_sub_inp(&psub->inpf,&psub->f);
	return(status);
}

/*
 *	GET_CALC_INPUT
 *
 *	return an input value
 */
static get_sub_inp(plink,pvalue)
struct link	*plink;	/* structure of the link field  */
float		*pvalue;
{
	float	float_value;

	/* database link */
	if (plink->type == DB_LINK){
		if (dbGetLink(&plink->value.db_link,DBR_FLOAT,&float_value,1) < 0)
			return(-1);
		*pvalue = float_value;

	/* constant */
	}else if (plink->type == CONSTANT){
		;
	/* illegal link type */
	}else{
		return(-1);
	}
	return(0);

}

static long alarm(psub)
    struct subRecord	*psub;
{
	float	ftemp;

	/* if in alarm and difference is not > hysterisis don't bother */
	if (psub->stat != NO_ALARM){
		ftemp = psub->lalm - psub->val;
		if(ftemp<0.0) ftemp = -ftemp;
		if (ftemp < psub->hyst) return(0);
	}

	/* alarm condition hihi */
	if (psub->hhsv != NO_ALARM){
		if (psub->val > psub->hihi){
			psub->lalm = psub->val;
			if (psub->stat != HIHI_ALARM){
				psub->stat = HIHI_ALARM;
				psub->sevr = psub->hhsv;
				psub->achn = 1;
			}
			return(0);
		}
	}

	/* alarm condition lolo */
	if (psub->llsv != NO_ALARM){
		if (psub->val < psub->lolo){
			psub->lalm = psub->val;
			if (psub->stat != LOLO_ALARM){
				psub->stat = LOLO_ALARM;
				psub->sevr = psub->llsv;
				psub->achn = 1;
			}
			return(0);
		}
	}

	/* alarm condition high */
	if (psub->hsv != NO_ALARM){
		if (psub->val > psub->high){
			psub->lalm = psub->val;
			if (psub->stat != HIGH_ALARM){
				psub->stat = HIGH_ALARM;
				psub->sevr =psub->hsv;
				psub->achn = 1;
			}
			return(0);
		}
	}

	/* alarm condition lolo */
	if (psub->lsv != NO_ALARM){
		if (psub->val < psub->low){
			psub->lalm = psub->val;
			if (psub->stat != LOW_ALARM){
				psub->stat = LOW_ALARM;
				psub->sevr = psub->lsv;
				psub->achn = 1;
			}
			return(0);
		}
	}

	/* no alarm */
	if (psub->stat != NO_ALARM){
		psub->stat = NO_ALARM;
		psub->sevr = NO_ALARM;
		psub->achn = 1;
	}

	return(0);
}

static long monitor(psub)
    struct subRecord	*psub;
{
	unsigned short	monitor_mask;
	float		delta;

	/* anyone wsubting for an event on this record */
	if (psub->mlis.count == 0) return(0L);

	/* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (psub->achn){
		/* post events for alarm condition change and value change */
		monitor_mask = DBE_ALARM | DBE_VALUE | DBE_LOG;

		/* post stat and sevr fields */
		db_post_events(psub,&psub->stat,DBE_VALUE);
		db_post_events(psub,&psub->sevr,DBE_VALUE);

		/* update last value monitored */
		psub->mlst = psub->val;

	/* check for value change */
	}else{
		delta = psub->mlst - psub->val;
		if(delta<0.0) delta = -delta;
		if (delta > psub->mdel) {
			/* post events for value change */
			monitor_mask = DBE_VALUE;

			/* update last value monitored */
			psub->mlst = psub->val;
		}
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
	return(0L);
}

process_sub(psub)
register struct sub	*psub; 	 /* pointer to subroutine record  */
{
	psub->achn = 0;

	/* lock the record */
	if (psub->lock){
		if (psub->lcnt >= MAX_LOCK){
			psub->stat = SCAN_ALARM;
			psub->sevr = MAJOR;
			psub->achn = 1;
			monitor_sub(psub);
		}else{
			psub->lcnt++;
		}
		return;
	}
	psub->lcnt = 0;
	psub->lock = 1;

	if (psub->init == 0){
		if (sub_init(psub) < 0){
			psub->lock = 0;
			return;
		}
	}

	/* check for valid subroutine connection */
	if (psub->stat == BAD_SUB_ALARM){
		psub->lock = 0;
		return;
	}

	/* perform the subroutine call */
	do_sub(psub);

	/* do post processing for synchronous routines */
	if (psub->rtcb == FALSE){
		sub_callback(psub);
	}
	/* unlock of record is done in sub_callback either directly (above) */
	/* or asynchronously through the callback mechanism		    */
}

/*
 * SUB_CALLBACK
 *
 * subroutine values returned handling -
 * either called immediately for fast routines or asynchronously for slow
 * routines (i.e. GPIB interfaces that require a wait)
 */
sub_callback(psub)
register struct sub	*psub; 	 /* pointer to subroutine record  */
{
	/* check for alarms */
	if ((psub->stat != READ_ALARM) && (psub->stat != RETURN_ALARM)
	  && (psub->stat != BAD_SUB_ALARM))
		alarm_sub(psub);

	/* check for monitors */
	monitor_sub(psub);

        /* process the forward scan link record */
        if (psub->flnk.type == DB_LINK)
                db_scan(&psub->flnk.value);

	/* unlock the record */
	psub->lock = 0;
}

/*
 *	DO_SUB
 *
 *	invoke the subroutine
 */
static do_sub(psub)
register struct sub *psub;  /* pointer to subroutine record  */
{
	register short	status;
	register FUNCPTR	psubroutine;

	/* get the subroutine arguments */
	status = 0;
	if (psub->inpa.type == DB_LINK){
		if (db_fetch(&psub->inpa.value.db_link,&psub->a) < 0)
			status |= -1;
	}
	if (psub->inpb.type == DB_LINK){
		if (db_fetch(&psub->inpb.value.db_link,&psub->b) < 0)
			status |= -1;
	}
	if (psub->inpc.type == DB_LINK){
		if (db_fetch(&psub->inpc.value.db_link,&psub->c) < 0)
			status |= -1;
	}
	if (psub->inpd.type == DB_LINK){
		if (db_fetch(&psub->inpd.value.db_link,&psub->d) < 0)
			status |= -1;
	}
	if (psub->inpe.type == DB_LINK){
		if (db_fetch(&psub->inpe.value.db_link,&psub->e) < 0)
			status |= -1;
	}
	if (psub->inpf.type == DB_LINK){
		if (db_fetch(&psub->inpf.value.db_link,&psub->f) < 0)
			status |= -1;
	}
	if (status != 0){
		psub->stat = READ_ALARM;
		psub->sevr = MAJOR;
		psub->achn = 1;
		return;
	}

	/* call the subroutine */
	(long)psubroutine = psub->sadr;
	if (psubroutine(psub,sub_callback,
	  &psub->a,&psub->b,&psub->c,&psub->d,&psub->e,&psub->f,
	  &psub->val) < 0){
		if (psub->brsv != NO_ALARM){
			psub->stat = RETURN_ALARM;
			psub->sevr = psub->brsv;
			psub->achn = 1;
			return;
		}
	}else{
		if (psub->brsv != NO_ALARM){
			psub->stat = NO_ALARM;
			psub->sevr = NO_ALARM;
			psub->achn = 1;
			return;
		}
	}
	return;
}

/*
 * ALARM_SUB
 *
 * check the alarm condition
 */
static alarm_sub(psub)
register struct sub	*psub;
{
	register float	ftemp;

	/* if in alarm and difference is not > hysterisis don't bother */
	if (psub->stat != NO_ALARM){
		ftemp = psub->lalm - psub->val;
		if ( (ftemp < psub->hyst) && (ftemp > -psub->hyst))
			return;
	}

	/* alarm condition hihi */
	if (psub->hhsv != NO_ALARM){
		if (psub->val > psub->hihi){
			if (psub->stat != HIHI_ALARM){
				psub->stat = HIHI_ALARM;
				psub->sevr = psub->hhsv;
				psub->lalm = psub->val;
				psub->achn = 1;
			}
			return;
		}
	}

	/* alarm condition lolo */
	if (psub->llsv != NO_ALARM){
		if (psub->val < psub->lolo){
			if (psub->stat != LOLO_ALARM){
				psub->stat = LOLO_ALARM;
				psub->sevr = psub->llsv;
				psub->lalm = psub->val;
				psub->achn = 1;
			}
			return;
		}
	}

	/* alarm condition high */
	if (psub->hsv != NO_ALARM){
		if (psub->val > psub->high){
			if (psub->stat != HIGH_ALARM){
				psub->stat = HIGH_ALARM;
				psub->sevr = psub->hsv;
				psub->lalm = psub->val;
				psub->achn = 1;
			}
			return;
		}
	}

	/* alarm condition low */
	if (psub->lsv != NO_ALARM){
		if (psub->val < psub->low){
			if (psub->stat != LOW_ALARM){
				psub->stat = LOW_ALARM;
				psub->sevr = psub->lsv;
				psub->lalm = psub->val;
				psub->achn = 1;
			}
			return;
		}
	}

	/* no alarm */
	if (psub->stat != NO_ALARM){
		psub->stat = NO_ALARM;
		psub->sevr = NO_ALARM;
		psub->achn = 1;
	}

	return;
}

/*
 * MONITOR_SUB
 *
 * process subroutine record monitors
 */
static monitor_sub(psub)
register struct sub      *psub;   /* pointer to the subroutine record */
{
        register unsigned short monitor_mask;
        register float          delta;

        /* anyone waiting for an event on this record */
        if (psub->mqct == 0) return;

	/* check monitors for each of the argument fields */
	if (psub->la != psub->a){
                db_post_events(psub,&psub->a,DBE_VALUE);
		psub->la = psub->a;
	}
	if (psub->lb != psub->b){
                db_post_events(psub,&psub->b,DBE_VALUE);
		psub->la = psub->b;
	}
	if (psub->lc != psub->c){
                db_post_events(psub,&psub->c,DBE_VALUE);
		psub->lc = psub->c;
	}
	if (psub->ld != psub->d){
                db_post_events(psub,&psub->d,DBE_VALUE);
		psub->ld = psub->d;
	}
	if (psub->le != psub->e){
                db_post_events(psub,&psub->e,DBE_VALUE);
		psub->le = psub->e;
	}
	if (psub->lf != psub->f){
                db_post_events(psub,&psub->f,DBE_VALUE);
		psub->lf = psub->f;
	}

        /* Flags which events to fire on the value field */
        monitor_mask = 0;

        /* alarm condition changed this scan */
        if (psub->achn != 0){
                /* post events for alarm condition change and value change */
                monitor_mask = DBE_ALARM | DBE_VALUE;

                /* post stat and sevr fields */
                db_post_events(psub,&psub->stat,DBE_VALUE);
                db_post_events(psub,&psub->sevr,DBE_VALUE);

                /* update last value monitored */
                psub->mlst = psub->val;

        /* check for value change */
        }else{
                delta = psub->mlst - psub->val;
                if ((delta > psub->mdel) || (delta < -psub->mdel)){
                        /* post events for value change */
                        monitor_mask = DBE_VALUE;

                        /* update last value monitored */
                        psub->mlst = psub->val;
                }
        }

        /* check for archive change */
        delta = psub->alst - psub->val;
        if ((delta > psub->adel) || (delta < -psub->adel)){
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;

                /* update last archive value monitored */
                psub->alst = psub->val;
        }
 
        /* send out monitors connected to the value field */
        if (monitor_mask)
                db_post_events(psub,&psub->val,monitor_mask);
}
