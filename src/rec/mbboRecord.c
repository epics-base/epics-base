/*************************************************************************\
* Copyright (c) 2009 Helmholtz-Zentrum Berlin fuer Materialien und Energie.
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* recMbbo.c - Record Support Routines for multi bit binary Output records */
/*
 *      Original Author: Bob Dalesio
 *      Date:            7-17-87
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
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#include "menuOmsl.h"
#include "menuIvoa.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "mbboRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(mbboRecord *, int);
static long process(mbboRecord *);
static long special(DBADDR *, int);
#define get_value NULL
static long cvt_dbaddr(DBADDR *);
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

rset mbboRSET={
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
epicsExportAddress(rset,mbboRSET);

struct mbbodset { /* multi bit binary output dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;  /*returns: (0,2)=>(success,success no convert)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo; /*returns: (0,2)=>(success,success no convert)*/
};


static void checkAlarms(mbboRecord *);
static void convert(mbboRecord *);
static void monitor(mbboRecord *);
static long writeValue(mbboRecord *);


static void init_common(mbboRecord *prec)
{
        epicsUInt32   *pstate_values;
	char		*pstate_string;
        short           i;

        /* determine if any states are defined */
        pstate_values = &(prec->zrvl); pstate_string = prec->zrst;
        prec->sdef = FALSE;
        for (i=0; i<16; i++, pstate_string += sizeof(prec->zrst)) {
                if((*(pstate_values+i)!= 0) || (*pstate_string !='\0')) {
                        prec->sdef = TRUE;
                        return;
                }
        }
        return;
}

static long init_record(mbboRecord *prec, int pass)
{
    struct mbbodset *pdset;
    long status;
    int	i;

    if (pass==0) {
        init_common(prec);
        return(0);
    }

    /* mbbo.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (prec->siml.type == CONSTANT) {
	recGblInitConstantLink(&prec->siml,DBF_USHORT,&prec->simm);
    }

    if(!(pdset = (struct mbbodset *)(prec->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)prec,"mbbo: init_record");
	return(S_dev_noDSET);
    }
    /* must have write_mbbo function defined */
    if( (pdset->number < 5) || (pdset->write_mbbo == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)prec,"mbbo: init_record");
	return(S_dev_missingSup);
    }
    if (prec->dol.type == CONSTANT){
	if(recGblInitConstantLink(&prec->dol,DBF_USHORT,&prec->val))
	    prec->udf = FALSE;
    }

    /* initialize mask*/
    prec->mask = 0;
    for (i=0; i<prec->nobt; i++) {
        prec->mask <<= 1; /* shift left 1 bit*/
        prec->mask |= 1;  /* set low order bit*/
    }
    if( pdset->init_record ) {
	epicsUInt32 rval;

	status=(*pdset->init_record)(prec);
        /* init_record might set status */
	init_common(prec);
	if(status==0){
		rval = prec->rval;
		if(prec->shft>0) rval >>= prec->shft;
		if (prec->sdef){
			epicsUInt32   *pstate_values;
			short           i;

			pstate_values = &(prec->zrvl);
			prec->val = 65535;        /* initalize to unknown state*/
			for (i = 0; i < 16; i++){
				if (*pstate_values == rval){
					prec->val = i;
					break;
				}
				pstate_values++;
			}
		}else{
		/* the raw  is the desired val */
		prec->val =  (unsigned short)rval;
		}
		prec->udf = FALSE;
	} else if (status==2) status=0;
    }
    init_common(prec);
    /* convert val to rval */
    convert(prec);
    prec->mlst = prec->val;
    prec->lalm = prec->val;
    prec->oraw = prec->rval;
    prec->orbv = prec->rbv;
    return(0);
}

static long process(mbboRecord *prec)
{
    struct mbbodset	*pdset = (struct mbbodset *)(prec->dset);
    long		status=0;
    unsigned char    pact=prec->pact;

    if( (pdset==NULL) || (pdset->write_mbbo==NULL) ) {
	prec->pact=TRUE;
	recGblRecordError(S_dev_missingSup,(void *)prec,"write_mbbo");
	return(S_dev_missingSup);
    }

    if (!prec->pact) {
	if (prec->dol.type != CONSTANT && prec->omsl == menuOmslclosed_loop) {
	    long status;
	    unsigned short val;

	    status = dbGetLink(&prec->dol,DBR_USHORT, &val,0,0);
	    if(status==0) {
		prec->val= val;
		prec->udf= FALSE;
	    } else {
		recGblSetSevr(prec,LINK_ALARM,INVALID_ALARM);
		goto CONTINUE;
	    }
	}
	if(prec->udf==TRUE) {
		recGblSetSevr(prec,UDF_ALARM,INVALID_ALARM);
		goto CONTINUE;
	}
	/* convert val to rval */
	convert(prec);
    }

CONTINUE:
    /* check for alarms */
    checkAlarms(prec);

    if (prec->nsev < INVALID_ALARM )
            status=writeValue(prec); /* write the new value */
    else {
            switch (prec->ivoa) {
                case (menuIvoaContinue_normally) :
                    status=writeValue(prec); /* write the new value */
                    break;
                case (menuIvoaDon_t_drive_outputs) :
                    break;
                case (menuIvoaSet_output_to_IVOV) :
                    if (prec->pact == FALSE){
			prec->val=prec->ivov;
			convert(prec);
		    }
                    status=writeValue(prec); /* write the new value */
                    break;
                default :
                    status=-1;
                    recGblRecordError(S_db_badField,(void *)prec,
                            "mbbo:process Illegal IVOA field");
            }
    }

    /* check if device support set pact */
    if ( !pact && prec->pact ) return(0);
    prec->pact = TRUE;

    recGblGetTimeStamp(prec);
    /* check event list */
    monitor(prec);
    /* process the forward scan link record */
    recGblFwdLink(prec);
    prec->pact=FALSE;
    return(status);
}

