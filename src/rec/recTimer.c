
/* recTimer.c */
/* share/src/rec $Id$ */

/* recTimer.c - Record Support Routines for Timer records
 *
 * Author: 	Bob Dalesio
 * Date:        1-9-89
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
 * .01  01-20-89        lrd     fix vx includes
 * .02  02-06-89        lrd     add event post capability
 * .03  03-29-89        lrd     make hardware errors MAJOR
 *                              remove hw severity spec from database
 * .04  04-07-89        lrd     service monitors
 * .05  05-03-89        lrd     removed process mask from arg list
 * .06  05-03-89        lrd     modified to read the timing on startup
 * .07  05-03-89        lrd     read trigger delay from trigger origin record
 * .08  07-03-89        lrd     add processing a forward link
 * .09  08-15-89        lrd     add post events for timing pulse 1 fields
 * .10  10-15-90	mrk	extensible record and device support
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
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<timerRecord.h>

/* Create RSET - Record Support Entry Table*/
long report();
#define initialize NULL
long init_record();
long process();
#define special();
#define get_precision();
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_enum_str NULL
#define get_units();
#define get_graphic_double();
#define get_control_double();
#define get_enum_strs NULL

struct rset timerRSET={
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

/* because the driver does all the work just declare device support here*/
struct dset devMizar8310={4,NULL,NULL,NULL,NULL};
struct dset devDg535={4,NULL,NULL,NULL,NULL};
struct dset devVxiAt5Time={4,NULL,NULL,NULL,NULL};

static long report(fp,paddr)
    FILE	  *fp;
    struct dbAddr *paddr;
{
    struct timerRecord	*ptimer=(struct timerRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    if(fprintf(fp,"VAL  %-12.4G\n",ptimer->val)) return(-1);
    if(recGblReportLink(fp,"INP ",&(ptimer->inp))) return(-1);
    if(fprintf(fp,"PREC %d\n",ptimer->prec)) return(-1);
    if(recGblReportCvtChoice(fp,"LINR",ptimer->linr)) return(-1);
    if(fprintf(fp,"HOPR %-12.4G LOPR %-12.4G\n",
	ptimer->hopr,ptimer->lopr)) return(-1);
    if(recGblReportLink(fp,"FLNK",&(ptimer->flnk))) return(-1);
    if(fprintf(fp,"HIHI %-12.4G HIGH %-12.4G  LOW %-12.4G LOLO %-12.4G\n",
	ptimer->hihi,ptimer->high,ptimer->low,ptimer->lolo)) return(-1);
    if(recGblReportGblChoice(fp,ptimer,"HHSV",ptimer->hhsv)) return(-1);
    if(recGblReportGblChoice(fp,ptimer,"HSV ",ptimer->hsv)) return(-1);
    if(recGblReportGblChoice(fp,ptimer,"LSV ",ptimer->lsv)) return(-1);
    if(recGblReportGblChoice(fp,ptimer,"LLSV",ptimer->llsv)) return(-1);
    if(fprintf(fp,"HYST %-12.4G ADEL %-12.4G MDEL %-12.4G ESLO %-12.4G\n",
	ptimer->hyst,ptimer->adel,ptimer->mdel,ptimer->eslo)) return(-1);
    if(fprintf(fp,"RVAL 0x%-8X   ACHN %d\n",
	ptimer->rval,ptimer->achn)) return(-1);
    if(fprintf(fp,"LALM %-12.4G ALST %-12.4G MLST %-12.4G\n",
	ptimer->lalm,ptimer->alst,ptimer->mlst)) return(-1);
    return(0);
}

static long init_record(ptimer)
    struct timerRecord	*ptimer;
{
    long status;

