/* recPid.c */
/* base/src/rec  $Id$ */

/* recPid.c - Record Support Routines for Pid records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            5-19-89 
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
 * .01  10-15-90	mrk	changes for new record support
 * .02  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .03  02-05-92	jba	Changed function arguments from paddr to precord 
 * .04  02-28-92        jba     Changed get_precision,get_graphic_double,get_control_double
 * .05  02-28-92	jba	ANSI C changes
 * .06  06-02-92        jba     changed graphic/control limits for hihi,high,low,lolo
 * .07  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .08  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .09  07-21-92        jba     changed alarm limits for non val related fields
 * .10  08-06-92        jba     New algorithm for calculating analog alarms
 * .11  09-10-92        jba     modified fetch of VAL from STPL to call recGblGetLinkValue
 * .12  03-29-94        mcn     Converted to fast links
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>
/*since tickLib is not defined just define tickGet*/
unsigned long tickGet();

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbEvent.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<recSup.h>
#define GEN_SIZE_OFFSET
#include	<pidRecord.h>
#undef  GEN_SIZE_OFFSET

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
#define get_value NULL
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

struct rset pidRSET={
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
static void monitor();
static long do_pid();


static long init_record(ppid,pass)
        struct pidRecord	*ppid;
        int pass;
{
        if (pass==0) return(0);
        /* initialize the setpoint for constant setpoint */
        if (ppid->stpl.type == CONSTANT){
	    if(recGblInitConstantLink(&ppid->stpl,DBF_FLOAT,&ppid->val))
                ppid->udf = FALSE;
	}
	return(0);
}

static long process(ppid)
	struct pidRecord	*ppid;
{
	long		 status;

	ppid->pact = TRUE;
	status=do_pid(ppid);
	if(status==1) {
		ppid->pact = FALSE;
		return(0);
	}
	recGblGetTimeStamp(ppid);
	alarm(ppid);
	monitor(ppid);
	recGblFwdLink(ppid);
	ppid->pact=FALSE;
	return(status);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    strncpy(units,ppid->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    *precision = ppid->prec;
    if(paddr->pfield == (void *)&ppid->val
    || paddr->pfield == (void *)&ppid->cval) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}


static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    if(paddr->pfield==(void *)&ppid->val
    || paddr->pfield==(void *)&ppid->hihi
    || paddr->pfield==(void *)&ppid->high
    || paddr->pfield==(void *)&ppid->low
    || paddr->pfield==(void *)&ppid->lolo
    || paddr->pfield==(void *)&ppid->p
    || paddr->pfield==(void *)&ppid->i
    || paddr->pfield==(void *)&ppid->d
    || paddr->pfield==(void *)&ppid->cval){
        pgd->upper_disp_limit = ppid->hopr;
        pgd->lower_disp_limit = ppid->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    if(paddr->pfield==(void *)&ppid->val
    || paddr->pfield==(void *)&ppid->hihi
    || paddr->pfield==(void *)&ppid->high
    || paddr->pfield==(void *)&ppid->low
    || paddr->pfield==(void *)&ppid->lolo
    || paddr->pfield==(void *)&ppid->p
    || paddr->pfield==(void *)&ppid->i
    || paddr->pfield==(void *)&ppid->d
    || paddr->pfield==(void *)&ppid->cval){
        pcd->upper_ctrl_limit = ppid->hopr;
        pcd->lower_ctrl_limit = ppid->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}
static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct pidRecord	*ppid=(struct pidRecord *)paddr->precord;

    if(paddr->pfield==(void *)&ppid->val){
         pad->upper_alarm_limit = ppid->hihi;
         pad->upper_warning_limit = ppid->high;
         pad->lower_warning_limit = ppid->low;
         pad->lower_alarm_limit = ppid->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void alarm(ppid)
    struct pidRecord	*ppid;
{
	double		val;
	float		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if(ppid->udf == TRUE ){
 		recGblSetSevr(ppid,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = ppid->hihi; lolo = ppid->lolo; high = ppid->high; low = ppid->low;
	hhsv = ppid->hhsv; llsv = ppid->llsv; hsv = ppid->hsv; lsv = ppid->lsv;
	val = ppid->val; hyst = ppid->hyst; lalm = ppid->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(ppid,HIHI_ALARM,ppid->hhsv)) ppid->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(ppid,LOLO_ALARM,ppid->llsv)) ppid->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(ppid,HIGH_ALARM,ppid->hsv)) ppid->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(ppid,LOW_ALARM,ppid->lsv)) ppid->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	ppid->lalm = val;
	return;
}

static void monitor(ppid)
    struct pidRecord	*ppid;
{
	unsigned short	monitor_mask;
	float		delta;

        monitor_mask = recGblResetAlarms(ppid);
        /* check for value change */
        delta = ppid->mlst - ppid->val;
        if(delta<0.0) delta = -delta;
        if (delta > ppid->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                ppid->mlst = ppid->val;
        }
        /* check for archive change */
        delta = ppid->alst - ppid->val;
        if(delta<0.0) delta = -delta;
        if (delta > ppid->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                ppid->alst = ppid->val;
        }

