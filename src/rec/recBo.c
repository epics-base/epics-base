
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

#include	<alarm.h>
#include	<cvtTable.h>
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
long report();
#define initialize NULL
long init_record();
long process();
long special();
long get_precision();
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
long get_enum_str();
#define get_units NULL
#define get_graphic_double NULL
#define get_control_double NULL
long get_enum_strs();

struct rset boRSET={
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

struct bodset { /* binary output dset */
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bo;/*(-1,0)=>(failure,success*/
	DEVSUPFUN	write_bo;/*(-1,0,1)=>(failure,success,don't Continue*/
};

/* the following definitions must match those in choiceGbl.ascii */
#define OUTPUT_FULL 0
#define CLOSED_LOOP 1

static long report(fp,paddr)
    FILE	  *fp;
    struct dbAddr *paddr;
{
    struct boRecord	*pbo=(struct boRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    if(fprintf(fp,"VAL  %d\n",pbo->val)) return(-1);
    if(recGblReportLink(fp,"OUT ",&(pbo->out))) return(-1);
    if(recGblReportLink(fp,"FLNK",&(pbo->flnk))) return(-1);
    if(fprintf(fp,"RVAL 0x%-8X\n",
	pbo->rval)) return(-1);
    return(0);
}

static long init_record(pbo)
    struct boRecord	*pbo;
{
    struct bodset *pdset;
    long status;

    if(!(pdset = (struct bodset *)(pbo->dset))) {
	recGblRecordError(S_dev_noDSET,pbo,"bo: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_bo and write_bo functions defined */
    if( (pdset->number < 5) || (pdset->read_bo == NULL) || (pdset->write_bo == NULL) ) {
	recGblRecordError(S_dev_missingSup,pbo,"bo: init_record");
	return(S_dev_missingSup);
    }
    /* get the intial value */
    if (pbo->dol.type == CONSTANT){
	if (pbo->dol.value.value != 0)  pbo->val = 1;
	else                            pbo->val = 0;
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pbo))) return(status);
    }
    pbo->time = pbo->high * 60;   /* seconds to ticks */
    pbo->lalm = -1;
    pbo->mlst = -1;
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct boRecord	*pbo=(struct boRecord *)(paddr->precord);
	struct bodset	*pdset = (struct bodset *)(pbo->dset);
	long		 status;
	long		nRequest;
        float   fval;

	if( (pdset==NULL) || (pdset->read_bo==NULL) ) {
		pbo->pact=TRUE;
		recGblRecordError(S_dev_missingSup,pbo,"read_bo");
		return(S_dev_missingSup);
	}


        /* fetch the desired output if there is a database link */
        if ( !(pbo->pact) && (pbo->dol.type == DB_LINK) && (pbo->omsl == CLOSED_LOOP)){
		nRequest = 1;
                if (dbGetLink(&pbo->dol.value,DBF_SHORT,&fval,&nRequest) < 0){
                        if (pbo->stat != READ_ALARM){
                                pbo->stat = READ_ALARM;
                                pbo->sevr = MAJOR_ALARM;
                                pbo->achn = 1;
                        }
                        return(-1);
                }
                if (fval != 0)  pbo->val = 1;
                else            pbo->val = 0;
        }

	status=(*pdset->write_bo)(pbo); /* write the new value */
	pbo->pact = TRUE;

	/* status is one if an asynchronous record is being processed*/
	if(status==1) return(1);
	else if(status == -1) {
		if(pbo->stat != WRITE_ALARM) {/* error. set alarm condition */
			pbo->stat = WRITE_ALARM;
			pbo->sevr = MAJOR_ALARM;
			pbo->achn=1;
		}
	}
	else if(status!=0) return(status);
	else if(pbo->stat == WRITE_ALARM) {
		pbo->stat = NO_ALARM;
		pbo->sevr = NO_ALARM;
		pbo->achn=1;
	}

	/* check for alarms */
	alarm(pbo);


	/* check event list */
	if(!pbo->disa) status = monitor(pbo);

	/* process the forward scan link record */
	if (pbo->flnk.type==DB_LINK) dbScanPassive(&pbo->flnk.value);

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
    } else if(pbo->val==0) {
	strncpy(pstring,pbo->onam,sizeof(pbo->onam));
	pstring[sizeof(pbo->onam)] = 0;
    } else {
	strcpy(pstring,"Illegal Value");
    }
    return(0L);
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
    return(0L);
}

static long alarm(pbo)
    struct boRecord	*pbo;
{
	float	ftemp;

	/* check for a hardware alarm */
	if (pbo->stat == WRITE_ALARM || pbo->stat == READ_ALARM) return(0);

        if (pbo->val == pbo->lalm){
                /* no new message for COS alarms */
                if (pbo->stat == COS_ALARM){
                        pbo->stat = NO_ALARM;
                        pbo->sevr = NO_ALARM;
                }
                return;
        }

        /* set last alarmed value */
        pbo->lalm = pbo->val;

        /* check for  state alarm */
        if (pbo->val == 0){
                if (pbo->zsv != NO_ALARM){
                        pbo->stat = STATE_ALARM;
                        pbo->sevr = pbo->zsv;
                        pbo->achn = 1;
                        return;
                }
        }else{
                if (pbo->osv != NO_ALARM){
                        pbo->stat = STATE_ALARM;
                        pbo->sevr = pbo->osv;
                        pbo->achn = 1;
                        return;
                }
        }

        /* check for cos alarm */
        if (pbo->cosv != NO_ALARM){
                pbo->sevr = pbo->cosv;
                pbo->stat = COS_ALARM;
                pbo->achn = 1;
                return;
        }

        /* check for change from alarm to no alarm */
        if (pbo->sevr != NO_ALARM){
                pbo->sevr = NO_ALARM;
                pbo->stat = NO_ALARM;
                pbo->achn = 1;
                return(0);
        }

	return(0);
}

static long monitor(pbo)
    struct boRecord	*pbo;
{
	unsigned short	monitor_mask;

	/* anyone waiting for an event on this record */
	if (pbo->mlis.count == 0) return(0L);

	/* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (pbo->achn){
		/* post events for alarm condition change and value change */
		monitor_mask = DBE_ALARM | DBE_VALUE | DBE_LOG;

		/* post stat and sevr fields */
		db_post_events(pbo,&pbo->stat,DBE_VALUE);
		db_post_events(pbo,&pbo->sevr,DBE_VALUE);

		/* update last value monitored */
		pbo->mlst = pbo->val;
        /* check for value change */
        }else if (pbo->mlst != pbo->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);

                /* update last value monitored */
                pbo->mlst = pbo->val;
        }

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(pbo,&pbo->val,monitor_mask);
		db_post_events(pbo,&pbo->rval,monitor_mask);
                db_post_events(pbo,&pbo->rbv,monitor_mask);
	}
	return(0L);
}
