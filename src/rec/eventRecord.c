/* recEvent.c */
/* base/src/rec  $Id$ */

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
 * .03  04-18-92        jba     removed process from dev init_record parms
 * .04  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .05  08-17-92        jba     added simulation mode handling
 * .06	03-29-94	mcn	converted to fast links
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbEvent.h>
#include	<dbScan.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#define GEN_SIZE_OFFSET
#include	<eventRecord.h>
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
static void monitor();
static long readValue();


static long init_record(pevent,pass)
    struct eventRecord	*pevent;
    int pass;
{
    struct eventdset *pdset;
    long status=0;

    if (pass==0) return(0);

    if (pevent->siml.type == CONSTANT) {
	recGblInitConstantLink(&pevent->siml,DBF_USHORT,&pevent->simm);
    }

    if (pevent->siol.type == CONSTANT) {
	recGblInitConstantLink(&pevent->siol,DBF_USHORT,&pevent->sval);
    }

    if( (pdset=(struct eventdset *)(pevent->dset)) && (pdset->init_record) ) 
		status=(*pdset->init_record)(pevent);
    return(status);
}

static long process(pevent)
        struct eventRecord     *pevent;
{
	struct eventdset	*pdset = (struct eventdset *)(pevent->dset);
	long		 status=0;
	unsigned char    pact=pevent->pact;

	if((pdset!=NULL) && (pdset->number >= 5) && pdset->read_event ) 
                status=readValue(pevent); /* read the new value */
	/* check if device support set pact */
	if ( !pact && pevent->pact ) return(0);
	pevent->pact = TRUE;
 
	if(pevent->val>0) post_event((int)pevent->val);

	recGblGetTimeStamp(pevent);

	/* check event list */
	monitor(pevent);

	/* process the forward scan link record */
	recGblFwdLink(pevent);

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

    /* get previous stat and sevr  and new stat and sevr*/
    monitor_mask = recGblResetAlarms(pevent);
    db_post_events(pevent,&pevent->val,monitor_mask|DBE_VALUE);
    return;
}

static long readValue(pevent)
        struct eventRecord *pevent;
{
        long            status;
        struct eventdset   *pdset = (struct eventdset *) (pevent->dset);

        if (pevent->pact == TRUE){
                status=(*pdset->read_event)(pevent);
                return(status);
        }

        status=dbGetLink(&(pevent->siml),DBR_USHORT,&(pevent->simm),0,0);

        if (status)
                return(status);

        if (pevent->simm == NO){
                status=(*pdset->read_event)(pevent);
                return(status);
        }
        if (pevent->simm == YES){
                status=dbGetLink(&(pevent->siol),DBR_USHORT,
			&(pevent->sval),0,0);
                if (status==0) {
                        pevent->val=pevent->sval;
                        pevent->udf=FALSE;
                }
        } else {
                status=-1;
                recGblSetSevr(pevent,SOFT_ALARM,INVALID_ALARM);
                return(status);
        }
        recGblSetSevr(pevent,SIMM_ALARM,pevent->sims);

        return(status);
}
