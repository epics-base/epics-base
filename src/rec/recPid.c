
/* recPid.c */
/* share/src/rec $Id$ */

/* recPid.c - Record Support Routines for Pid records
 *
 * Author: 	Bob Dalesio
 * Date:        05-19-89
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
 * .01  10-15-90	mrk	changes for new record support
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<pidRecord.h>

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

struct rset pidRSET={
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
    struct pidRecord	*ppid=(struct pidRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    if(fprintf(fp,"VAL  %-12.4G\n",ppid->val)) return(-1);
    if(recGblReportLink(fp,"INP ",&(ppid->inp))) return(-1);
    if(fprintf(fp,"PREC %d\n",ppid->prec)) return(-1);
    if(recGblReportCvtChoice(fp,"LINR",ppid->linr)) return(-1);
    if(fprintf(fp,"EGUF %-12.4G EGUL %-12.4G  EGU %-8s\n",
	ppid->eguf,ppid->egul,ppid->egu)) return(-1);
    if(fprintf(fp,"HOPR %-12.4G LOPR %-12.4G\n",
	ppid->hopr,ppid->lopr)) return(-1);
    if(recGblReportLink(fp,"FLNK",&(ppid->flnk))) return(-1);
    if(fprintf(fp,"HIHI %-12.4G HIGH %-12.4G  LOW %-12.4G LOLO %-12.4G\n",
	ppid->hihi,ppid->high,ppid->low,ppid->lolo)) return(-1);
    if(recGblReportGblChoice(fp,ppid,"HHSV",ppid->hhsv)) return(-1);
    if(recGblReportGblChoice(fp,ppid,"HSV ",ppid->hsv)) return(-1);
    if(recGblReportGblChoice(fp,ppid,"LSV ",ppid->lsv)) return(-1);
    if(recGblReportGblChoice(fp,ppid,"LLSV",ppid->llsv)) return(-1);
    if(fprintf(fp,"HYST %-12.4G ADEL %-12.4G MDEL %-12.4G ESLO %-12.4G\n",
	ppid->hyst,ppid->adel,ppid->mdel,ppid->eslo)) return(-1);
    if(fprintf(fp,"ACHN %d\n", ppid->achn)) return(-1);
    if(fprintf(fp,"LALM %-12.4G ALST %-12.4G MLST %-12.4G\n",
	ppid->lalm,ppid->alst,ppid->mlst)) return(-1);
    return(0);
}
^L
static pid_intervals[] = {1,5,4,3,2,1,1,1};

static long init_record(ppid)
    struct pidRecord     *ppid;
{
        /* get the interval based on the scan type */
        ppid->intv = pid_intervals[ppid->scan];

        /* initialize the setpoint for constant setpoint */
        if (ppid->stpl.type == CONSTANT)
                ppid->val = ppid->stpl.value.value;
	return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct pidRecord	*ppid=(struct pidRecord *)(paddr->precord);
	long		 status;

	ppid->pact = TRUE;
	status=do_pid(ppid);
	if(status == -1) {
		if(ppid->stat != READ_ALARM) {/* error. set alarm condition */
			ppid->stat = READ_ALARM;
			ppid->sevr = MAJOR_ALARM;
			ppid->achn=1;
		}
	}else if(status!=0) return(status);
	else if(ppid->stat == READ_ALARM || ppid->stat ==  HW_LIMIT_ALARM) {
		ppid->stat = NO_ALARM;
		ppid->sevr = NO_ALARM;
		ppid->achn=1;
	}
	if(status==0) do_pidect(ppid);

	/* check for alarms */
	alarm(ppid);


	/* check event list */
	monitor(ppid);

	/* process the forward scan link record */
	if (ppid->flnk.type==DB_LINK) dbScanPassive(&ppid->flnk.value);

	ppid->pact=FALSE;
	return(status);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    *precision = ppid->prec;
    return(0L);
}

static long get_value(ppid,pvdes)
    struct pidRecord		*ppid;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_FLOAT;
    pvdes->no_elements=1;
    (float *)(pvdes->pvalue) = &ppid->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    strncpy(units,ppid->egu,sizeof(ppid->egu));
    return(0L);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    pgd->upper_disp_limit = ppid->hopr;
    pgd->lower_disp_limit = ppid->lopr;
    pgd->upper_alarm_limit = ppid->hihi;
    pgd->upper_warning_limit = ppid->high;
    pgd->lower_warning_limit = ppid->low;
    pgd->lower_alarm_limit = ppid->lolo;
    return(0L);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    pcd->upper_ctrl_limit = ppid->hopr;
    pcd->lower_ctrl_limit = ppid->lopr;
    return(0L);
}

static void alarm(ppid)
    struct pidRecord	*ppid;
{
	float	ftemp;

	/* check for a hardware alarm */
	if (ppid->stat == READ_ALARM) return(0);

	/* if in alarm and difference is not > hysterisis don't bother */
	if (ppid->stat != NO_ALARM){
		ftemp = ppid->lalm - ppid->val;
		if(ftemp<0.0) ftemp = -ftemp;
		if (ftemp < ppid->hyst) return;
	}

