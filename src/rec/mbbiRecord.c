/*************************************************************************\
* Copyright (c) 2009 Helmholtz-Zentrum Berlin fuer Materialien und Energie.
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

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

static void init_common(mbbiRecord *prec)
{
        epicsUInt32 	*pstate_values;
	char		*pstate_string;
        short  		i;

        /* determine if any states are defined */
        pstate_values = &(prec->zrvl); pstate_string = prec->zrst;
        prec->sdef = FALSE;
        for (i=0; i<16; i++, pstate_string += sizeof(prec->zrst)) {
                if((*(pstate_values+i) != 0) || (*pstate_string !='\0')) {
			prec->sdef = TRUE;
			return;
		}
	}
	return;
}

static long init_record(mbbiRecord *prec, int pass)
{
    struct mbbidset *pdset;
    long status;

    if (pass==0) return(0);

    if (prec->siml.type == CONSTANT) {
	recGblInitConstantLink(&prec->siml,DBF_USHORT,&prec->simm);
    }
    if (prec->siol.type == CONSTANT) {
	recGblInitConstantLink(&prec->siol,DBF_USHORT,&prec->sval);
    }
    if(!(pdset = (struct mbbidset *)(prec->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)prec,"mbbi: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_mbbi function defined */
    if( (pdset->number < 5) || (pdset->read_mbbi == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)prec,"mbbi: init_record");
	return(S_dev_missingSup);
    }
    /* initialize mask*/
    prec->mask = (1 << prec->nobt) - 1;

    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(prec))) return(status);
    }
    init_common(prec);
    prec->mlst = prec->val;
    prec->lalm = prec->val;
    prec->oraw = prec->rval;
    return(0);
}

static long process(mbbiRecord *prec)
{
	struct mbbidset	*pdset = (struct mbbidset *)(prec->dset);
	long		status;
	unsigned char    pact=prec->pact;

	if( (pdset==NULL) || (pdset->read_mbbi==NULL) ) {
		prec->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)prec,"read_mbbi");
		return(S_dev_missingSup);
	}

	status=readValue(prec); /* read the new value */
	/* check if device support set pact */
	if ( !pact && prec->pact ) return(0);
	prec->pact = TRUE;

	recGblGetTimeStamp(prec);
	if(status==0) { /* convert the value */
        	epicsUInt32 	*pstate_values;
        	short  		i;
		epicsUInt32 rval = prec->rval;

		prec->udf = FALSE;
		if(prec->shft>0) rval >>= prec->shft;
		if (prec->sdef){
			pstate_values = &(prec->zrvl);
			prec->val = 65535;         /* initalize to unknown state*/
			for (i = 0; i < 16; i++){
				if (*pstate_values == rval){
                               		prec->val = i;
                               		break;
			    	}
			    	pstate_values++;
			}
		}else{
			/* the raw value is the desired value */
			prec->val =  (unsigned short)rval;
		}
	}
	else if(status == 2) status = 0;

	/* check for alarms */
	checkAlarms(prec);

	/* check event list */
	monitor(prec);

	/* process the forward scan link record */
	recGblFwdLink(prec);

	prec->pact=FALSE;
	return(status);
}


static long special(DBADDR *paddr,int after)
{
    mbbiRecord     *prec = (mbbiRecord *)(paddr->precord);
    int            special_type = paddr->special;
    int            fieldIndex = dbGetFieldIndex(paddr);

    if(!after) return(0);
    switch(special_type) {
    case(SPC_MOD):
	init_common(prec);
        if (fieldIndex >= mbbiRecordZRST && fieldIndex <= mbbiRecordFFST)
            db_post_events(prec,&prec->val,DBE_PROPERTY);
        return(0);
    default:
        recGblDbaddrError(S_db_badChoice,paddr,"mbbi: special");
        return(S_db_badChoice);
    }
}

