/* recBo.c */
/* share/src/rec $Id$ */

/* recBo.c - Record Support Routines for Binary Output records
 *
 * Author: 	Bob Dalesio
 * Date:        7-17-87
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
 * .01  12-9-88         lrd     lock the record during processing
 * .02  12-15-88        lrd     process the forward scan link
 * .03  12-23-88        lrd     Alarm on locked MAX_LOCKED times
 * .04  01-13-89        lrd     delete db_read_bo and db_write_bo
 * .05  01-20-89        lrd     fixed vx includes
 * .06  03-29-89        lrd     make hardware errors MAJOR
 *                              remove hw severity spec from database
 *                              add continuous control
 * .07  04-06-89        lrd     add monitor detection
 * .08  05-03-89        lrd     removed process mask from arg list
 * .09  08-16-89        lrd     add ability to do softchannel momentary
 * .10  08-17-89        lrd     add soft channel support
 * .11  01-31-90        lrd     add the plc_flag arg to the ab_bodriver call
 * .12  03-21-90        lrd     add db_post_events for RVAL and RBV
 * .13  04-02-90        ba/lrd  add monitor handling for momentary outputs
 *                              remove rbv arg to ab_bodriver
 * .14  04-05-90        lrd     make momentary output handling a task
 *                              as the watchdog runs at interrupt level
 * .15  04-11-90        lrd     make locals static
 * .16  05-02-90        lrd     fix initial value set in the DOL field
 * .17  10-10-90	mrk	Changes for record and device support
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<strLib.h>
#include	<wdLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<boRecord.h>

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
long get_enum_str();
long get_enum_strs();
long put_enum_str();
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

struct rset boRSET={
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

struct bodset { /* binary output dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;/*(-1,0,1)=>(failure,success,don't Continue*/
};


/* control block for callback*/
struct callback {
        void (*callback)();
	int priority;
        struct dbAddr dbAddr;
        WDOG_ID wd_id;
};

void callbackRequest();
void alarm();
void monitor();



static void myCallback(pcallback)
    struct callback *pcallback;
{
    short	value=0;
    struct boRecord *pbo = (struct boRecord *)(pcallback->dbAddr.precord);

    dbScanLock(pbo);
    pbo->val = 0;
    (void)process(&(pcallback->dbAddr));
    dbScanUnlock(pbo); 
}

static long init_record(pbo)
    struct boRecord	*pbo;
{
    struct bodset *pdset;
    long status;
    struct callback *pcallback;

    if(!(pdset = (struct bodset *)(pbo->dset))) {
	recGblRecordError(S_dev_noDSET,pbo,"bo: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_bo functions defined */
    if( (pdset->number < 5) || (pdset->write_bo == NULL) ) {
	recGblRecordError(S_dev_missingSup,pbo,"bo: init_record");
	return(S_dev_missingSup);
    }
    /* get the initial value */
    if (pbo->dol.type == CONSTANT
    && (pbo->dol.value.value<=0.0 || pbo->dol.value.value>=udfFtest)){
	if (pbo->dol.value.value == 0)  pbo->val = 0;
	else                            pbo->val = 1;
    }
    pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
    pbo->dpvt = (caddr_t)pcallback;
    pcallback->callback = myCallback;
    pcallback->priority = priorityMedium;
    if(dbNameToAddr(pbo->name,&(pcallback->dbAddr))) {
            logMsg("dbNameToAddr failed in init_record for recBo\n");
            exit(1);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pbo,process))) return(status);
	if(pbo->rval!=udfUlong) { /* convert*/
	    if(pbo->rval==0) pbo->val = 0;
	    else pbo->val = 1;
	}
    }
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct boRecord	*pbo=(struct boRecord *)(paddr->precord);
	struct bodset	*pdset = (struct bodset *)(pbo->dset);
	long		 status;
	int		wait_time;

	if( (pdset==NULL) || (pdset->write_bo==NULL) ) {
		pbo->pact=TRUE;
		recGblRecordError(S_dev_missingSup,pbo,"write_bo");
		return(S_dev_missingSup);
	}
        if (!pbo->pact) {
		if((pbo->dol.type == DB_LINK) && (pbo->omsl == CLOSED_LOOP)){
			long options=0;
			long nRequest=1;
			unsigned short val;

			pbo->pact = TRUE;
			status = dbGetLink(&pbo->dol.value.db_link,pbo,
				DBR_USHORT,&val,&options,&nRequest);
			pbo->pact = FALSE;
			if(status==0){
				pbo->val = val;
			}else {
				if(pbo->nsev < VALID_ALARM) {
					pbo->nsev = VALID_ALARM;
					pbo->nsta = LINK_ALARM;
				}
			}
		}
		if(pbo->mask != udfUlong) {
			if(pbo->val==0) pbo->rval = 0;
			else pbo->rval = pbo->mask;
		}
	}
	if(pbo->val==udfEnum) {
		if(pbo->nsev < VALID_ALARM) {
			pbo->nsev = VALID_ALARM;
			pbo->nsta = SOFT_ALARM;
		}
		status = 0;
	} else status=(*pdset->write_bo)(pbo); /* write the new value */
	pbo->pact = TRUE;
	/* status is one if an asynchronous record is being processed*/
	if(status==1) return(0);
	tsLocalTime(&pbo->time);
	wait_time = (int)(pbo->high) * vxTicksPerSecond; /* seconds to ticks */
	if(pbo->val==1 && wait_time>0) {
		struct callback *pcallback;
	
		pcallback = (struct callback *)(pbo->dpvt);
        	if(pcallback->wd_id==NULL) pcallback->wd_id = wdCreate();
               	wdStart(pcallback->wd_id,wait_time,callbackRequest,pcallback);
	}
	/* check for alarms */
	alarm(pbo);
	/* check event list */
	monitor(pbo);
	/* process the forward scan link record */
	if (pbo->flnk.type==DB_LINK) dbScanPassive(pbo->flnk.value.db_link.pdbAddr);

	pbo->pact=FALSE;
	return(status);
}

