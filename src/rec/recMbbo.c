/* recMbbo.c */
/* share/src/rec $Id$ */

/* recMbbo.c - Record Support Routines for multi bit binary Output records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            7-17-87
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
 * .17  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .18  02-05-92	jba	Changed function arguments from paddr to precord 
 * .19  02-28-92	jba	ANSI C changes
 * .20  04-10-92        jba     pact now used to test for asyn processing, not status
 * .21  04-18-92        jba     removed process from dev init_record parms
 * .22  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .23  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .24  08-14-92        jba     Added simulation processing
 * .25  08-19-92        jba     Added code for invalid alarm output action
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include	<mbboRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
static long get_value();
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

struct rset mbboRSET={
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

struct mbbodset { /* multi bit binary output dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;  /*returns: (0,2)=>(success,success no convert)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo; /*returns: (0,2)=>(success,success no convert)*/
};


static void alarm();
static void convert();
static void monitor();
static long writeValue();


static void init_common(pmbbo)
    struct mbboRecord   *pmbbo;
{
        unsigned long   *pstate_values;
        short           i;

        /* determine if any states are defined */
        pstate_values = &(pmbbo->zrvl);
        pmbbo->sdef = FALSE;
        for (i=0; i<16; i++) {
                if (*(pstate_values+i)!= 0) {
                        pmbbo->sdef = TRUE;
                        return;
                }
        }
        return;
}