static long special(DBADDR *paddr, int after)
{
    mbboRecord     *prec = (mbboRecord *)(paddr->precord);
    int            special_type = paddr->special;
    int            fieldIndex = dbGetFieldIndex(paddr);

    if(!after) return(0);
    switch(special_type) {
    case(SPC_MOD):
        init_common(prec);
        if (fieldIndex >= mbboRecordZRST && fieldIndex <= mbboRecordFFST)
            db_post_events(prec,&prec->val,DBE_PROPERTY);
        return(0);
    default:
        recGblDbaddrError(S_db_badChoice,paddr,"mbbo: special");
        return(S_db_badChoice);
    }
}

static long cvt_dbaddr(DBADDR *paddr)
{
    mbboRecord *prec=(mbboRecord *)paddr->precord;
    int index;

    index = dbGetFieldIndex(paddr);
    if(index!=mbboRecordVAL) {
        recGblDbaddrError(S_db_badField,paddr,"mbbo: cvt_dbaddr");
        return(0);
    }
    if(!prec->sdef)  {
        paddr->field_type = DBF_USHORT;
        paddr->dbr_field_type = DBF_USHORT;
    }
    return(0);
}

static long get_enum_str(DBADDR *paddr, char *pstring)
{
    mbboRecord	*prec=(mbboRecord *)paddr->precord;
    char		*psource;
    int                 index;
    unsigned short      *pfield = (unsigned short *)paddr->pfield;
    unsigned short	val=*pfield;

    index = dbGetFieldIndex(paddr);
    if(index!=mbboRecordVAL) {
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
    mbboRecord	*prec=(mbboRecord *)paddr->precord;
    char		*psource;
    int			i;
    short		no_str;

    no_str=0;
    memset(pes->strs,'\0',sizeof(pes->strs));
    for(i=0,psource=(prec->zrst); i<16; i++, psource += sizeof(prec->zrst) ) {
	strncpy(pes->strs[i],psource,sizeof(prec->zrst));
  	if(*psource!=0)no_str=i+1;
    }
    pes->no_str = no_str;

    return(0);
}
static long put_enum_str(DBADDR *paddr,char *pstring)
{
    mbboRecord     *prec=(mbboRecord *)paddr->precord;
        char              *pstate_name;
        short             i;

        if (prec->sdef){
                pstate_name = prec->zrst;
                for (i = 0; i < 16; i++){
                        if(strncmp(pstate_name,pstring,sizeof(prec->zrst))==0){
                                prec->val = i;
                                return(0);
                        }
                        pstate_name += sizeof(prec->zrst);
                }
        }
	return(S_db_badChoice);
}

static void checkAlarms(mbboRecord *prec)
{
	unsigned short *severities;
	unsigned short	val=prec->val;


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
        if(recGblSetSevr(prec,COS_ALARM,prec->cosv)) return;
        prec->lalm = val;
	return;
}

static void monitor(mbboRecord *prec)
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
        if(prec->orbv!=prec->rbv) {
                db_post_events(prec,&prec->rbv,monitor_mask|DBE_VALUE);
                prec->orbv = prec->rbv;
        }
        return;
}

static void convert(mbboRecord *prec)
{
	epicsUInt32 *pvalues = &(prec->zrvl);

	/* convert val to rval */
	if(prec->sdef) {

	    if(prec->val>15) {
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
		return;
	    }
	    prec->rval = pvalues[prec->val];
	} else prec->rval = (epicsUInt32)(prec->val);
	if(prec->shft>0) prec->rval <<= prec->shft;

	return;
}



static long writeValue(mbboRecord *prec)
{
	long		status;
        struct mbbodset 	*pdset = (struct mbbodset *) (prec->dset);

	if (prec->pact == TRUE){
		status=(*pdset->write_mbbo)(prec);
		return(status);
	}

	status=dbGetLink(&(prec->siml),DBR_USHORT, &(prec->simm),0,0);
	if (status)
		return(status);

	if (prec->simm == menuYesNoNO){
		status=(*pdset->write_mbbo)(prec);
		return(status);
	}
	if (prec->simm == menuYesNoYES){
		status=dbPutLink(&prec->siol,DBR_USHORT, &prec->val,1);
	} else {
		status=-1;
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(prec,SIMM_ALARM,prec->sims);

	return(status);
}