static long get_value(pbo,pvdes)
    struct boRecord		*pbo;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_ENUM;
    pvdes->no_elements=1;
    (unsigned short *)(pvdes->pvalue) = &pbo->val;
    return(0);
}

static long get_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char	  *pstring;
{
    struct boRecord	*pbo=(struct boRecord *)paddr->precord;

    if(pbo->val==0) {
	strncpy(pstring,pbo->znam,sizeof(pbo->znam));
	pstring[sizeof(pbo->znam)] = 0;
    } else if(pbo->val==1) {
	strncpy(pstring,pbo->onam,sizeof(pbo->onam));
	pstring[sizeof(pbo->onam)] = 0;
    } else {
	strcpy(pstring,"Illegal_Value");
    }
    return(0);
}

static long get_enum_strs(paddr,pes)
    struct dbAddr *paddr;
    struct dbr_enumStrs *pes;
{
    struct boRecord	*pbo=(struct boRecord *)paddr->precord;

    pes->no_str = 2;
    bzero(pes->strs,sizeof(pes->strs));
    strncpy(pes->strs[0],pbo->znam,sizeof(pbo->znam));
    strncpy(pes->strs[1],pbo->onam,sizeof(pbo->onam));
    return(0);
}
static long put_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char          *pstring;
{
    struct boRecord     *pbo=(struct boRecord *)paddr->precord;

    if(strncmp(pstring,pbo->znam,sizeof(pbo->znam))==0) pbo->val = 0;
    else  if(strncmp(pstring,pbo->onam,sizeof(pbo->onam))==0) pbo->val = 1;
    else return(S_db_badChoice);
    return(0);
}

static void alarm(pbo)
    struct boRecord	*pbo;
{
	unsigned short val = pbo->val;


        /* check for  state alarm */
        if (val == 0){
                if (pbo->nsev<pbo->zsv){
                        pbo->nsta = STATE_ALARM;
                        pbo->nsev = pbo->zsv;
                }
        }else{
                if (pbo->nsev<pbo->osv){
                        pbo->nsta = STATE_ALARM;
                        pbo->nsev = pbo->osv;
                }
        }

        /* check for cos alarm */
	if(pbo->lalm==udfUshort) pbo->lalm = val;
	if(val == pbo->lalm) return;
        if (pbo->nsev<pbo->cosv) {
                pbo->nsta = COS_ALARM;
                pbo->nsev = pbo->cosv;
        }
	pbo->lalm = val;
        return;
}

static void monitor(pbo)
    struct boRecord	*pbo;
{
	unsigned short	monitor_mask;
        short           stat,sevr,nsta,nsev;

        /* get previous stat and sevr  and new stat and sevr*/
        stat=pbo->stat;
        sevr=pbo->sevr;
        nsta=pbo->nsta;
        nsev=pbo->nsev;
        /*set current stat and sevr*/
        pbo->stat = nsta;
        pbo->sevr = nsev;
        pbo->nsta = 0;
        pbo->nsev = 0;

        monitor_mask = 0;

        /* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev){
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and sevr fields */
                db_post_events(pbo,&pbo->stat,DBE_VALUE);
                db_post_events(pbo,&pbo->sevr,DBE_VALUE);
        }
        /* check for value change */
        if (pbo->mlst != pbo->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                pbo->mlst = pbo->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pbo,&pbo->val,monitor_mask);
        }
	if(pbo->oraw!=pbo->rval) {
		db_post_events(pbo,&pbo->rval,monitor_mask|DBE_VALUE);
		pbo->oraw = pbo->rval;
	}
	if(pbo->orbv!=pbo->rbv) {
		db_post_events(pbo,&pbo->rbv,monitor_mask|DBE_VALUE);
		pbo->orbv = pbo->rbv;
	}
        return;
}