static long init_record(pmbbo,pass)
    struct mbboRecord	*pmbbo;
    int pass;
{
    struct mbbodset *pdset;
    long status;
    int	i;

    if (pass==0) return(0);

    /* mbbo.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    switch (pmbbo->siml.type) {
    case (CONSTANT) :
        pmbbo->simm = pmbbo->siml.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pmbbo->siml), (void *) pmbbo, "SIMM");
	if(status) return(status);
	break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pmbbo,
                "mbbo: init_record Illegal SIML field");
        return(S_db_badField);
    }

    /* mbbo.siol may be a PV_LINK */
    if (pmbbo->siol.type == PV_LINK){
        status = dbCaAddOutlink(&(pmbbo->siol), (void *) pmbbo, "VAL");
	if(status) return(status);
    }

    if(!(pdset = (struct mbbodset *)(pmbbo->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pmbbo,"mbbo: init_record");
	return(S_dev_noDSET);
    }
    /* must have write_mbbo function defined */
    if( (pdset->number < 5) || (pdset->write_mbbo == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pmbbo,"mbbo: init_record");
	return(S_dev_missingSup);
    }
    if (pmbbo->dol.type == CONSTANT){
	pmbbo->val = pmbbo->dol.value.value;
	pmbbo->udf = FALSE;
    }
    /* initialize mask*/
    pmbbo->mask = 0;
    for (i=0; i<pmbbo->nobt; i++) {
        pmbbo->mask <<= 1; /* shift left 1 bit*/
        pmbbo->mask |= 1;  /* set low order bit*/
    }
    if( pdset->init_record ) {
	unsigned long rval;

	status=(*pdset->init_record)(pmbbo);
        /* init_record might set status */
        init_common(pmbbo);
	if(status==0){
		rval = pmbbo->rval;
		if(pmbbo->shft>0) rval >>= pmbbo->shft;
		if (pmbbo->sdef){
			unsigned long   *pstate_values;
			short           i;

			pstate_values = &(pmbbo->zrvl);
			pmbbo->val = 65535;        /* initalize to unknown state*/
			for (i = 0; i < 16; i++){
				if (*pstate_values == rval){
					pmbbo->val = i;
					break;
				}
				pstate_values++;
			}
		}else{
		/* the raw  is the desired val */
		pmbbo->val =  (unsigned short)rval;
		}
		pmbbo->udf = FALSE;
	} else if (status==2) status=0;
    }
    init_common(pmbbo);
    return(0);
}

static long process(pmbbo)
    struct mbboRecord     *pmbbo;
{
    struct mbbodset	*pdset = (struct mbbodset *)(pmbbo->dset);
    long		status=0;
    unsigned char    pact=pmbbo->pact;

    if( (pdset==NULL) || (pdset->write_mbbo==NULL) ) {
	pmbbo->pact=TRUE;
	recGblRecordError(S_dev_missingSup,(void *)pmbbo,"write_mbbo");
	return(S_dev_missingSup);
    }

    if (!pmbbo->pact) {
	if(pmbbo->dol.type==DB_LINK && pmbbo->omsl==CLOSED_LOOP){
	    long options=0;
	    long nRequest=1;
	    long status;
	    unsigned short val;

	    pmbbo->pact = TRUE;
	    status = dbGetLink(&pmbbo->dol.value.db_link,(struct dbCommon *)pmbbo,DBR_USHORT,
			&val,&options,&nRequest);
	    pmbbo->pact = FALSE;
	    if(status==0) {
		pmbbo->val= val;
		pmbbo->udf= FALSE;
	    } else {
		recGblSetSevr(pmbbo,LINK_ALARM,INVALID_ALARM);
		goto CONTINUE;
	    }
	}
	if(pmbbo->udf==TRUE) {
		recGblSetSevr(pmbbo,UDF_ALARM,INVALID_ALARM);
		goto CONTINUE;
	}
	/* convert val to rval */
	convert(pmbbo);
    }

CONTINUE:
    /* check for alarms */
    alarm(pmbbo);

    if (pmbbo->nsev < INVALID_ALARM )
            status=writeValue(pmbbo); /* write the new value */
    else {
            switch (pmbbo->ivoa) {
                case (IVOA_CONTINUE) :
                    status=writeValue(pmbbo); /* write the new value */
                    break;
                case (IVOA_NO_OUTPUT) :
                    break;
                case (IVOA_OUTPUT_IVOV) :
                    if (pmbbo->pact == FALSE){
			pmbbo->val=pmbbo->ivov;
			convert(pmbbo);
		    }
                    status=writeValue(pmbbo); /* write the new value */
                    break;
                default :
                    status=-1;
                    recGblRecordError(S_db_badField,(void *)pmbbo,
                            "ao:process Illegal IVOA field");
            }
    }

    /* check if device support set pact */
    if ( !pact && pmbbo->pact ) return(0);
    pmbbo->pact = TRUE;

    tsLocalTime(&pmbbo->time);
    /* check event list */
    monitor(pmbbo);
    /* process the forward scan link record */
    recGblFwdLink(pmbbo);
    pmbbo->pact=FALSE;
    return(status);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int           after;
{
    struct mbboRecord     *pmbbo = (struct mbboRecord *)(paddr->precord);
    int                 special_type = paddr->special;

    if(!after) return(0);
    switch(special_type) {
    case(SPC_MOD):
        init_common(pmbbo);
        return(0);
    default:
        recGblDbaddrError(S_db_badChoice,paddr,"mbbo: special");
        return(S_db_badChoice);
    }
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

    if(val<= 15) {
	psource = (pmbbo->zrst);
	psource += (val * sizeof(pmbbo->zrst));
	strncpy(pstring,psource,sizeof(pmbbo->zrst));
    } else {
	strcpy(pstring,"Illegal Value");
    }
    return(0);
}

static long get_enum_strs(paddr,pes)
    struct dbAddr *paddr;
    struct dbr_enumStrs *pes;
{
    struct mbboRecord	*pmbbo=(struct mbboRecord *)paddr->precord;
    char		*psource;
    int			i;
    short		no_str;

    no_str=0;
    memset(pes->strs,'\0',sizeof(pes->strs));
    for(i=0,psource=(pmbbo->zrst); i<16; i++, psource += sizeof(pmbbo->zrst) ) {
	strncpy(pes->strs[i],psource,sizeof(pmbbo->zrst));
  	if(*psource!=0)no_str=i+1;
    }
    pes->no_str = no_str;

    return(0);
}
static long put_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char          *pstring;
{
    struct mbboRecord     *pmbbo=(struct mbboRecord *)paddr->precord;
        char              *pstate_name;
        short             i;

        if (pmbbo->sdef){
                pstate_name = pmbbo->zrst;
                for (i = 0; i < 16; i++){
                        if(strncmp(pstate_name,pstring,sizeof(pmbbo->zrst))==0){
                                pmbbo->val = i;
                                return(0);
                        }
                        pstate_name += sizeof(pmbbo->zrst);
                }
        }
	return(S_db_badChoice);
}

static void alarm(pmbbo)
    struct mbboRecord	*pmbbo;
{
	unsigned short *severities;
	unsigned short	val=pmbbo->val;


        /* check for  state alarm */
        /* unknown state */
        if (val > 15){
                recGblSetSevr(pmbbo,STATE_ALARM,pmbbo->unsv);
        } else {
        	/* in a state which is an error */
        	severities = (unsigned short *)&(pmbbo->zrsv);
                recGblSetSevr(pmbbo,STATE_ALARM,severities[pmbbo->val]);
	}

        /* check for cos alarm */
	if(val == pmbbo->lalm) return;
        if(recGblSetSevr(pmbbo,COS_ALARM,pmbbo->cosv)) return;
        pmbbo->lalm = val;
	return;
}

static void monitor(pmbbo)
    struct mbboRecord	*pmbbo;
{
	unsigned short	monitor_mask;
        short           stat,sevr,nsta,nsev;

        /* get previous stat and sevr  and new stat and sevr*/
        recGblResetSevr(pmbbo,stat,sevr,nsta,nsev);

	monitor_mask = 0;

        /* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev){
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and sevr fields */
                db_post_events(pmbbo,&pmbbo->stat,DBE_VALUE);
                db_post_events(pmbbo,&pmbbo->sevr,DBE_VALUE);
        }
        /* check for value change */
        if (pmbbo->mlst != pmbbo->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                pmbbo->mlst = pmbbo->val;
        }
        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pmbbo,&pmbbo->val,monitor_mask);
	}
        if(pmbbo->oraw!=pmbbo->rval) {
                db_post_events(pmbbo,&pmbbo->rval,monitor_mask|DBE_VALUE);
                pmbbo->oraw = pmbbo->rval;
        }
        if(pmbbo->orbv!=pmbbo->rbv) {
                db_post_events(pmbbo,&pmbbo->rbv,monitor_mask|DBE_VALUE);
                pmbbo->orbv = pmbbo->rbv;
        }
        return;
}

