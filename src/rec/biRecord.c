/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

/* recBi.c - Record Support Routines for Binary Input records */
/*
 *      Original Author: Bob Dalesio
 *      Date:            7-14-89
 *
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
#include "dbFldTypes.h"
#include "dbEvent.h"
#include "devSup.h"
#include "errMdef.h"
#include "menuSimm.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#define GEN_SIZE_OFFSET
#include "biRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"
/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(biRecord *, int);
static long process(biRecord *);
#define special NULL
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
rset biRSET={
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
struct bidset { /* binary input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;/*(0,2)=> success and convert, don't convert)*/
                        /* if convert then raw value stored in rval */
};
epicsExportAddress(rset,biRSET);
static void checkAlarms(biRecord *);
static void monitor(biRecord *);
static long readValue(biRecord *);

static long init_record(biRecord *pbi, int pass)
{
    struct bidset *pdset;
    long status;

    if (pass==0) return(0);

    recGblInitConstantLink(&pbi->siml,DBF_USHORT,&pbi->simm);
    recGblInitConstantLink(&pbi->siol,DBF_USHORT,&pbi->sval);
    if(!(pdset = (struct bidset *)(pbi->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pbi,"bi: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_bi function defined */
    if( (pdset->number < 5) || (pdset->read_bi == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pbi,"bi: init_record");
	return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pbi))) return(status);
    }
    return(0);
}

static long process(biRecord *pbi)
{
	struct bidset	*pdset = (struct bidset *)(pbi->dset);
	long		 status;
	unsigned char    pact=pbi->pact;

	if( (pdset==NULL) || (pdset->read_bi==NULL) ) {
		pbi->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pbi,"read_bi");
		return(S_dev_missingSup);
	}

	status=readValue(pbi); /* read the new value */
	/* check if device support set pact */
	if ( !pact && pbi->pact ) return(0);
	pbi->pact = TRUE;

	recGblGetTimeStamp(pbi);
	if(status==0) { /* convert rval to val */
		if(pbi->rval==0) pbi->val =0;
		else pbi->val = 1;
		pbi->udf = FALSE;
	}
	else if(status==2) status=0;
	/* check for alarms */
	checkAlarms(pbi);
	/* check event list */
	monitor(pbi);
	/* process the forward scan link record */
	recGblFwdLink(pbi);

	pbi->pact=FALSE;
	return(status);
}

static long get_enum_str(DBADDR *paddr, char *pstring)
{
    biRecord	*pbi=(biRecord *)paddr->precord;
    int                 index;
    unsigned short      *pfield = (unsigned short *)paddr->pfield;


    index = dbGetFieldIndex(paddr);
    if(index!=biRecordVAL) {
	strcpy(pstring,"Illegal_Value");
    } else if(*pfield==0) {
	strncpy(pstring,pbi->znam,sizeof(pbi->znam));
	pstring[sizeof(pbi->znam)] = 0;
    } else if(*pfield==1) {
	strncpy(pstring,pbi->onam,sizeof(pbi->onam));
	pstring[sizeof(pbi->onam)] = 0;
    } else {
	strcpy(pstring,"Illegal_Value");
    }
    return(0);
}

static long get_enum_strs(DBADDR *paddr,struct dbr_enumStrs *pes)
{
    biRecord	*pbi=(biRecord *)paddr->precord;

    pes->no_str = 2;
    memset(pes->strs,'\0',sizeof(pes->strs));
    strncpy(pes->strs[0],pbi->znam,sizeof(pbi->znam));
    if(*pbi->znam!=0) pes->no_str=1;
    strncpy(pes->strs[1],pbi->onam,sizeof(pbi->onam));
    if(*pbi->onam!=0) pes->no_str=2;
    return(0);
}

static long put_enum_str(DBADDR *paddr, char *pstring)
{
    biRecord     *pbi=(biRecord *)paddr->precord;

    if(strncmp(pstring,pbi->znam,sizeof(pbi->znam))==0) pbi->val = 0;
    else  if(strncmp(pstring,pbi->onam,sizeof(pbi->onam))==0) pbi->val = 1;
    else return(S_db_badChoice);
    pbi->udf=FALSE;
    return(0);
}


static void checkAlarms(biRecord *pbi)
{
	unsigned short val = pbi->val;


        if(pbi->udf == TRUE){
                recGblSetSevr(pbi,UDF_ALARM,INVALID_ALARM);
                return;
        }

	if(val>1)return;
        /* check for  state alarm */
        if (val == 0){
                recGblSetSevr(pbi,STATE_ALARM,pbi->zsv);
        }else{
                recGblSetSevr(pbi,STATE_ALARM,pbi->osv);
        }

        /* check for cos alarm */
	if(val == pbi->lalm) return;
        recGblSetSevr(pbi,COS_ALARM,pbi->cosv);
	pbi->lalm = val;
	return;
}

static void monitor(biRecord *pbi)
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(pbi);
        /* check for value change */
        if (pbi->mlst != pbi->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                pbi->mlst = pbi->val;
        }

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(pbi,&pbi->val,monitor_mask);
	}
	if(pbi->oraw!=pbi->rval) {
		db_post_events(pbi,&pbi->rval,
		    monitor_mask|DBE_VALUE|DBE_LOG);
		pbi->oraw = pbi->rval;
	}
	return;
}

static long readValue(biRecord *pbi)
{
	long		status;
        struct bidset 	*pdset = (struct bidset *) (pbi->dset);

	if (pbi->pact == TRUE){
		status=(*pdset->read_bi)(pbi);
		return(status);
	}

	status = dbGetLink(&(pbi->siml),DBR_USHORT, &(pbi->simm),0,0);
	if (status)
		return(status);

	if (pbi->simm == menuSimmNO){
		status=(*pdset->read_bi)(pbi);
		return(status);
	}
	if (pbi->simm == menuSimmYES){
		status=dbGetLink(&(pbi->siol),DBR_ULONG,&(pbi->sval),0,0);
		if (status==0){
			pbi->val=(unsigned short)pbi->sval;
			pbi->udf=FALSE;
		}
                status=2; /* dont convert */
	}
	else if (pbi->simm == menuSimmRAW){
		status=dbGetLink(&(pbi->siol),DBR_ULONG,&(pbi->sval),0,0);
		if (status==0){
			pbi->rval=pbi->sval;
			pbi->udf=FALSE;
		}
		status=0; /* convert since we've written RVAL */
	} else {
		status=-1;
		recGblSetSevr(pbi,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
	recGblSetSevr(pbi,SIMM_ALARM,pbi->sims);

	return(status);
}
