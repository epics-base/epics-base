/* recStringin.c */
 ! share/ascii $Id$

/* recState.c - Record Support Routines for State records
 *
 * Author: 	Janet Anderson
 * Date:	4/23/91
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
 * .01  10-10-90	jba	device support added
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<strLib.h>

#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<stringinRecord.h>

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

struct rset stringinRSET={
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

struct stringindset { /* stringin input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_stringin;/*(0,1)=> success, async */
};
void monitor();

static long init_record(pstringin)
    struct stringinRecord	*pstringin;
{
    struct stringindset *pdset;
    long status;

    if(!(pdset = (struct stringindset *)(pstringin->dset))) {
	recGblRecordError(S_dev_noDSET,pstringin,"stringin: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_stringin function defined */
    if( (pdset->number < 5) || (pdset->read_stringin == NULL) ) {
	recGblRecordError(S_dev_missingSup,pstringin,"stringin: init_record");
	return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pstringin,process))) return(status);
    }
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct stringinRecord	*pstringin=(struct stringinRecord *)(paddr->precord);
	struct stringindset	*pdset = (struct stringindset *)(pstringin->dset);
	long		 status;

	if( (pdset==NULL) || (pdset->read_stringin==NULL) ) {
		pstringin->pact=TRUE;
		recGblRecordError(S_dev_missingSup,pstringin,"read_stringin");
		return(S_dev_missingSup);
	}

	status=(*pdset->read_stringin)(pstringin); /* read the new value */
	pstringin->pact = TRUE;

	/* status is one if an asynchronous record is being processed*/
	if (status==1) return(0);

	tsLocalTime(&pstringin->time);
	if(status==2) status=0;

	/* check event list */
	monitor(pstringin);

	/* process the forward scan link record */
	if (pstringin->flnk.type==DB_LINK) dbScanPassive(pstringin->flnk.value.db_link.pdbAddr);

	pstringin->pact=FALSE;
	return(status);
}


static long get_value(pstringin,pvdes)
    struct stringinRecord             *pstringin;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_STRING;
    pvdes->no_elements=1;
    pvdes->pvalue = (caddr_t)(&pstringin->val[0]);
    return(0);
}


static void monitor(pstringin)
    struct stringinRecord             *pstringin;
{
    unsigned short  monitor_mask;
    short           stat,sevr,nsta,nsev;

    /* get previous stat and sevr  and new stat and sevr*/
    stat=pstringin->stat;
    sevr=pstringin->sevr;
    nsta=pstringin->nsta;
    nsev=pstringin->nsev;
    /*set current stat and sevr*/
    pstringin->stat = nsta;
    pstringin->sevr = nsev;
    pstringin->nsta = 0;
    pstringin->nsev = 0;

    /* Flags which events to fire on the value field */
    monitor_mask = 0;

    /* alarm condition changed this scan */
    if (stat!=nsta || sevr!=nsev) {
            /* post events for alarm condition change*/
            monitor_mask = DBE_ALARM;
            /* post stat and nsev fields */
            db_post_events(pstringin,&pstringin->stat,DBE_VALUE);
            db_post_events(pstringin,&pstringin->sevr,DBE_VALUE);
    }

    if(strncmp(pstringin->oval,pstringin->val,sizeof(pstringin->val))) {
       	if(pstringin->mlis.count != 0)
             db_post_events(pstringin,&(pstringin->val[0]),monitor_mask|DBE_VALUE);
	strncpy(pstringin->oval,pstringin->val,sizeof(pstringin->val));
    }
    return;
}