static void convert(pmbbo)
	struct mbboRecord  *pmbbo;
{
	unsigned long *pvalues = &(pmbbo->zrvl);

	/* convert val to rval */
	if(pmbbo->sdef) {

	    if(pmbbo->val>15) {
		recGblSetSevr(pmbbo,SOFT_ALARM,INVALID_ALARM);
		return;
	    }
	    pmbbo->rval = pvalues[pmbbo->val];
	} else pmbbo->rval = (unsigned long)(pmbbo->val);
	if(pmbbo->shft>0) pmbbo->rval <<= pmbbo->shft;

	return;
}



static long writeValue(pmbbo)
	struct mbboRecord	*pmbbo;
{
	long		status;
        struct mbbodset 	*pdset = (struct mbbodset *) (pmbbo->dset);
	long            nRequest=1;

	if (pmbbo->pact == TRUE){
		status=(*pdset->write_mbbo)(pmbbo);
		return(status);
	}

	status=recGblGetLinkValue(&(pmbbo->siml),
		(void *)pmbbo,DBR_ENUM,&(pmbbo->simm),&nRequest);
	if (status)
		return(status);

	if (pmbbo->simm == NO){
		status=(*pdset->write_mbbo)(pmbbo);
		return(status);
	}
	if (pmbbo->simm == YES){
		status=recGblPutLinkValue(&(pmbbo->siol),
				(void *)pmbbo,DBR_USHORT,&(pmbbo->val),&nRequest);
	} else {
		status=-1;
		recGblSetSevr(pmbbo,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pmbbo,SIMM_ALARM,pmbbo->sims);

	return(status);
}
