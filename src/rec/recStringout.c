/* recStringout.c */
 ! share/rec $Id$

/* recStringout.c - Record Support Routines for Stringout records
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
 * .01  04-23-91	jba	device support added
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
#include	<stringoutRecord.h>

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

struct rset stringoutRSET={
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

struct stringoutdset { /* stringout input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_stringout;/*(-1,0,1)=>(failure,success,don't Continue*/
};
void monitor();

static long init_record(pstringout)
    struct stringoutRecord	*pstringout;
{
    struct stringoutdset *pdset;
    long status=0;

    if(!(pdset = (struct stringoutdset *)(pstringout->dset))) {
	recGblRecordError(S_dev_noDSET,pstringout,"stringout: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_stringout functions defined */
    if( (pdset->number < 5) || (pdset->write_stringout == NULL) ) {
	recGblRecordError(S_dev_missingSup,pstringout,"stringout: init_record");
	return(S_dev_missingSup);
    }
    /* get the initial value dol is a constant*/
    if (pstringout->dol.type == CONSTANT
    && (pstringout->dol.value.value<=0.0 || pstringout->dol.value.value>=udfFtest)){
	pstringout->val = pstringout->dol.value.value;
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pstringout,process))) return(status);
    }
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct stringoutRecord	*pstringout=(struct stringoutRecord *)(paddr->precord);
	struct stringoutdset	*pdset = (struct stringoutdset *)(pstringout->dset);
	long		 status=0;
	int		wait_time;

	if( (pdset==NULL) || (pdset->write_stringout==NULL) ) {
		pstringout->pact=TRUE;
		recGblRecordError(S_dev_missingSup,pstringout,"write_stringout");
		return(S_dev_missingSup);
	}
        if (!pstringout->pact) {
		if((pstringout->dol.type == DB_LINK) && (pstringout->omsl == CLOSED_LOOP)){
			long options=0;
			long nRequest=1;
			string val[20];

			pstringout->pact = TRUE;
			status = dbGetLink(&pstringout->dol.value.db_link,pstringout,
				DBR_STRING,&val,&options,&nRequest);
			pstringout->pact = FALSE;
			if(status==0){
				pstringout->val = val;
			}else {
				if(pstringout->nsev < VALID_ALARM) {
					pstringout->nsev = VALID_ALARM;
					pstringout->nsta = LINK_ALARM;
				}
			}
		}
	}

	if(status==0) {
		status=(*pdset->write_stringout)(pstringout); /* write the new value */
	}
	pstringout->pact = TRUE;

	/* status is one if an asynchronous record is being processed*/
	if (status==1) return(0);

	tsLocalTime(&pstringout->time);

	/* check event list */
	monitor(pstringout);

	/* process the forward scan link record */
	if (pstringout->flnk.type==DB_LINK) dbScanPassive(pstringout->flnk.value.db_link.pdbAddr);

	pstringout->pact=FALSE;
	return(status);
}

static long get_value(pstringout,pvdes)
    struct stringoutRecord             *pstringout;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_STRING;
    pvdes->no_elements=1;
    pvdes->pvalue = (caddr_t)(&pstringout->val[0]);
    return(0);
}


static void monitor(pstringout)
    struct stringoutRecord             *pstringout;
{
    unsigned short  monitor_mask;
    short           stat,sevr,nsta,nsev;

    /* get previous stat and sevr  and new stat and sevr*/
    stat=pstringout->stat;
    sevr=pstringout->sevr;
    nsta=pstringout->nsta;
    nsev=pstringout->nsev;
    /*set current stat and sevr*/
    pstringout->stat = nsta;
    pstringout->sevr = nsev;
    pstringout->nsta = 0;
    pstringout->nsev = 0;

    /* Flags which events to fire on the value field */
    monitor_mask = 0;

    /* alarm condition changed this scan */
    if (stat!=nsta || sevr!=nsev) {
            /* post events for alarm condition change*/
            monitor_mask = DBE_ALARM;
            /* post stat and nsev fields */
            db_post_events(pstringout,&pstringout->stat,DBE_VALUE);
            db_post_events(pstringout,&pstringout->sevr,DBE_VALUE);
    }

    if(strncmp(pstringout->oval,pstringout->val,sizeof(pstringout->val))) {
       	if(pstringout->mlis.count != 0)
             db_post_events(pstringout,&(pstringout->val[0]),monitor_mask|DBE_VALUE);
	strncpy(pstringout->oval,pstringout->val,sizeof(pstringout->val));
    }
    return;
}
