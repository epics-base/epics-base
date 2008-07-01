/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */
/*
 *      Original Author: Bob Dalesio
 *      Date:            5-9-88
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "menuSimm.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#define GEN_SIZE_OFFSET
#include "mbbiRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"
/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(mbbiRecord *, int);
static long process(mbbiRecord *);
static long  special(DBADDR *, int);
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
static long get_enum_str(DBADDR *, char *);
static long get_enum_strs(DBADDR *, struct dbr_enumStrs *);
static long put_enum_str(DBADDR *, char *);
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL
rset mbbiRSET={
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
	get_alarm_double
};
epicsExportAddress(rset,mbbiRSET);

struct mbbidset { /* multi bit binary input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;/*(0,2)=>(success, success no convert)*/
};
static void checkAlarms(mbbiRecord *);
static void monitor(mbbiRecord *);
static long readValue(mbbiRecord *);

static void init_common(mbbiRecord *pmbbi)
{
        epicsUInt32 	*pstate_values;
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

static long init_record(mbbiRecord *pmbbi, int pass)
{
    struct mbbidset *pdset;
    long status;

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
    pmbbi->mask = (1 << pmbbi->nobt) - 1;

    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pmbbi))) return(status);
    }
    init_common(pmbbi);
    return(0);
}

static long process(mbbiRecord *pmbbi)
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
        	epicsUInt32 	*pstate_values;
        	short  		i;
		epicsUInt32 rval = pmbbi->rval;

		pmbbi->udf = FALSE;
		if(pmbbi->shft>0) rval >>= pmbbi->shft;
		if (pmbbi->sdef){
			pstate_values = &(pmbbi->zrvl);
			pmbbi->val = 65535;         /* initalize to unknown state*/
			for (i = 0; i < 16; i++){
				if (*pstate_values == rval){
                               		pmbbi->val = i;
                               		break;
			    	}
			    	pstate_values++;
			}
		}else{
			/* the raw value is the desired value */
			pmbbi->val =  (unsigned short)rval;
		}
	}
	else if(status == 2) status = 0;

	/* check for alarms */
	checkAlarms(pmbbi);

	/* check event list */
	monitor(pmbbi);

	/* process the forward scan link record */
	recGblFwdLink(pmbbi);

	pmbbi->pact=FALSE;
	return(status);
}


static long special(DBADDR *paddr,int after)
{
    mbbiRecord     *pmbbi = (mbbiRecord *)(paddr->precord);
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

static long get_enum_str(DBADDR *paddr,char* pstring)
{
    mbbiRecord	*pmbbi=(mbbiRecord *)paddr->precord;
    char		*psource;
    int                 index;
    unsigned short      *pfield = (unsigned short *)paddr->pfield;
    unsigned short	val=*pfield;

    index = dbGetFieldIndex(paddr);
    if(index!=mbbiRecordVAL) {
	strcpy(pstring,"Illegal_Value");
    } else if(val<= 15) {
	psource = (pmbbi->zrst);
	psource += (val * sizeof(pmbbi->zrst));
	strncpy(pstring,psource,sizeof(pmbbi->zrst));
    } else {
	strcpy(pstring,"Illegal Value");
    }
    return(0);
}

static long get_enum_strs(DBADDR *paddr, struct dbr_enumStrs *pes)
{
    mbbiRecord	*pmbbi=(mbbiRecord *)paddr->precord;
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

static long put_enum_str(DBADDR *paddr, char *pstring)
{
    mbbiRecord     *pmbbi=(mbbiRecord *)paddr->precord;
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

static void checkAlarms(mbbiRecord *pmbbi)
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

static void monitor(mbbiRecord *pmbbi)
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

static long readValue(mbbiRecord *pmbbi)
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

	if (pmbbi->simm == menuSimmNO){
		status=(*pdset->read_mbbi)(pmbbi);
		return(status);
	}
	if (pmbbi->simm == menuSimmYES){
		status=dbGetLink(&(pmbbi->siol),DBR_ULONG,&(pmbbi->sval),0,0);
		if (status==0){
			pmbbi->val=(unsigned short)pmbbi->sval;
			pmbbi->udf=FALSE;
		}
                status=2; /* dont convert */
	}
	else if (pmbbi->simm == menuSimmRAW){
		status=dbGetLink(&(pmbbi->siol),DBR_ULONG,&(pmbbi->sval),0,0);
		if (status==0){
			pmbbi->rval=pmbbi->sval;
			pmbbi->udf=FALSE;
		}
                status=0; /* convert since we've written RVAL */
	} else {
		status=-1;
		recGblSetSevr(pmbbi,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pmbbi,SIMM_ALARM,pmbbi->sims);

	return(status);
}