        /* send out all monitors  for value changes*/
        if (monitor_mask){
                db_post_events(ppid,&ppid->val,monitor_mask);
        }
	delta = ppid->odm - ppid->dm;
	if(delta<0.0) delta = -delta;
	if(delta > ppid->odel) {
		ppid->odm = ppid->dm;
		monitor_mask = DBE_LOG|DBE_VALUE;
		db_post_events(ppid,&ppid->dm,monitor_mask);
		db_post_events(ppid,&ppid->p,monitor_mask);
		db_post_events(ppid,&ppid->i,monitor_mask);
		db_post_events(ppid,&ppid->d,monitor_mask);
		db_post_events(ppid,&ppid->ct,monitor_mask);
		db_post_events(ppid,&ppid->dt,monitor_mask);
		db_post_events(ppid,&ppid->err,monitor_mask);
		db_post_events(ppid,&ppid->derr,monitor_mask);
	}
        return;
}

/* A discrete form of the PID algorithm is as follows
 * M(n) = KP*(E(n) + KI*SUMi(E(i)*dT(i))
 *		   + KD*(E(n) -E(n-1))/dT(n) + Mr
 * where
 *	M(n)	Value of manipulated variable at nth sampling instant
 *	KP,KI,KD Proportional, Integral, and Differential Gains
 *		NOTE: KI is inverse of normal definition of KI
 *	E(n)	Error at nth sampling instant
 *	SUMi	Sum from i=0 to i=n
 *	dT(n)	Time difference between n-1 and n
 *	Mr midrange adjustment
 *
 * Taking first difference yields
 * delM(n) = KP*((E(n)-E(n-1)) + E(n)*dT(n)*KI
 *		+ KD*((E(n)-E(n-1))/dT(n) - (E(n-1)-E(n-2))/dT(n-1))
 * or using variables defined in following
 * dm = kp*(de + e*dt*ki + kd*(de/dt - dep/dtp)
 */

static long do_pid(ppid)
struct pidRecord     *ppid;
{
	long		status;
	unsigned long	ctp;	/*clock ticks previous	*/
	unsigned long	ct;	/*clock ticks		*/
	float		cval;	/*actual value		*/
	float		val;	/*desired value(setpoint)*/
	float		dt;	/*delta time (seconds)	*/
	float		dtp;	/*previous dt		*/
	float		kp,ki,kd;/*gains		*/
	float		e;	/*error			*/
	float		ep;	/*previous error	*/
	float		de;	/*change in error	*/
	float		dep;	/*prev change in error	*/
	float		dm;	/*change in manip variable */
	float		p;	/*proportional contribution*/
	float		i;	/*integral contribution*/
	float		d;	/*derivative contribution*/

        /* fetch the controlled value */
        if (ppid->cvl.type == CONSTANT) { /* nothing to control*/
                if (recGblSetSevr(ppid,SOFT_ALARM,INVALID_ALARM)) return(0);
	}
        if (dbGetLink(&ppid->cvl,DBR_FLOAT,&cval,0,0)) {
                recGblSetSevr(ppid,LINK_ALARM,INVALID_ALARM);
                return(0);
        }
        /* fetch the setpoint */
        if(ppid->smsl == CLOSED_LOOP){
        	status = dbGetLink(&(ppid->stpl),DBR_FLOAT,
                                           &(ppid->val),0,0);

                if (RTN_SUCCESS(status)) ppid->udf=FALSE;
        }
	val = ppid->val;
	if (ppid->udf == TRUE ) {
                recGblSetSevr(ppid,UDF_ALARM,INVALID_ALARM);
                return(0);
	}

	/* compute time difference and make sure it is large enough*/
	ctp = ppid->ct;
	ct = tickGet();
	if(ctp==0) {/*this happens the first time*/
		dt=0.0;
	} else {
		if(ctp<ct) {
			dt = (float)(ct-ctp);
		}else { /* clock has overflowed */
			dt = (unsigned long)(0xffffffff) - ctp;
			dt = dt + ct + 1;
		}
		dt = dt/vxTicksPerSecond;
		if(dt<ppid->mdt) return(1);
	}
	/* get the rest of values needed */
	dtp = ppid->dt;
	kp = ppid->kp;
	ki = ppid->ki/60.0;
	kd = ppid->kd/60.0;
	ep = ppid->err;
	dep = ppid->derr;
	e = val - cval;
	de = e - ep;
	p = kp*de;
	i = kp*e*dt*ki;
	if(dtp>0.0 && dt>0.0) d = kp*kd*(de/dt - dep/dtp);
	else d = 0.0;
	dm = p + i + d;
	/* update record*/
	ppid->ct  = ct;
	ppid->dt   = dt;
	ppid->err  = e;
	ppid->derr = de;
	ppid->cval  = cval;
	ppid->dm  = dm;
	ppid->p  = p;
	ppid->i  = i;
	ppid->d  = d;
	return(0);
}
