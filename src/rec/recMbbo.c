
/* recMbbo.c */
/* share/src/rec $Id$ */

/* recMbbo.c - Record Support Routines for multi bit binary Output records
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
 * .01  11-28-88        lrd     add desired value fetched
 * .02  12-12-88        lrd     lock the record while processing
 * .03  12-15-88        lrd     Process the forward scan link
 * .04  12-23-88        lrd     Alarm on locked MAX_LOCKED times
 * .05  01-09-89        lrd     Fix direct value outputs so that they shift the
 *                              value into the correct bit position
 * .06  01-13-89        lrd     delete db_write_mbbo and db_write_mbbo
 * .07  01-20-89        lrd     fixed vx inlcudes
 * .08  03-03-89        lrd     add supervisory/closed loop control
 * .09  03-29-89        lrd     make hardware errors MAJOR
 *                              remove hw severity spec from database
 * .10  04-07-89        lrd     add monitor service
 * .11  05-03-89        lrd     removed process mask from arg list
 * .12  05-30-89        lrd     fixed mask for allen-bradley IO
 * .13  01-05-90        joh,lrd fixed write_mbbo() to set rval
 * .14  02-08-90        lrd/cr  fixed the mbbo to read at initialization for
 *                              Allen-Bradley and added PLC support
 * .15  04-11-90        lrd     make locals static
 * .16  10-11-90	mrk	make changes for new record and device support
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
#include	<mbboRecord.h>

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

struct rset mbboRSET={
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

struct mbbodset { /* multi bit binary input dset */
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;/*(-1,0,1)=>(failure,success,don't Continue*/
};

/* the following definitions must match those in choiceGbl.ascii */
#define OUTPUT_FULL 0
#define CLOSED_LOOP 1

static long report(fp,paddr)
    FILE	  *fp;
    struct dbAddr *paddr;
{
    struct mbboRecord	*pmbbo=(struct mbboRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    if(fprintf(fp,"VAL  %d\n",pmbbo->val)) return(-1);
    if(recGblReportLink(fp,"OUT ",&(pmbbo->out))) return(-1);
    if(recGblReportLink(fp,"FLNK",&(pmbbo->flnk))) return(-1);
    if(fprintf(fp,"RVAL 0x%-8X\n",
	pmbbo->rval)) return(-1);
    return(0);
}

static long init_record(pmbbo)
    struct mbboRecord	*pmbbo;
{
    struct mbbodset *pdset;
    long status;

    if(!(pdset = (struct mbbodset *)(pmbbo->dset))) {
	recGblRecordError(S_dev_noDSET,pmbbo,"mbbo: init_record");
	return(S_dev_noDSET);
    }
    /* must have write_mbbo function defined */
    if( (pdset->number < 5) || (pdset->write_mbbo == NULL) ) {
	recGblRecordError(S_dev_missingSup,pmbbo,"mbbo: init_record");
	return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pmbbo))) return(status);
    }
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct mbboRecord	*pmbbo=(struct mbboRecord *)(paddr->precord);
	struct mbbodset	*pdset = (struct mbbodset *)(pmbbo->dset);
	long		status;
	unsigned long   *pvalues;
	short      states_defined,i;

	if( (pdset==NULL) || (pdset->write_mbbo==NULL) ) {
		pmbbo->pact=TRUE;
		recGblRecordError(S_dev_missingSup,pmbbo,"write_mbbo");
		return(S_dev_missingSup);
	}

	status=(*pdset->write_mbbo)(pmbbo); /* read the new value */
	pmbbo->pact = TRUE;

	/* status is one if an asynchronous record is being processed*/
	if(status==1) return(1);
	else if(status == -1) {
		if(pmbbo->stat != READ_ALARM) {/* error. set alarm condition */
			pmbbo->stat = READ_ALARM;
			pmbbo->sevr = MAJOR_ALARM;
			pmbbo->achn=1;
		}
	}
	else if(status!=0) return(status);
	else if(pmbbo->stat == READ_ALARM) {
		pmbbo->stat = NO_ALARM;
		pmbbo->sevr = NO_ALARM;
		pmbbo->achn=1;
	}

	/* determine if any states are defined */
	pvalues = &(pmbbo->zrvl);
	states_defined = FALSE;
	for (i=0; (i<16) && (!states_defined); i++)
	    if (*(pvalues+i)) states_defined = TRUE;

	/* convert the value */
	if (states_defined){
	    pvalues = (unsigned long *)&(pmbbo->zrvl);
	    pmbbo->rbv = -1;        /* initialize to unknown state */
	    for (i = 0; i < 16; i++,pvalues++){
		if (*pvalues == pmbbo->rval){
		    pmbbo->rbv = i;
		    break;
		}
	    }
	}else{
	    pmbbo->rbv = pmbbo->rval;
	}

	/* check for alarms */
	alarm(pmbbo);


	/* check event list */
	if(!pmbbo->disa) status = monitor(pmbbo);

	/* process the forward scan link record */
	if (pmbbo->flnk.type==DB_LINK) dbScanPassive(&pmbbo->flnk.value);

	pmbbo->pact=FALSE;
	return(status);
}