	/* alarm condition hihi */
	if (ppid->hhsv != NO_ALARM){
		if (ppid->val > ppid->hihi){
			ppid->lalm = ppid->val;
			if (ppid->stat != HIHI_ALARM){
				ppid->stat = HIHI_ALARM;
				ppid->sevr = ppid->hhsv;
				ppid->achn = 1;
			}
			return;
		}
	}

	/* alarm condition lolo */
	if (ppid->llsv != NO_ALARM){
		if (ppid->val < ppid->lolo){
			ppid->lalm = ppid->val;
			if (ppid->stat != LOLO_ALARM){
				ppid->stat = LOLO_ALARM;
				ppid->sevr = ppid->llsv;
				ppid->achn = 1;
			}
			return;
		}
	}

	/* alarm condition high */
	if (ppid->hsv != NO_ALARM){
		if (ppid->val > ppid->high){
			ppid->lalm = ppid->val;
			if (ppid->stat != HIGH_ALARM){
				ppid->stat = HIGH_ALARM;
				ppid->sevr =ppid->hsv;
				ppid->achn = 1;
			}
			return;
		}
	}

	/* alarm condition lolo */
	if (ppid->lsv != NO_ALARM){
		if (ppid->val < ppid->low){
			ppid->lalm = ppid->val;
			if (ppid->stat != LOW_ALARM){
				ppid->stat = LOW_ALARM;
				ppid->sevr = ppid->lsv;
				ppid->achn = 1;
			}
			return;
		}
	}

	/* no alarm */
	if (ppid->stat != NO_ALARM){
		ppid->stat = NO_ALARM;
		ppid->sevr = NO_ALARM;
		ppid->achn = 1;
	}

	return;
}

static void monitor(ppid)
    struct pidRecord	*ppid;
{
	unsigned short	monitor_mask;
	float		delta;

	/* anyone waiting for an event on this record */
	if (ppid->mlis.count == 0) return;

	/* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (ppid->achn){
		/* post events for alarm condition change and value change */
		monitor_mask = DBE_ALARM | DBE_VALUE | DBE_LOG;

		/* post stat and sevr fields */
		db_post_events(ppid,&ppid->stat,DBE_VALUE);
		db_post_events(ppid,&ppid->sevr,DBE_VALUE);

		/* update last value monitored */
		ppid->mlst = ppid->val;

	/* check for value change */
	}else{
		delta = ppid->mlst - ppid->val;
		if(delta<0.0) delta = -delta;
		if (delta > ppid->mdel) {
			/* post events for value change */
			monitor_mask = DBE_VALUE;

			/* update last value monitored */
			ppid->mlst = ppid->val;
		}
	}

	/* check for archive change */
	delta = ppid->alst - ppid->val;
	if(delta<0.0) delta = 0.0;
	if (delta > ppid->adel) {
		/* post events on value field for archive change */
		monitor_mask |= DBE_LOG;

		/* update last archive value monitored */
		ppid->alst = ppid->val;
	}

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(ppid,&ppid->val,monitor_mask);
		db_post_events(ppid,&ppid->rval,monitor_mask);
	}
	return;
}

static long do_pid(ppid)
register struct pid     *ppid;
{
        /* fetch the controlled value */
        if (ppid->cvl.type != DB_LINK) return(-1);      /* nothing to control */
        if (db_fetch(&ppid->cvl.value,&ppid->cval) < 0){
                if (ppid->stat != READ_ALARM){
                        ppid->stat = READ_ALARM;
                        ppid->sevr = MAJOR;
                        ppid->achn = 1;
                        return(-1);
                }
        }

/* rate of change on the setpoint? */
        /* fetch the setpoint */
        if ((ppid->stpl.type == DB_LINK) && (ppid->smsl == CLOSED_LOOP)){
                if (db_fetch(&ppid->stpl.value,&ppid->val) < 0){
                        if (ppid->stat != READ_ALARM){
                                ppid->stat = READ_ALARM;
                                ppid->sevr = MAJOR;
                                ppid->achn = 1;
                                return(-1);
                        }
                }
        }

        /* reset the integral term when the setpoint changes */
        if (ppid->val != ppid->lval){
                ppid->lval = ppid->val;
                ppid->inte = 0;
        }

        /* determine the error */
        ppid->lerr = ppid->err;
        ppid->err = ppid->val - ppid->cval;
        ppid->derr = ppid->err - ppid->lerr;

        /* determine the proportional contribution */
        ppid->prop = ppid->err * ppid->kp;

        /* determine the integral contribution */
        ppid->inte += ppid->err;
        ppid->intg = ppid->inte * ppid->ki * ppid->intv;

        /* determine the derivative contribution */
/* we are implementing the derivativa term on error - do we want value also? */
        ppid->der = (ppid->derr * ppid->kd) / ppid->intv;

        /* delta output contributions weighted by the proportional constant */
        ppid->out = ppid->intg + ppid->der + ppid->prop;

	return(0);
}
