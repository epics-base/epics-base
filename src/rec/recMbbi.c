
/* recMbbi.c */
/* share/src/rec $Id$ */

/* recMbbi.c - Record Support Routines for multi bit binary Input records
 *
 * Author: 	Bob Dalesio
 * Date:        5-9-88
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
 * .01  12-12-88        lrd     lock the record while processing
 * .02  12-15-88        lrd     Process the forward scan link
 * .03  12-23-88        lrd     Alarm on locked MAX_LOCKED times
 * .04  01-13-89        lrd     delete db_read_mbbi
 * .05  03-29-89        lrd     make hardware errors MAJOR
 *                              remove hw severity spec from database
 * .06  04-07-89        lrd     add monitor detection
 * .07  05-03-89        lrd     removed process mask from arg list
 * .08  05-29-89        lrd     support 16 states
 * .09  05-30-89        lrd     fixed masks for allen-bradley IO
 * .10  06-06-89        lrd     fixed AB mbbi conversion - signal wrong
 *                              added ability to enter raw numbers if no
 *                              states are defined - like the mbbo
 * .11  12-06-89        lrd     add database fetch support
 * .12  02-08-90        lrd     add Allen-Bradley PLC support
 * .13  10-11-90	mrk	changes for new record and device support
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
#include	<mbbiRecord.h>

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

struct rset mbbiRSET={
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

struct mbbidset { /* multi bit binary input dset */
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;/*(-1,0,1)=>(failure,success,don't Continue*/
};

static long report(fp,paddr)
    FILE	  *fp;
    struct dbAddr *paddr;
{
    struct mbbiRecord	*pmbbi=(struct mbbiRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    if(fprintf(fp,"VAL  %d\n",pmbbi->val)) return(-1);
    if(recGblReportLink(fp,"INP ",&(pmbbi->inp))) return(-1);
    if(recGblReportLink(fp,"FLNK",&(pmbbi->flnk))) return(-1);
    if(fprintf(fp,"RVAL 0x%-8X\n",
	pmbbi->rval)) return(-1);
    return(0);
}

static long init_record(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    struct mbbidset *pdset;
    long status;

    if(!(pdset = (struct mbbidset *)(pmbbi->dset))) {
	recGblRecordError(S_dev_noDSET,pmbbi,"mbbi: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_mbbi function defined */
    if( (pdset->number < 5) || (pdset->read_mbbi == NULL) ) {
	recGblRecordError(S_dev_missingSup,pmbbi,"mbbi: init_record");
	return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pmbbi))) return(status);
    }
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct mbbiRecord	*pmbbi=(struct mbbiRecord *)(paddr->precord);
	struct mbbidset	*pdset = (struct mbbidset *)(pmbbi->dset);
	long		status;
        unsigned long 	*pstate_values;
        short  		i,states_defined;

	if( (pdset==NULL) || (pdset->read_mbbi==NULL) ) {
		pmbbi->pact=TRUE;
		recGblRecordError(S_dev_missingSup,pmbbi,"read_mbbi");
		return(S_dev_missingSup);
	}

	status=(*pdset->read_mbbi)(pmbbi); /* read the new value */
	pmbbi->pact = TRUE;

	/* status is one if an asynchronous record is being processed*/
	if(status==1) return(1);
	else if(status == -1) {
		if(pmbbi->stat != READ_ALARM) {/* error. set alarm condition */
			pmbbi->stat = READ_ALARM;
			pmbbi->sevr = MAJOR_ALARM;
			pmbbi->achn=1;
		}
	}
	else if(status!=0) return(status);
	else if(pmbbi->stat == READ_ALARM) {
		pmbbi->stat = NO_ALARM;
		pmbbi->sevr = NO_ALARM;
		pmbbi->achn=1;
	}

        /* determine if any states are defined */
        pstate_values = &(pmbbi->zrvl);
        states_defined = 0;
        for (i=0; (i<16) && (!states_defined); i++)
                if (*(pstate_values+i)) states_defined = 1;

        /* convert the value */
        if (states_defined){
                pstate_values = &(pmbbi->zrvl);
                for (i = 0; i < 16; i++){
                        if (*pstate_values == pmbbi->rval){
                                pmbbi->val = i;
                                return(0);
                        }
                        pstate_values++;
                }
                pmbbi->val = -2;        /* unknown state-other than init LALM */
        }else{
                /* the raw value is the desired value */
                pmbbi->val = pmbbi->rval;
	}

