/* recTimer.c */
/* base/src/rec  $Id$ */

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
 * .15  02-28-92	jba	ANSI C changes
 * .16  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .17  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .18  10-10-92        jba     replaced get of trdl from torg code with recGblGetLinkValue call
 * .19  09-02-93        mcn     Changed DSET structure for timers, moved code that bypassed
 *                              device support into new device support (major improvement).
 * .20  03-30-94        mcn     converted to fast links
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
#define GEN_SIZE_OFFSET
#include	<timerRecord.h>
#undef  GEN_SIZE_OFFSET

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

struct tmdset { /* timer dset */
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN	read;
        DEVSUPFUN	write;
};

/* The one remaining definition for still unimplemented DG535 */
/* so iocInit shuts up */
struct dset devTmDg535={4,NULL,NULL,NULL,NULL};

extern int post_event();

static void monitor();
static long read_timer();
static void convert_timer();
static long write_timer();

static long init_record(ptimer, pass)
    struct timerRecord	*ptimer;
    int pass;
{
    long status;
/* Added for Channel Access Links */

    if (pass==0) return(0);

    /* get the delay initial value if torg is a constant*/
    if (ptimer->torg.type == CONSTANT) {
	recGblInitConstantLink(&ptimer->torg,DBF_FLOAT,&ptimer->trdl);
    }

    /* read to maintain time pulses over a restart */
    return read_timer(ptimer);
}

static long process(ptimer)
	struct timerRecord	*ptimer;
{
        long status;

	ptimer->pact=TRUE;

	/* write the new value */
	status = write_timer(ptimer);
	ptimer->udf=FALSE;
	recGblGetTimeStamp(ptimer);

        /* check event list */
        monitor(ptimer);
        /* process the forward scan link record */
        recGblFwdLink(ptimer);

        ptimer->pact=FALSE;
        return status;
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
	     
  monitor_mask = recGblResetAlarms(ptimer);
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

static long read_timer(struct timerRecord *ptimer)
{
   struct tmdset *pdset;
   double constant;

   /* initiate the write */
   if (ptimer->out.type != VME_IO) {
           recGblRecordError(S_dev_badOutType,(void *)ptimer,"read_timer");
           return S_dev_badOutType;
   }

   pdset = (struct tmdset *) ptimer->dset;
   if (pdset == NULL) {
         recGblRecordError(S_dev_noDSET,(void *)pdset, "timer: process");
         return S_dev_noDSET;
   }

   /* call device support */
   (pdset->read)(ptimer);

   return 0;
}

static long write_timer(struct timerRecord *ptimer)
{
   struct tmdset *pdset;
   long status;

   /* get the delay from trigger source */
   if (ptimer->torg.type == DB_LINK) {
           status=dbGetLink(&(ptimer->torg),DBR_FLOAT,&(ptimer->trdl),0,0);
           if (!RTN_SUCCESS(status)) return status;
   }
 
   if (ptimer->out.type != VME_IO) {
           recGblSetSevr(ptimer,WRITE_ALARM,INVALID_ALARM);
           return 0;
   }

   /* should we maintain through a reboot */
   if (ptimer->main && ptimer->rdt1 && ptimer->rpw1) {
           ptimer->dut1 = ptimer->rdt1 - ptimer->trdl;
           ptimer->opw1 = ptimer->rpw1;
           ptimer->main = 0;       /* only kept on the first write */
   }
 
   /* convert the value */
   convert_timer(ptimer);

   pdset = (struct tmdset *) ptimer->dset;
   if (pdset == NULL) {
         recGblRecordError(S_dev_noDSET,(void *)pdset, "timer: process");
         return S_dev_noDSET;
   }

   /* call device support */
   if ((pdset->write)(ptimer) != OK) {
         recGblSetSevr(ptimer,WRITE_ALARM,INVALID_ALARM);
         return 0;
   }

   return 0;
}

