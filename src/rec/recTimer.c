/* recTimer.c */
/* share/src/rec $Id$ */

/* recTimer.c - Record Support Routines for Timer records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            1-9-89
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
 * .11  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .12  12-02-91        jba     Added cmd control to io-interrupt processing
 * .13  12-12-91        jba     Set cmd to zero in io-interrupt processing
 * .14  02-05-92	jba	Changed function arguments from paddr to precord 
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<module_types.h>
#include	<timerRecord.h>

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
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

struct rset timerRSET={
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

/* because the driver does all the work just declare device support here*/
long get_ioint_info();
struct dset devTmMizar8310={4,NULL,NULL,NULL,get_ioint_info};
struct dset devTmDg535={4,NULL,NULL,NULL,get_ioint_info};
struct dset devTmVxiAt5={4,NULL,NULL,NULL,get_ioint_info};
static long get_ioint_info(cmd,ptimer,io_type,card_type,card_number)
    short               *cmd;
    struct timerRecord     *ptimer;
    short               *io_type;
    short               *card_type;
    short               *card_number;
{
    *cmd=-1;
    if(ptimer->out.type != VME_IO) return(S_dev_badInpType);
    *io_type = IO_TIMER;
    if(ptimer->dtyp==0)
	*card_type = MZ8310;
    else if(ptimer->dtyp==3)
	*card_type = VXI_AT5_TIME;
    else
	return(1);
    *card_number = ptimer->out.value.vmeio.card;
    return(0);
}


extern int	post_event();

void monitor();
void read_timer();
void convert_timer();
void write_timer();

static long init_record(ptimer)
    struct timerRecord	*ptimer;
{
    /* get the delay initial value if torg is a constant*/
    if (ptimer->torg.type == CONSTANT ){
            ptimer->trdl = ptimer->torg.value.value;
    }


    /* read to maintain time pulses over a restart */
    read_timer(ptimer);
    return(0);
}

static long process(ptimer)
	struct timerRecord	*ptimer;
{

	ptimer->pact=TRUE;

	/* write the new value */
	write_timer(ptimer);
	ptimer->udf=FALSE;
	tsLocalTime(&ptimer->time);

        /* check event list */
        monitor(ptimer);
        /* process the forward scan link record */
        if (ptimer->flnk.type==DB_LINK) dbScanPassive(((struct dbAddr *)ptimer->flnk.value.db_link.pdbAddr)->precord);

        ptimer->pact=FALSE;
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

static void monitor(ptimer)
    struct timerRecord *ptimer;
{
        unsigned short  monitor_mask;
        short           stat,sevr,nsta,nsev;

        /* get previous stat and sevr  and new stat and sevr*/
        recGblResetSevr(ptimer,stat,sevr,nsta,nsev);

        /* Flags which events to fire on the value field */
        monitor_mask = 0;

        /* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev){
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and sevr fields */
                db_post_events(ptimer,&ptimer->stat,DBE_VALUE);
                db_post_events(ptimer,&ptimer->sevr,DBE_VALUE);
        }

	monitor_mask |= (DBE_VALUE | DBE_LOG);
	db_post_events(ptimer,&ptimer->val,monitor_mask);
	db_post_events(ptimer,&ptimer->t1wd,monitor_mask);
	db_post_events(ptimer,&ptimer->t1ld,monitor_mask);
	db_post_events(ptimer,&ptimer->t1td,monitor_mask);
	return;
}

/*
 * These constants are indexed by the time units field in the timer record.
 * Values are converted to seconds.
 */
static double constants[] = {1e3,1e6,1e9,1e12};
/*
 * CONVERT_TIMER
 *
 */
static void convert_timer(ptimer)
struct timerRecord	*ptimer;
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
static void write_timer(ptimer)
struct timerRecord	*ptimer;
{
	struct vmeio	*pvmeio;
	long status,options,nRequest;

	/* get the delay from trigger source */
	if (ptimer->torg.type == DB_LINK) {
		options=0;
		nRequest=1;
		status = dbGetLink(&(ptimer->torg.value.db_link),(struct dbCommon *)ptimer,DBR_FLOAT,
                &(ptimer->trdl),&options,&nRequest);
                if(status!=0){
                       recGblSetSevr(ptimer,LINK_ALARM,VALID_ALARM);
                       return;
                }
        }
	if (ptimer->out.type != VME_IO) {
                recGblSetSevr(ptimer,WRITE_ALARM,VALID_ALARM);
		return;
	}
	pvmeio = (struct vmeio *)(&ptimer->out.value);

        /* should we maintain through a reboot */
        if (ptimer->main && ptimer->rdt1 && ptimer->rpw1){
                ptimer->dut1 = ptimer->rdt1 - ptimer->trdl;
                ptimer->opw1 = ptimer->rpw1;
                ptimer->main = 0;       /* only kept on the first write */
        }


	/* convert the value */
	convert_timer(ptimer);

	/* put the value to the ao driver */
	if (time_driver((int)pvmeio->card, /* card number */
	  (int)pvmeio->signal,		/* signal number */
	  (int)ptimer->dtyp,		/* card type */
	  (int)ptimer->tsrc,		/* trigger source */
	  (int)ptimer->ptst,		/* pre-trigger state */
	  &ptimer->t1dl,		/* delay/width array */
	  1,				/* number of pulses */
	  ((ptimer->tevt == 0)?0:post_event),	/* addr of event post routine */
	  (int)ptimer->tevt)		/* event to post on trigger */
	    != 0){
                recGblSetSevr(ptimer,WRITE_ALARM,VALID_ALARM);
	}
	return;
}

/*
 * READ_TIMER
 *
 * read the current timer pulses and convert them to engineering units 
 */
static void read_timer(ptimer)
struct timerRecord	*ptimer;
{
	struct vmeio	*pvmeio;
	int			source;
	int			ptst;
	int			no_pulses;
	double			time_pulse[2];	/* delay and width */
	double			constant;

	/* initiate the write */
	if (ptimer->out.type != VME_IO) {
		recGblRecordError(S_dev_badOutType,ptimer,"read_timer");
		return;
	}

	/* only supports a one channel VME timer module !!!! */

	pvmeio = (struct vmeio *)(&ptimer->out.value);

	/* put the value to the ao driver */
	if (time_driver_read((int)pvmeio->card, /* card number */
	  (int)pvmeio->signal,			/* signal number */
	  (int)ptimer->dtyp,			/* card type */
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
	ptimer->rdt1 = time_pulse[0] * constant;	/* delay to trigger */
	ptimer->rpw1 = time_pulse[1] * constant;	/* pulse width */
	return;
}