static long get_enum_str(DBADDR *paddr,char* pstring)
{
    mbbiRecord	*prec=(mbbiRecord *)paddr->precord;
    char		*psource;
    int                 index;
    unsigned short      *pfield = (unsigned short *)paddr->pfield;
    unsigned short	val=*pfield;

    index = dbGetFieldIndex(paddr);
    if(index!=mbbiRecordVAL) {
	strcpy(pstring,"Illegal_Value");
    } else if(val<= 15) {
	psource = (prec->zrst);
	psource += (val * sizeof(prec->zrst));
	strncpy(pstring,psource,sizeof(prec->zrst));
    } else {
	strcpy(pstring,"Illegal Value");
    }
    return(0);
}

static long get_enum_strs(DBADDR *paddr, struct dbr_enumStrs *pes)
{
    mbbiRecord	*prec=(mbbiRecord *)paddr->precord;
    char		*psource;
    int			i;
    short		no_str;

    no_str = 0;
    memset(pes->strs,'\0',sizeof(pes->strs));
    for(i=0,psource=(prec->zrst); i<16; i++, psource += sizeof(prec->zrst) ) {
	strncpy(pes->strs[i],psource,sizeof(prec->zrst));
	if(*psource!=0) no_str=i+1;
    }
    pes->no_str=no_str;
    return(0);
}

static long put_enum_str(DBADDR *paddr, char *pstring)
{
    mbbiRecord     *prec=(mbbiRecord *)paddr->precord;
        char              *pstate_name;
        short             i;

        if (prec->sdef){
                pstate_name = prec->zrst;
                for (i = 0; i < 16; i++){
                        if(strncmp(pstate_name,pstring,sizeof(prec->zrst))==0){
        			prec->val = i;
					prec->udf =  FALSE;
                                return(0);
                        }
                	pstate_name += sizeof(prec->zrst);
                }
        }
	return(S_db_badChoice);
}

static void checkAlarms(mbbiRecord *prec)
{
	unsigned short *severities;
	unsigned short	val=prec->val;

        /* check for udf alarm */
        if(prec->udf == TRUE ){
                recGblSetSevr(prec,UDF_ALARM,INVALID_ALARM);
        }

        /* check for  state alarm */
        /* unknown state */
        if (val > 15){
                recGblSetSevr(prec,STATE_ALARM,prec->unsv);
        } else {
        	/* in a state which is an error */
        	severities = (unsigned short *)&(prec->zrsv);
                recGblSetSevr(prec,STATE_ALARM,severities[prec->val]);
	}

        /* check for cos alarm */
	if(val == prec->lalm) return;
        recGblSetSevr(prec,COS_ALARM,prec->cosv);
	prec->lalm = val;
	return;
}

static void monitor(mbbiRecord *prec)
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(prec);
        /* check for value change */
        if (prec->mlst != prec->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                prec->mlst = prec->val;
        }
        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(prec,&prec->val,monitor_mask);
	}
        if(prec->oraw!=prec->rval) {
                db_post_events(prec,&prec->rval,monitor_mask|DBE_VALUE);
                prec->oraw = prec->rval;
        }
        return;
}

static long readValue(mbbiRecord *prec)
{
	long		status;
        struct mbbidset 	*pdset = (struct mbbidset *) (prec->dset);

	if (prec->pact == TRUE){
		status=(*pdset->read_mbbi)(prec);
		return(status);
	}

	status=dbGetLink(&(prec->siml),DBR_USHORT,&(prec->simm),0,0);
	if (status)
		return(status);

	if (prec->simm == menuSimmNO){
		status=(*pdset->read_mbbi)(prec);
		return(status);
	}
	if (prec->simm == menuSimmYES){
		status=dbGetLink(&(prec->siol),DBR_ULONG,&(prec->sval),0,0);
		if (status==0){
			prec->val=(unsigned short)prec->sval;
			prec->udf=FALSE;
		}
                status=2; /* dont convert */
	}
	else if (prec->simm == menuSimmRAW){
		status=dbGetLink(&(prec->siol),DBR_ULONG,&(prec->sval),0,0);
		if (status==0){
			prec->rval=prec->sval;
			prec->udf=FALSE;
		}
                status=0; /* convert since we've written RVAL */
	} else {
		status=-1;
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(prec,SIMM_ALARM,prec->sims);

	return(status);
}
