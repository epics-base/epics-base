/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recTimer.c */
/* base/src/rec  $Id$ */

/* recTimer.c - Record Support Routines for Timer records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            1-9-89
 *
 */

#include <vxWorks.h>
#include <types.h>
#include <stdioLib.h>
#include <lstLib.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "module_types.h"
#define GEN_SIZE_OFFSET
#include "timerRecord.h"
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