    /* read to maintain time pulses over a restart */
    read_timer(ptimer);
    return(0);
}

static long get_value(ptimer,pvdes)
    struct timerRecord		*ptimer;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_SHORT;
    pvdes->no_elements=1;
    (short *)(pvdes->pvalue) = &ptimer->val;
    return(0);
}

extern int	post_event();

static long process(paddr)
    struct dbAddr	*paddr;
{
	struct timerRecord	*ptimer=(struct timerRecord *)(paddr->precord);
	long		 status;

	ptimer->pact=TRUE;
	ptimer->achn = 0;	/* init the alarm change flag */

	/* write the new value */
	write_timer(ptimer);

	/* need to post events for an alarm condition change */
	if ((ptimer->achn) && (ptimer->mlis.count!=0))
		db_post_events(ptimer,&ptimer->val,DBE_ALARM);

	if (ptimer->mlis.count!=0){
		db_post_events(ptimer,&ptimer->val,DBE_VALUE);
		db_post_events(ptimer,&ptimer->t1wd,DBE_VALUE);
		db_post_events(ptimer,&ptimer->t1ld,DBE_VALUE);
		db_post_events(ptimer,&ptimer->t1td,DBE_VALUE);
	}

	/* process the forward scan link record */
	if (ptimer->flnk.type == DB_LINK)
		dbScanPassive(&ptimer->flnk.value);

