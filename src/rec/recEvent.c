/* recEvent.c */
/* share/src/rec $Id$ */

/* recEvent.c - Record Support Routines for Event records */
/*
 *      Author:          Janet Anderson
 *      Date:            12-13-91
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
 4 .00  12-13-91        jba     Initial definition
 * .01  02-28-92	jba	ANSI C changes
 * .02  04-10-92        jba     pact now used to test for asyn processing, not status
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbScan.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<eventRecord.h>

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

struct rset eventRSET={
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

struct eventdset { /* event input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_event;/*(0)=> success */
};
void monitor();

static long init_record(pevent)
    struct eventRecord	*pevent;
{
    struct eventdset *pdset;
    long status=0;

    if( (pdset=(struct eventdset *)(pevent->dset)) && (pdset->init_record) ) 
		status=(*pdset->init_record)(pevent,process);
    return(status);
}

static long process(pevent)
        struct eventRecord     *pevent;
{
	struct eventdset	*pdset = (struct eventdset *)(pevent->dset);
	long		 status=0;
	unsigned char    pact=pevent->pact;

	if((pdset!=NULL) && (pdset->number >= 5) && pdset->read_event ) 
		status=(*pdset->read_event)(pevent); /* read the new value */
	/* check if device support set pact */
	if ( !pact && pevent->pact ) return(0);
	pevent->pact = TRUE;
 
	if(pevent->val>0) post_event((int)pevent->val);

	tsLocalTime(&pevent->time);

	/* check event list */
	monitor(pevent);

	/* process the forward scan link record */
	if (pevent->flnk.type==DB_LINK) dbScanPassive(((struct dbAddr *)pevent->flnk.value.db_link.pdbAddr)->precord);

	pevent->pact=FALSE;
	return(status);
}


static long get_value(pevent,pvdes)
    struct eventRecord             *pevent;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_USHORT;
    pvdes->no_elements=1;
    pvdes->pvalue = (void *)(&pevent->val);
    return(0);
}


static void monitor(pevent)
    struct eventRecord             *pevent;
{
    unsigned short  monitor_mask;
    short           stat,sevr,nsta,nsev;

    /* get previous stat and sevr  and new stat and sevr*/
    recGblResetSevr(pevent,stat,sevr,nsta,nsev);

    /* Flags which events to fire on the value field */
    monitor_mask = 0;

    /* alarm condition changed this scan */
    if (stat!=nsta || sevr!=nsev) {
            /* post events for alarm condition change*/
            monitor_mask = DBE_ALARM;
            /* post stat and nsev fields */
            db_post_events(pevent,&pevent->stat,DBE_VALUE);
            db_post_events(pevent,&pevent->sevr,DBE_VALUE);
    }

    db_post_events(pevent,&pevent->val,monitor_mask|DBE_VALUE);
    return;
}
