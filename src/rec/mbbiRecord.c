/* recMbbi.c */
/* base/src/rec  $Id$ */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            5-9-88
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
 * .13  10-31-90	mrk	changes for new record and device support
 * .14  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .15  02-05-92	jba	Changed function arguments from paddr to precord 
 * .16  02-28-92	jba	ANSI C changes
 * .17  04-10-92        jba     pact now used to test for asyn processing, not status
 * .18  04-18-92        jba     removed process from dev dev init_record parms
 * .19  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .20  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .21  08-14-92        jba     Added simulation processing
 * .22  03-29-94        mcn     converted to fast links.
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>
#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbEvent.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#define GEN_SIZE_OFFSET
#include	<mbbiRecord.h>
#undef  GEN_SIZE_OFFSET
/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long  special();
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
static long get_enum_str();
static long get_enum_strs();
static long put_enum_str();
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL
struct rset mbbiRSET={
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

struct mbbidset { /* multi bit binary input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;/*(0,2)=>(success, success no convert)*/
};
static void alarm();
static void monitor();
static long readValue();

static void init_common(pmbbi)
    struct mbbiRecord	*pmbbi;
{
        unsigned long 	*pstate_values;
	char		*pstate_string;
        short  		i;

        /* determine if any states are defined */
        pstate_values = &(pmbbi->zrvl); pstate_string = pmbbi->zrst;
        pmbbi->sdef = FALSE;
        for (i=0; i<16; i++, pstate_string += sizeof(pmbbi->zrst)) {
                if((*(pstate_values+i) != 0) || (*pstate_string !='\0')) {
			pmbbi->sdef = TRUE;
			return;
		}
	}
	return;
}

static long init_record(pmbbi,pass)
    struct mbbiRecord	*pmbbi;
    int pass;
{
    struct mbbidset *pdset;
    long status;
    int i;

    if (pass==0) return(0);

    if (pmbbi->siml.type == CONSTANT) {
	recGblInitConstantLink(&pmbbi->siml,DBF_USHORT,&pmbbi->simm);
    }
    if (pmbbi->siol.type == CONSTANT) {
	recGblInitConstantLink(&pmbbi->siol,DBF_USHORT,&pmbbi->sval);
    }
    if(!(pdset = (struct mbbidset *)(pmbbi->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pmbbi,"mbbi: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_mbbi function defined */
    if( (pdset->number < 5) || (pdset->read_mbbi == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pmbbi,"mbbi: init_record");
	return(S_dev_missingSup);
    }
    /* initialize mask*/
    pmbbi->mask = 0;
    for (i=0; i<pmbbi->nobt; i++) {
	pmbbi->mask <<= 1; /* shift left 1 bit*/
	pmbbi->mask |= 1;  /* set low order bit*/
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pmbbi))) return(status);
    }
    init_common(pmbbi);
    return(0);
}

static long process(pmbbi)
        struct mbbiRecord     *pmbbi;
{
	struct mbbidset	*pdset = (struct mbbidset *)(pmbbi->dset);
	long		status;
	unsigned char    pact=pmbbi->pact;

	if( (pdset==NULL) || (pdset->read_mbbi==NULL) ) {
		pmbbi->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pmbbi,"read_mbbi");
		return(S_dev_missingSup);
	}

	status=readValue(pmbbi); /* read the new value */
	/* check if device support set pact */
	if ( !pact && pmbbi->pact ) return(0);
	pmbbi->pact = TRUE;

	recGblGetTimeStamp(pmbbi);
	if(status==0) { /* convert the value */
        	unsigned long 	*pstate_values;
        	short  		i;
		unsigned long rval = pmbbi->rval;

		if(pmbbi->shft>0) rval >>= pmbbi->shft;
		if (pmbbi->sdef){
			pstate_values = &(pmbbi->zrvl);
			pmbbi->val = 65535;         /* initalize to unknown state*/
			pmbbi->udf = TRUE;
			for (i = 0; i < 16; i++){
				if (*pstate_values == rval){
                               		pmbbi->val = i;
                               		pmbbi->udf = FALSE;
                               		break;
			    	}
			    	pstate_values++;
			}
		}else{
			/* the raw value is the desired value */
			pmbbi->val =  (unsigned short)rval;
			pmbbi->udf =  FALSE;
		}
	}
	else if(status == 2) status = 0;

	/* check for alarms */
	alarm(pmbbi);

	/* check event list */
	monitor(pmbbi);

	/* process the forward scan link record */
	recGblFwdLink(pmbbi);

	pmbbi->pact=FALSE;
	return(status);
}