	/* unlock the record */
	ptimer->lock = 0;
	ptimer->pact=FALSE;
	return(0);
}

/*
 * These constants are indexed by the time units field in the timer record.
 * Values are converted to seconds.
 */
static double constants[] = {1000,1000000,1000000000,1000000000000};
/*
 * CONVERT_TIMER
 *
 */
static convert_timer(ptimer)
register struct timer	*ptimer;
{
	double	constant;

	/* check the tdisble bit */
	if (ptimer->tdis == 1){
		ptimer->t1dl = ptimer->t1wd = 0;
		ptimer->t2dl = ptimer->t2wd = 0;
		ptimer->t3dl = ptimer->t3wd = 0;
		ptimer->t4dl = ptimer->t4wd = 0;
		ptimer->t5dl = ptimer->t5wd = 0;
		return;
	}
	
	/* convert according to time units */
	constant = constants[ptimer->timu];

	/* timing pulse 1 */
	ptimer->t1dl = (ptimer->dut1 + ptimer->trdl) / constant; /* delay */
	ptimer->t1wd = ptimer->opw1 / constant; 		 /* width */
	ptimer->t1ld = ptimer->dut1 + ptimer->trdl;   /* leading edge delay */
	ptimer->t1td = ptimer->t1ld + ptimer->opw1;   /* trailing edge delay */

	/* timing pulse 2 */
	ptimer->t2dl = (ptimer->dut2 + ptimer->trdl) / constant; /* delay */
	ptimer->t2wd = ptimer->opw2 / constant; 	  	 /* width */
	ptimer->t2ld = ptimer->dut2 + ptimer->trdl;   /* leading edge delay */
	ptimer->t2td = ptimer->t2ld + ptimer->opw2;   /* trailing edge delay */

	/* timing pulse 3 */
	ptimer->t3dl = (ptimer->dut3 + ptimer->trdl) / constant; /* delay */
	ptimer->t3wd = ptimer->opw3 / constant; 	 	 /* width */
	ptimer->t3ld = ptimer->dut3 + ptimer->trdl;   /* leading edge delay */
	ptimer->t3td = ptimer->t3ld + ptimer->opw3;   /* trailing edge delay */

	/* timing pulse 4 */
	ptimer->t4dl = (ptimer->dut4 + ptimer->trdl) / constant; /* delay */
	ptimer->t4wd = ptimer->opw4 / constant;	    		 /* width */
	ptimer->t4ld = ptimer->dut4 + ptimer->trdl;   /* leading edge delay */
	ptimer->t4td = ptimer->t4ld + ptimer->opw4;   /* trailing edge delay */

	/* timing pulse 5 */
	ptimer->t5dl = (ptimer->dut5 + ptimer->trdl) / constant; /* delay */
	ptimer->t5wd = ptimer->opw5 / constant;	   		 /* width */
	ptimer->t5ld = ptimer->dut5 + ptimer->trdl;   /* leading edge delay */
	ptimer->t5td = ptimer->t5ld + ptimer->opw5;   /* trailing edge delay */
}

/*
 * WRITE_TIMER
 *
 * convert the value and write it 
 */
static write_timer(ptimer)
register struct timer	*ptimer;
{
	register struct vmeio	*pvmeio;
	register double		*pdelay;
	register short		count;
	register short		i;

	/* get the delay from trigger source */
	if (ptimer->torg.type == DB_LINK){
                if (db_fetch(&ptimer->torg.value,&ptimer->trdl) < 0){
			if (ptimer->stat != READ_ALARM){
				ptimer->stat = READ_ALARM;
				ptimer->sevr = MAJOR;
				ptimer->achn = 1;
			}
			return(-1);
		}else if (ptimer->stat == READ_ALARM){
			ptimer->stat = NO_ALARM;
			ptimer->sevr = NO_ALARM;
			ptimer->achn = 1;
		}
	}
			
	if (ptimer->out.type != VME_IO) return(-1);

	pvmeio = (struct vmeio *)(&ptimer->out.value);

	/* convert the value */
	convert_timer(ptimer);

	/* put the value to the ao driver */
	if (time_driver((int)pvmeio->card, /* card number */
	  (int)pvmeio->signal,		/* signal number */
	  (int)ptimer->type,		/* card type */
	  (int)ptimer->tsrc,		/* trigger source */
	  (int)ptimer->ptst,		/* pre-trigger state */
	  &ptimer->t1dl,		/* delay/width array */
	  1,				/* number of pulses */
	  ((ptimer->tevt == 0)?0:post_event),	/* addr of event post routine */
	  (int)ptimer->tevt)		/* event to post on trigger */
	    != NO_ALARM){
		if (ptimer->stat != WRITE_ALARM){
			ptimer->stat = WRITE_ALARM;
			ptimer->sevr = MAJOR;
			ptimer->achn = 1;
		}
		return(-1);
	}else if (ptimer->stat == WRITE_ALARM){
		ptimer->stat = NO_ALARM;
		ptimer->sevr = NO_ALARM;
		ptimer->achn = 1;
	}
	return(0);
}

/*
 * READ_TIMER
 *
 * read the current timer pulses and convert them to engineering units 
 */
static read_timer(ptimer)
register struct timer	*ptimer;
{
	register struct vmeio	*pvmeio;
	int			source;
	int			ptst;
	int			no_pulses;
	double			time_pulse[2];	/* delay and width */
	double			constant;

	/* initiate the write */
	if (ptimer->out.type != VME_IO) return(-1);

	/* only supports a one channel VME timer module !!!! */


	pvmeio = (struct vmeio *)(&ptimer->out.value);

	/* put the value to the ao driver */
	if (time_driver_read((int)pvmeio->card, /* card number */
	  (int)pvmeio->signal,			/* signal number */
	  (int)ptimer->type,			/* card type */
	  &source,				/* trigger source */
	  &ptst,				/* pre-trigger state */
	  &time_pulse[0],			/* delay/width */
	  &no_pulses,				/* number pulses found */
	  1) < 0) return;

	if (no_pulses == 0) return;

	/* convert according to time units */
	constant = constants[ptimer->timu];

	/* timing pulse 1 is currently active				   */
	/* put its parameters into the database so that it will not change */
	/* when the timer record is written				   */
	ptimer->dut1 = time_pulse[0] * constant;	/* delay to trigger */
	ptimer->opw1 = time_pulse[1] * constant;	/* pulse width */
	ptimer->ptst = ptst;				/* pre-trigger state */
	ptimer->tsrc = source;				/* clock source */
	return(0);
}