	/* check for alarms */
	alarm(pmbbi);


	/* check event list */
	if(!pmbbi->disa) status = monitor(pmbbi);

	/* process the forward scan link record */
	if (pmbbi->flnk.type==DB_LINK) dbScanPassive(&pmbbi->flnk.value);

	pmbbi->pact=FALSE;
	return(status);
}

static long get_value(pmbbi,pvdes)
    struct mbbiRecord		*pmbbi;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_ENUM;
    pvdes->no_elements=1;
    (unsigned short *)(pvdes->pvalue) = &pmbbi->val;
    return(0);
}

static long get_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char	  *pstring;
{
    struct mbbiRecord	*pmbbi=(struct mbbiRecord *)paddr->precord;
    char		*psource;
    unsigned short	val=pmbbi->val;

    if( val>0 && val<= 15) {
	psource = (pmbbi->zrst);
	psource += (val * sizeof(pmbbi->zrst));
	strncpy(pstring,psource,sizeof(pmbbi->zrst));
    } else {
	strcpy(pstring,"Illegal Value");
    }
    return(0L);
}

static long get_enum_strs(paddr,pes)
    struct dbAddr *paddr;
    struct dbr_enumStrs *pes;
{
    struct mbbiRecord	*pmbbi=(struct mbbiRecord *)paddr->precord;
    char		*psource;
    int			i;

    pes->no_str = 16;
    bzero(pes->strs,sizeof(pes->strs));
    for(i=0,psource=(pmbbi->zrst); i<15; i++, psource += sizeof(pmbbi->zrst) ) 
	strncpy(pes->strs[i],pmbbi->zrst,sizeof(pmbbi->zrst));
    return(0L);
}

static long alarm(pmbbi)
    struct mbbiRecord	*pmbbi;
{
	float	ftemp;
	unsigned short *severities;

	/* check for a hardware alarm */
	if (pmbbi->stat == READ_ALARM) return(0);

        if (pmbbi->val == pmbbi->lalm){
                /* no new message for COS alarms */
                if (pmbbi->stat == COS_ALARM){
                        pmbbi->stat = NO_ALARM;
                        pmbbi->sevr = NO_ALARM;
                }
                return;
        }

        /* set last alarmed value */
        pmbbi->lalm = pmbbi->val;

        /* check for  state alarm */
        /* unknown state */
        if ((pmbbi->val < 0) || (pmbbi->val > 15)){
                if (pmbbi->unsv != NO_ALARM){
                        pmbbi->stat = STATE_ALARM;
                        pmbbi->sevr = pmbbi->unsv;
                        pmbbi->achn = 1;
                        return;
                }
        }
        /* in a state which is an error */
        severities = (unsigned short *)&(pmbbi->zrsv);
        if (severities[pmbbi->val] != NO_ALARM){
                pmbbi->stat = STATE_ALARM;
                pmbbi->sevr = severities[pmbbi->val];
                pmbbi->achn = 1;
                return;
        }

        /* check for cos alarm */
        if (pmbbi->cosv != NO_ALARM){
                pmbbi->sevr = pmbbi->cosv;
                pmbbi->stat = COS_ALARM;
                pmbbi->achn = 1;
                return;
        }
        /* check for change from alarm to no alarm */
        if (pmbbi->sevr != NO_ALARM){
                pmbbi->sevr = NO_ALARM;
                pmbbi->stat = NO_ALARM;
                pmbbi->achn = 1;
                return;
        }

	return(0);
}

static long monitor(pmbbi)
    struct mbbiRecord	*pmbbi;
{
	unsigned short	monitor_mask;

	/* anyone waiting for an event on this record */
	if (pmbbi->mlis.count == 0) return(0L);

	/* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (pmbbi->achn){
		/* post events for alarm condition change and value change */
		monitor_mask = DBE_ALARM | DBE_VALUE | DBE_LOG;

		/* post stat and sevr fields */
		db_post_events(pmbbi,&pmbbi->stat,DBE_VALUE);
		db_post_events(pmbbi,&pmbbi->sevr,DBE_VALUE);

		/* update last value monitored */
		pmbbi->mlst = pmbbi->val;
        /* check for value change */
        }else if (pmbbi->mlst != pmbbi->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);

                /* update last value monitored */
                pmbbi->mlst = pmbbi->val;
        }

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(pmbbi,&pmbbi->val,monitor_mask);
		db_post_events(pmbbi,&pmbbi->rval,monitor_mask);
	}
	return(0L);
}
