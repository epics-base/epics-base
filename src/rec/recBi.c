/* recBi.c */
/* share/src/rec $Id$ */

/* recBi.c - Record Support Routines for Binary Input records
 *
 * Author: 	Bob Dalesio
 * Date:	7-9-87
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
 * .01  12-12-88        lrd     lock the record on entry and unlock on exit
 * .02  12-15-88        lrd     Process the forward scan link
 * .03  12-23-88        lrd     Alarm on locked MAX_LOCKED times
 * .04  01-13-89        lrd     delete db_read_bi
 * .05  03-17-89        lrd     database link inputs
 * .06  03-29-89        lrd     make hardware errors MAJOR
 *                              remove hw severity spec from database
 * .07  04-06-89        lrd     add monitor detection
 * .08  05-03-89        lrd     removed process mask from arg list
 * .09  01-31-90        lrd     add the plc_flag arg to the ab_bidriver call
 * .10  03-21-90        lrd     add db_post_events for RVAL
 * .11  04-11-90        lrd     make local variables static
 * .12  10-10-90	mrk	Made changes for new record and device support
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<strLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<biRecord.h>
/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
long init_record();
long process();
#define special NULL
#define get_precision NULL
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
long get_enum_str();
#define get_units NULL
#define get_graphic_double NULL
#define get_control_double NULL
long get_enum_strs();
struct rset biRSET={
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
struct bidset { /* binary input dset */
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;/*(-1,0,1)=>(failure,success,don't Continue*/
};
void alarm();
void monitor();

static long init_record(pbi)
    struct biRecord	*pbi;
{
    struct bidset *pdset;
    long status;

    if(!(pdset = (struct bidset *)(pbi->dset))) {
	recGblRecordError(S_dev_noDSET,pbi,"bi: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_bi function defined */
    if( (pdset->number < 5) || (pdset->read_bi == NULL) ) {
	recGblRecordError(S_dev_missingSup,pbi,"bi: init_record");
	return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pbi,process))) return(status);
    }
    pbi->mlst = -1;
    pbi->lalm = -1;
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct biRecord	*pbi=(struct biRecord *)(paddr->precord);
	struct bidset	*pdset = (struct bidset *)(pbi->dset);
	long		 status;

	if( (pdset==NULL) || (pdset->read_bi==NULL) ) {
		pbi->pact=TRUE;
		recGblRecordError(S_dev_missingSup,pbi,"read_bi");
		return(S_dev_missingSup);
	}

	status=(*pdset->read_bi)(pbi); /* read the new value */
	pbi->pact = TRUE;

	/* status is one if an asynchronous record is being processed*/
	if(status==1) return(1);

	/* check for alarms */
	alarm(pbi);
	/* check event list */
	monitor(pbi);
	/* process the forward scan link record */
	if (pbi->flnk.type==DB_LINK) dbScanPassive(pbi->flnk.value.db_link.pdbAddr);

	pbi->pact=FALSE;
	return(status);
}

static long get_value(pbi,pvdes)
    struct biRecord		*pbi;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_ENUM;
    pvdes->no_elements=1;
    (unsigned short *)(pvdes->pvalue) = &pbi->val;
    return(0);
}

static long get_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char	  *pstring;
{
    struct biRecord	*pbi=(struct biRecord *)paddr->precord;

    if(pbi->val==0) {
	strncpy(pstring,pbi->znam,sizeof(pbi->znam));
	pstring[sizeof(pbi->znam)] = 0;
    } else if(pbi->val==1) {
	strncpy(pstring,pbi->onam,sizeof(pbi->onam));
	pstring[sizeof(pbi->onam)] = 0;
    } else {
	strcpy(pstring,"Illegal_Value");
    }
    return(0);
}

static long get_enum_strs(paddr,pes)
    struct dbAddr *paddr;
    struct dbr_enumStrs *pes;
{
    struct biRecord	*pbi=(struct biRecord *)paddr->precord;

    pes->no_str = 2;
    bzero(pes->strs,sizeof(pes->strs));
    strncpy(pes->strs[0],pbi->znam,sizeof(pbi->znam));
    strncpy(pes->strs[1],pbi->onam,sizeof(pbi->onam));
    return(0);
}

static void alarm(pbi)
    struct biRecord	*pbi;
{

        if (pbi->val == pbi->lalm) return;

        /* set last alarmed value */
        pbi->lalm = pbi->val;

        /* check for  state alarm */
        if (pbi->val == 0){
                if (pbi->nsev<pbi->zsv){
                        pbi->nsta = STATE_ALARM;
                        pbi->nsev = pbi->zsv;
                }
        }else{
                if (pbi->nsev<pbi->osv){
                        pbi->nsta = STATE_ALARM;
                        pbi->nsev = pbi->osv;
                }
        }

        /* check for cos alarm */
        if (pbi->nsev<pbi->cosv) {
                pbi->nsta = COS_ALARM;
                pbi->nsev = pbi->cosv;
        }

	return;
}

static void monitor(pbi)
    struct biRecord	*pbi;
{
	unsigned short	monitor_mask;
        short           stat,sevr,nsta,nsev;

        /* get previous stat and sevr  and new stat and sevr*/
        stat=pbi->stat;
        sevr=pbi->sevr;
        nsta=pbi->nsta;
        nsev=pbi->nsev;
        /*set current stat and sevr*/
        pbi->stat = nsta;
        pbi->sevr = nsev;
        pbi->nsta = 0;
        pbi->nsev = 0;

	/* anyone waiting for an event on this record */
	if (pbi->mlis.count == 0) return;

	/* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (stat!=nsta || sevr!=nsev){
		/* post events for alarm condition change*/
		monitor_mask = DBE_ALARM;
		/* post stat and sevr fields */
		db_post_events(pbi,&pbi->stat,DBE_VALUE);
		db_post_events(pbi,&pbi->sevr,DBE_VALUE);
	}
        /* check for value change */
        if (pbi->mlst != pbi->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                pbi->mlst = pbi->val;
        }

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(pbi,&pbi->val,monitor_mask);
	}
	if(pbi->oraw!=pbi->rval) {
		db_post_events(pbi,&pbi->rval,monitor_mask|DBE_VALUE);
		pbi->oraw = pbi->rval;
	}
	return;
}