static long get_value(pmbbo,pvdes)
    struct mbboRecord		*pmbbo;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_ENUM;
    pvdes->no_elements=1;
    (unsigned short *)(pvdes->pvalue) = &pmbbo->val;
    return(0);
}

static long get_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char	  *pstring;
{
    struct mbboRecord	*pmbbo=(struct mbboRecord *)paddr->precord;
    char		*psource;
    unsigned short	val=pmbbo->val;

    if( val>0 && val<= 15) {
	psource = (pmbbo->zrst);
	psource += (val * sizeof(pmbbo->zrst));
	strncpy(pstring,psource,sizeof(pmbbo->zrst));
    } else {
	strcpy(pstring,"Illegal Value");
    }
    return(0L);
}

static long get_enum_strs(paddr,pes)
    struct dbAddr *paddr;
    struct dbr_enumStrs *pes;
{
    struct mbboRecord	*pmbbo=(struct mbboRecord *)paddr->precord;
    char		*psource;
    int			i;

    pes->no_str = 16;
    bzero(pes->strs,sizeof(pes->strs));
    for(i=0,psource=(pmbbo->zrst); i<15; i++, psource += sizeof(pmbbo->zrst) ) 
	strncpy(pes->strs[i],pmbbo->zrst,sizeof(pmbbo->zrst));
    return(0L);
}

static long alarm(pmbbo)
    struct mbboRecord	*pmbbo;
{
	float	ftemp;
	unsigned short *severities;

	/* check for a hardware alarm */
	if (pmbbo->stat == READ_ALARM) return(0);
	if (pmbbo->stat == WRITE_ALARM) return(0);

        if (pmbbo->val == pmbbo->lalm){
                /* no new message for COS alarms */
                if (pmbbo->stat == COS_ALARM){
                        pmbbo->stat = NO_ALARM;
                        pmbbo->sevr = NO_ALARM;
                }
                return;
        }

        /* set last alarmed value */
        pmbbo->lalm = pmbbo->val;

        /* check for  state alarm */
        /* unknown state */
        if ((pmbbo->val < 0) || (pmbbo->val > 15)){
                if (pmbbo->unsv != NO_ALARM){
                        pmbbo->stat = STATE_ALARM;
                        pmbbo->sevr = pmbbo->unsv;
                        pmbbo->achn = 1;
                        return;
                }
        }
        /* in a state which is an error */
        severities = (unsigned short *)&(pmbbo->zrsv);
        if (severities[pmbbo->val] != NO_ALARM){
                pmbbo->stat = STATE_ALARM;
                pmbbo->sevr = severities[pmbbo->val];
                pmbbo->achn = 1;
                return;
        }

        /* check for cos alarm */
        if (pmbbo->cosv != NO_ALARM){
                pmbbo->sevr = pmbbo->cosv;
                pmbbo->stat = COS_ALARM;
                pmbbo->achn = 1;
                return;
        }
        /* check for change from alarm to no alarm */
        if (pmbbo->sevr != NO_ALARM){
                pmbbo->sevr = NO_ALARM;
                pmbbo->stat = NO_ALARM;
                pmbbo->achn = 1;
                return;
        }

	return(0);
}

static long monitor(pmbbo)
    struct mbboRecord	*pmbbo;
{
	unsigned short	monitor_mask;

	/* anyone waiting for an event on this record */
	if (pmbbo->mlis.count == 0) return(0L);

	/* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (pmbbo->achn){
		/* post events for alarm condition change and value change */
		monitor_mask = DBE_ALARM | DBE_VALUE | DBE_LOG;

		/* post stat and sevr fields */
		db_post_events(pmbbo,&pmbbo->stat,DBE_VALUE);
		db_post_events(pmbbo,&pmbbo->sevr,DBE_VALUE);

		/* update last value monitored */
		pmbbo->mlst = pmbbo->val;
        /* check for value change */
        }else if (pmbbo->mlst != pmbbo->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);

                /* update last value monitored */
                pmbbo->mlst = pmbbo->val;
        }

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(pmbbo,&pmbbo->val,monitor_mask);
		db_post_events(pmbbo,&pmbbo->rval,monitor_mask);
	}
	return(0L);
}
