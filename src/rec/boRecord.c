/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

/* recBo.c - Record Support Routines for Binary Output records */
/*
 *      Original Author: Bob Dalesio
 *      Date:            7-14-89
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "callback.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#define GEN_SIZE_OFFSET
#include "boRecord.h"
#undef  GEN_SIZE_OFFSET
#include "menuIvoa.h"
#include "menuOmsl.h"
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(boRecord *, int);
static long process(boRecord *);
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
static long get_precision(DBADDR *, long *);
static long get_enum_str(DBADDR *, char *);
static long get_enum_strs(DBADDR *, struct dbr_enumStrs *);
static long put_enum_str(DBADDR *, char *);
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset boRSET={
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
epicsExportAddress(rset,boRSET);

struct bodset { /* binary output dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;  /*returns:(0,2)=>(success,success no convert*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;/*returns: (-1,0)=>(failure,success)*/
};


/* control block for callback*/
typedef struct myCallback {
        CALLBACK        callback;
        struct dbCommon *precord;
}myCallback;

static void checkAlarms(boRecord *);
static void monitor(boRecord *);
static long writeValue(boRecord *);

static void myCallbackFunc(CALLBACK *arg)
{
    myCallback *pcallback;
    boRecord *pbo;

    callbackGetUser(pcallback,arg);
    pbo=(boRecord *)pcallback->precord;
    dbScanLock((struct dbCommon *)pbo);
    if(pbo->pact) {
	if((pbo->val==1) && (pbo->high>0)){
	    myCallback *pcallback;
	    pcallback = (myCallback *)(pbo->rpvt);
            callbackSetPriority(pbo->prio, &pcallback->callback);
            callbackRequestDelayed(&pcallback->callback,(double)pbo->high);
	}
    } else {
	pbo->val = 0;
	dbProcess((struct dbCommon *)pbo);
    }
    dbScanUnlock((struct dbCommon *)pbo);
}

static long init_record(boRecord *pbo,int pass)
{
    struct bodset *pdset;
    long status=0;
    myCallback *pcallback;

    if (pass==0) return(0);

    /* bo.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pbo->siml.type == CONSTANT) {
	recGblInitConstantLink(&pbo->siml,DBF_USHORT,&pbo->simm);
    }

    if(!(pdset = (struct bodset *)(pbo->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pbo,"bo: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_bo functions defined */
    if( (pdset->number < 5) || (pdset->write_bo == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pbo,"bo: init_record");
	return(S_dev_missingSup);
    }
    /* get the initial value */
    if (pbo->dol.type == CONSTANT) {
	unsigned short ival = 0;

	if(recGblInitConstantLink(&pbo->dol,DBF_USHORT,&ival)) {
	    if (ival  == 0)  pbo->val = 0;
	    else  pbo->val = 1;
	    pbo->udf = FALSE;
	}
    }

    pcallback = (myCallback *)(calloc(1,sizeof(myCallback)));
    pbo->rpvt = (void *)pcallback;
    callbackSetCallback(myCallbackFunc,&pcallback->callback);
    callbackSetUser(pcallback,&pcallback->callback);
    pcallback->precord = (struct dbCommon *)pbo;

    if( pdset->init_record ) {
	status=(*pdset->init_record)(pbo);
	if(status==0) {
		if(pbo->rval==0) pbo->val = 0;
		else pbo->val = 1;
		pbo->udf = FALSE;
	} else if (status==2) status=0;
    }
    /* convert val to rval */
    if ( pbo->mask != 0 ) {
	if(pbo->val==0) pbo->rval = 0;
	else pbo->rval = pbo->mask;
    } else pbo->rval = (epicsUInt32)pbo->val;
    return(status);
}

static long process(boRecord *pbo)
{
	struct bodset	*pdset = (struct bodset *)(pbo->dset);
	long		 status=0;
	unsigned char    pact=pbo->pact;

	if( (pdset==NULL) || (pdset->write_bo==NULL) ) {
		pbo->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pbo,"write_bo");
		return(S_dev_missingSup);
	}
        if (!pbo->pact) {
		if ((pbo->dol.type != CONSTANT) && (pbo->omsl == menuOmslclosed_loop)){
			unsigned short val;

			pbo->pact = TRUE;
			status=dbGetLink(&pbo->dol,DBR_USHORT, &val,0,0);
			pbo->pact = FALSE;
			if(status==0){
				pbo->val = val;
				pbo->udf = FALSE;
			}else {
       				recGblSetSevr(pbo,LINK_ALARM,INVALID_ALARM);
			}
		}

		/* convert val to rval */
		if ( pbo->mask != 0 ) {
			if(pbo->val==0) pbo->rval = 0;
			else pbo->rval = pbo->mask;
		} else pbo->rval = (epicsUInt32)pbo->val;
	}

	/* check for alarms */
	checkAlarms(pbo);

        if (pbo->nsev < INVALID_ALARM )
                status=writeValue(pbo); /* write the new value */
        else {
                switch (pbo->ivoa) {
                    case (menuIvoaContinue_normally) :
                        status=writeValue(pbo); /* write the new value */
                        break;
                    case (menuIvoaDon_t_drive_outputs) :
                        break;
                    case (menuIvoaSet_output_to_IVOV) :
                        if(pbo->pact == FALSE){
				/* convert val to rval */
                                pbo->val=pbo->ivov;
				if ( pbo->mask != 0 ) {
					if(pbo->val==0) pbo->rval = 0;
					else pbo->rval = pbo->mask;
				} else pbo->rval = (epicsUInt32)pbo->val;
			}
                        status=writeValue(pbo); /* write the new value */
                        break;
                    default :
                        status=-1;
                        recGblRecordError(S_db_badField,(void *)pbo,
                                "bo:process Illegal IVOA field");
                }
        }

	/* check if device support set pact */
	if ( !pact && pbo->pact ) return(0);
	pbo->pact = TRUE;

	recGblGetTimeStamp(pbo);
	if((pbo->val==1) && (pbo->high>0)){
	    myCallback *pcallback;
	    pcallback = (myCallback *)(pbo->rpvt);
            callbackSetPriority(pbo->prio, &pcallback->callback);
            callbackRequestDelayed(&pcallback->callback,(double)pbo->high);
	}
	/* check event list */
	monitor(pbo);
	/* process the forward scan link record */
	recGblFwdLink(pbo);

	pbo->pact=FALSE;
	return(status);
}