static long special(paddr,after)
    struct dbAddr *paddr;
    int           after;
{
    struct mbbiRecord     *pmbbi = (struct mbbiRecord *)(paddr->precord);
    int                 special_type = paddr->special;

    if(!after) return(0);
    switch(special_type) {
    case(SPC_MOD):
	init_common(pmbbi);
	return(0);
    default:
        recGblDbaddrError(S_db_badChoice,paddr,"mbbi: special");
        return(S_db_badChoice);
    }
}

static long get_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char	  *pstring;
{
    struct mbbiRecord	*pmbbi=(struct mbbiRecord *)paddr->precord;
    char		*psource;
    unsigned short	val=pmbbi->val;

    if(val<= 15) {
	psource = (pmbbi->zrst);
	psource += (val * sizeof(pmbbi->zrst));
	strncpy(pstring,psource,sizeof(pmbbi->zrst));
    } else {
	strcpy(pstring,"Illegal Value");
    }
    return(0);
}

static long get_enum_strs(paddr,pes)
    struct dbAddr *paddr;
    struct dbr_enumStrs *pes;
{
    struct mbbiRecord	*pmbbi=(struct mbbiRecord *)paddr->precord;
    char		*psource;
    int			i;
    short		no_str;

    no_str = 0;
    memset(pes->strs,'\0',sizeof(pes->strs));
    for(i=0,psource=(pmbbi->zrst); i<16; i++, psource += sizeof(pmbbi->zrst) ) {
	strncpy(pes->strs[i],psource,sizeof(pmbbi->zrst));
	if(*psource!=0) no_str=i+1;
    }
    pes->no_str=no_str;
    return(0);
}

static long put_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char          *pstring;
{
    struct mbbiRecord     *pmbbi=(struct mbbiRecord *)paddr->precord;
        char              *pstate_name;
        short             i;

        if (pmbbi->sdef){
                pstate_name = pmbbi->zrst;
                for (i = 0; i < 16; i++){
                        if(strncmp(pstate_name,pstring,sizeof(pmbbi->zrst))==0){
        			pmbbi->val = i;
					pmbbi->udf =  FALSE;
                                return(0);
                        }
                	pstate_name += sizeof(pmbbi->zrst);
                }
        }
	return(S_db_badChoice);
}

static void alarm(pmbbi)
    struct mbbiRecord	*pmbbi;
{
	unsigned short *severities;
	unsigned short	val=pmbbi->val;

        /* check for udf alarm */
        if(pmbbi->udf == TRUE ){
                recGblSetSevr(pmbbi,UDF_ALARM,INVALID_ALARM);
        }

        /* check for  state alarm */
        /* unknown state */
        if (val > 15){
                recGblSetSevr(pmbbi,STATE_ALARM,pmbbi->unsv);
        } else {
        	/* in a state which is an error */
        	severities = (unsigned short *)&(pmbbi->zrsv);
                recGblSetSevr(pmbbi,STATE_ALARM,severities[pmbbi->val]);
	}

        /* check for cos alarm */
	if(val == pmbbi->lalm) return;
        recGblSetSevr(pmbbi,COS_ALARM,pmbbi->cosv);
	pmbbi->lalm = val;
	return;
}

static void monitor(pmbbi)
    struct mbbiRecord	*pmbbi;
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(pmbbi);
        /* check for value change */
        if (pmbbi->mlst != pmbbi->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                pmbbi->mlst = pmbbi->val;
        }
        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pmbbi,&pmbbi->val,monitor_mask);
	}
        if(pmbbi->oraw!=pmbbi->rval) {
                db_post_events(pmbbi,&pmbbi->rval,monitor_mask|DBE_VALUE);
                pmbbi->oraw = pmbbi->rval;
        }
        return;
}

static long readValue(pmbbi)
	struct mbbiRecord	*pmbbi;
{
	long		status;
        struct mbbidset 	*pdset = (struct mbbidset *) (pmbbi->dset);

	if (pmbbi->pact == TRUE){
		status=(*pdset->read_mbbi)(pmbbi);
		return(status);
	}

	status=dbGetLink(&(pmbbi->siml),DBR_USHORT,&(pmbbi->simm),0,0);
	if (status)
		return(status);

	if (pmbbi->simm == NO){
		status=(*pdset->read_mbbi)(pmbbi);
		return(status);
	}
	if (pmbbi->simm == YES){
		status=dbGetLink(&(pmbbi->siol),DBR_USHORT,&(pmbbi->sval),0,0);
		if (status==0){
			pmbbi->val=pmbbi->sval;
			pmbbi->udf=FALSE;
		}
                status=2; /* dont convert */
	} else {
		status=-1;
		recGblSetSevr(pmbbi,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pmbbi,SIMM_ALARM,pmbbi->sims);

	return(status);
}
