/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recMbbo.c */
/* base/src/rec  $Id$ */

/* recMbbo.c - Record Support Routines for multi bit binary Output records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
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
#define epicsExportSharedSymbols

#include "mbboRecord.h"
#include "menuOmsl.h"
#include "menuIvoa.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
static long cvt_dbaddr();
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

epicsShareDef struct rset mbboRSET={
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


static void checkAlarms();
static void convert();
static void monitor();
static long writeValue();


static void init_common(pmbbo)
    struct mbboRecord   *pmbbo;
{
        unsigned long   *pstate_values;
	char		*pstate_string;
        short           i;

        /* determine if any states are defined */
        pstate_values = &(pmbbo->zrvl); pstate_string = pmbbo->zrst;
        pmbbo->sdef = FALSE;
        for (i=0; i<16; i++, pstate_string += sizeof(pmbbo->zrst)) {
                if((*(pstate_values+i)!= 0) || (*pstate_string !='\0')) {
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
    if (pmbbo->siml.type == CONSTANT) {
	recGblInitConstantLink(&pmbbo->siml,DBF_USHORT,&pmbbo->simm);
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
	if(recGblInitConstantLink(&pmbbo->dol,DBF_USHORT,&pmbbo->val))
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
    /* convert val to rval */
    convert(pmbbo);
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
	if (pmbbo->dol.type != CONSTANT && pmbbo->omsl == menuOmslclosed_loop) {
	    long status;
	    unsigned short val;

	    status = dbGetLink(&pmbbo->dol,DBR_USHORT, &val,0,0);
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
    checkAlarms(pmbbo);

    if (pmbbo->nsev < INVALID_ALARM )
            status=writeValue(pmbbo); /* write the new value */
    else {
            switch (pmbbo->ivoa) {
                case (menuIvoaContinue_normally) :
                    status=writeValue(pmbbo); /* write the new value */
                    break;
                case (menuIvoaDon_t_drive_outputs) :
                    break;
                case (menuIvoaSet_output_to_IVOV) :
                    if (pmbbo->pact == FALSE){
			pmbbo->val=pmbbo->ivov;
			convert(pmbbo);
		    }
                    status=writeValue(pmbbo); /* write the new value */
                    break;
                default :
                    status=-1;
                    recGblRecordError(S_db_badField,(void *)pmbbo,
                            "mbbo:process Illegal IVOA field");
            }
    }

    /* check if device support set pact */
    if ( !pact && pmbbo->pact ) return(0);
    pmbbo->pact = TRUE;

    recGblGetTimeStamp(pmbbo);
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

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct mbboRecord *pmbbo=(struct mbboRecord *)paddr->precord;
    int index;

    index = dbGetFieldIndex(paddr);
    if(index!=mbboRecordVAL) {
        recGblDbaddrError(S_db_badField,paddr,"mbbo: cvt_dbaddr");
        return(0);
    }
    if(!pmbbo->sdef)  {
        paddr->field_type = DBF_USHORT;
        paddr->dbr_field_type = DBF_USHORT;
    }
    return(0);
}

static long get_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char	  *pstring;
{
    struct mbboRecord	*pmbbo=(struct mbboRecord *)paddr->precord;
    char		*psource;
    int                 index;
    unsigned short      *pfield = (unsigned short *)paddr->pfield;
    unsigned short	val=*pfield;

    index = dbGetFieldIndex(paddr);
    if(index!=mbboRecordVAL) {
        strcpy(pstring,"Illegal_Value");
    } else if(val<= 15) {
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

static void checkAlarms(pmbbo)
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

        monitor_mask = recGblResetAlarms(pmbbo);
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

	if (pmbbo->pact == TRUE){
		status=(*pdset->write_mbbo)(pmbbo);
		return(status);
	}

	status=dbGetLink(&(pmbbo->siml),DBR_USHORT, &(pmbbo->simm),0,0);
	if (status)
		return(status);

	if (pmbbo->simm == NO){
		status=(*pdset->write_mbbo)(pmbbo);
		return(status);
	}
	if (pmbbo->simm == YES){
		status=dbPutLink(&pmbbo->siol,DBR_USHORT, &pmbbo->val,1);
	} else {
		status=-1;
		recGblSetSevr(pmbbo,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pmbbo,SIMM_ALARM,pmbbo->sims);

	return(status);
}