static long get_precision(DBADDR *paddr, long *precision)
{
    boRecord	*pbo=(boRecord *)paddr->precord;

    if(paddr->pfield == (void *)&pbo->high) *precision=2;
    else recGblGetPrec(paddr,precision);
    return(0);
}

static long get_enum_str(DBADDR *paddr, char *pstring)
{
    boRecord	*pbo=(boRecord *)paddr->precord;
    int                 index;
    unsigned short      *pfield = (unsigned short *)paddr->pfield;


    index = dbGetFieldIndex(paddr);
    if(index!=boRecordVAL) {
	strcpy(pstring,"Illegal_Value");
    } else if(*pfield==0) {
	strncpy(pstring,pbo->znam,sizeof(pbo->znam));
	pstring[sizeof(pbo->znam)] = 0;
    } else if(*pfield==1) {
	strncpy(pstring,pbo->onam,sizeof(pbo->onam));
	pstring[sizeof(pbo->onam)] = 0;
    } else {
	strcpy(pstring,"Illegal_Value");
    }
    return(0);
}

static long get_enum_strs(DBADDR *paddr,struct dbr_enumStrs *pes)
{
    boRecord	*pbo=(boRecord *)paddr->precord;

    /*SETTING no_str=0 breaks channel access clients*/
    pes->no_str = 2;
    memset(pes->strs,'\0',sizeof(pes->strs));
    strncpy(pes->strs[0],pbo->znam,sizeof(pbo->znam));
    if(*pbo->znam!=0) pes->no_str=1;
    strncpy(pes->strs[1],pbo->onam,sizeof(pbo->onam));
    if(*pbo->onam!=0) pes->no_str=2;
    return(0);
}
static long put_enum_str(DBADDR *paddr, char *pstring)
{
    boRecord     *pbo=(boRecord *)paddr->precord;

    if(strncmp(pstring,pbo->znam,sizeof(pbo->znam))==0) pbo->val = 0;
    else  if(strncmp(pstring,pbo->onam,sizeof(pbo->onam))==0) pbo->val = 1;
    else return(S_db_badChoice);
    return(0);
}


static void checkAlarms(boRecord *pbo)
{
	unsigned short val = pbo->val;

        /* check for udf alarm */
        if(pbo->udf == TRUE ){
			recGblSetSevr(pbo,UDF_ALARM,INVALID_ALARM);
        }

        /* check for  state alarm */
        if (val == 0){
		recGblSetSevr(pbo,STATE_ALARM,pbo->zsv);
        }else{
		recGblSetSevr(pbo,STATE_ALARM,pbo->osv);
        }

        /* check for cos alarm */
	if(val == pbo->lalm) return;
	recGblSetSevr(pbo,COS_ALARM,pbo->cosv);
	pbo->lalm = val;
        return;
}

static void monitor(boRecord *pbo)
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(pbo);
        /* check for value change */
        if (pbo->mlst != pbo->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                pbo->mlst = pbo->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pbo,&pbo->val,monitor_mask);
        }
	if(pbo->oraw!=pbo->rval) {
		db_post_events(pbo,&pbo->rval,
		    monitor_mask|DBE_VALUE|DBE_LOG);
		pbo->oraw = pbo->rval;
	}
	if(pbo->orbv!=pbo->rbv) {
		db_post_events(pbo,&pbo->rbv,
		    monitor_mask|DBE_VALUE|DBE_LOG);
		pbo->orbv = pbo->rbv;
	}
        return;
}

static long writeValue(boRecord *pbo)
{
	long		status;
        struct bodset 	*pdset = (struct bodset *) (pbo->dset);

	if (pbo->pact == TRUE){
		status=(*pdset->write_bo)(pbo);
		return(status);
	}

	status=dbGetLink(&pbo->siml,DBR_USHORT, &pbo->simm,0,0);
	if (status)
		return(status);

	if (pbo->simm == NO){
		status=(*pdset->write_bo)(pbo);
		return(status);
	}
	if (pbo->simm == YES){
		status=dbPutLink(&(pbo->siol),DBR_USHORT, &(pbo->val),1);
	} else {
		status=-1;
		recGblSetSevr(pbo,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pbo,SIMM_ALARM,pbo->sims);

	return(status);
}
