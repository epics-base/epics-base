/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devMz8310.c */
/* base/src/dev $Id$ */

/* Device Support Routines for MZ8310 Timer */
/*
 *      Original Author: Matthew C. Needes
 *      Date:            9-2-93
 */

#include	<vxWorks.h>
#include	<vme.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<iv.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbCommon.h>
#include	<fast_lock.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<dbScan.h>
#include	<special.h>
#include	<module_types.h>
#include	<eventRecord.h>
#include	<pulseCounterRecord.h>
#include	<pulseDelayRecord.h>
#include	<pulseTrainRecord.h>
#include	<timerRecord.h>

#include 	<drvMz8310.h>

static long read_timer();
static long write_timer();

static void localPostEvent (void *pParam);

struct tmdset {
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read;
        DEVSUPFUN       write;
};

struct tmdset devTmMizar8310 = { 6, NULL, NULL, NULL, NULL, read_timer, write_timer };

/*
 * These constants are indexed by the time units field in the timer record.
 * Values are converted to seconds.
 */
static double constants[] = {1e3,1e6,1e9,1e12};

static long read_timer(struct timerRecord *ptimer)
{
   struct vmeio    	*pvmeio;
   unsigned		source;
   unsigned		ptst;
   double          	time_pulse[2];  /* delay and width */
   double          	constant;
 
   /* only supports a one channel VME timer module !!!! */
   pvmeio = (struct vmeio *)(&ptimer->out.value);
 
   if (mz8310_one_shot_read(
           &ptst,                     /* pre-trigger state */
           &(time_pulse[0]),          /* offset of pulse */
           &(time_pulse[1]),          /* width of pulse */
           pvmeio->card,              /* card number */
           pvmeio->signal,            /* signal number */
           &source) != 0) {           /* trigger source */
       return 1;
   }

   /* convert according to time units */
   constant = constants[ptimer->timu];
 
   /* timing pulse 1 is currently active                              */
   /* put its parameters into the database so that it will not change */
   /* when the timer record is written                                */
   ptimer->rdt1 = time_pulse[0] * constant;        /* delay to trigger */
   ptimer->rpw1 = time_pulse[1] * constant;        /* pulse width */
 
   return 0;
}

static long write_timer(struct timerRecord *ptimer)
{
   struct vmeio    *pvmeio;
   void		   (*pCB)(void *);

   /* put the value to the ao driver */
   pvmeio = (struct vmeio *)(&ptimer->out.value);
 
   if (ptimer->tevt) {
	pCB = localPostEvent;
   }
   else {
	pCB = NULL;
   }

   /* put the value to the ao driver */
   return mz8310_one_shot(
         ptimer->ptst,		/* pre-trigger state */
         ptimer->t1dl, 		/* pulse offset */
         ptimer->t1wd,		/* pulse width */
         pvmeio->card,		/* card number */
         pvmeio->signal,	/* signal number */
         ptimer->tsrc,		/* trigger source */
         pCB,   		/* addr of event post routine */
         ptimer);		/* event to post on trigger */
}

static void localPostEvent (void *pParam)
{
	struct timerRecord *ptimer = pParam;

	if (ptimer->tevt) {
		post_event (ptimer->tevt);
	}
}

