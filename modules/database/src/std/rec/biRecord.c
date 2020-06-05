/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

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
#include "callback.h"
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
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
static long special(DBADDR *, int);
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
static long get_enum_str(const DBADDR *, char *);
static long get_enum_strs(const DBADDR *, struct dbr_enumStrs *);
static long put_enum_str(const DBADDR *, const char *);
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
	get_alarm_double
};
epicsExportAddress(rset,biRSET);

static void checkAlarms(biRecord *);
static void monitor(biRecord *);
static long readValue(biRecord *);

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct biRecord *prec = (struct biRecord *)pcommon;
    bidset *pdset;
    long status;

    if (pass == 0) return 0;

    recGblInitSimm(pcommon, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);
    recGblInitConstantLink(&prec->siol, DBF_USHORT, &prec->sval);

    if(!(pdset = (bidset *)(prec->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)prec,"bi: init_record");
        return(S_dev_noDSET);
    }
    /* must have read_bi function defined */
    if( (pdset->common.number < 5) || (pdset->read_bi == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)prec,"bi: init_record");
        return(S_dev_missingSup);
    }
    if( pdset->common.init_record ) {
	if((status=(*pdset->common.init_record)(pcommon))) return(status);
    }
    prec->mlst = prec->val;
    prec->lalm = prec->val;
    prec->oraw = prec->rval;
    return(0);
}

static long process(struct dbCommon *pcommon)
{
    struct biRecord *prec = (struct biRecord *)pcommon;
    bidset  *pdset = (bidset *)(prec->dset);
    long            status;
    unsigned char   pact=prec->pact;

    if( (pdset==NULL) || (pdset->read_bi==NULL) ) {
        prec->pact=TRUE;
        recGblRecordError(S_dev_missingSup,(void *)prec,"read_bi");
        return(S_dev_missingSup);
    }

    status=readValue(prec); /* read the new value */
    /* check if device support set pact */
    if ( !pact && prec->pact ) return(0);
    prec->pact = TRUE;

    recGblGetTimeStampSimm(prec, prec->simm, &prec->siol);

    if(status==0) { /* convert rval to val */
        if(prec->rval==0) prec->val =0;
        else prec->val = 1;
        prec->udf = FALSE;
    }
    else if(status==2) status=0;
    /* check for alarms */
    checkAlarms(prec);
    /* check event list */
    monitor(prec);
    /* process the forward scan link record */
    recGblFwdLink(prec);

    prec->pact=FALSE;
    return(status);
}

static long special(DBADDR *paddr, int after)
{
    biRecord    *prec = (biRecord *)(paddr->precord);
    int          special_type = paddr->special;

    switch(special_type) {
    case(SPC_MOD):
        if (dbGetFieldIndex(paddr) == biRecordSIMM) {
            if (!after)
                recGblSaveSimm(prec->sscn, &prec->oldsimm, prec->simm);
            else
                recGblCheckSimm((dbCommon *)prec, &prec->sscn, prec->oldsimm, prec->simm);
            return(0);
        }
    default:
        recGblDbaddrError(S_db_badChoice, paddr, "bi: special");
        return(S_db_badChoice);
    }
}

static long get_enum_str(const DBADDR *paddr, char *pstring)
{
    biRecord        *prec=(biRecord *)paddr->precord;
    int             index;
    unsigned short  *pfield = (unsigned short *)paddr->pfield;


    index = dbGetFieldIndex(paddr);
    if(index!=biRecordVAL) {
        strcpy(pstring,"Illegal_Value");
    } else if(*pfield==0) {
        strncpy(pstring,prec->znam,sizeof(prec->znam));
        pstring[sizeof(prec->znam)] = 0;
    } else if(*pfield==1) {
        strncpy(pstring,prec->onam,sizeof(prec->onam));
        pstring[sizeof(prec->onam)] = 0;
    } else {
        strcpy(pstring,"Illegal_Value");
    }
    return(0);
}

static long get_enum_strs(const DBADDR *paddr,struct dbr_enumStrs *pes)
{
    biRecord    *prec=(biRecord *)paddr->precord;

    pes->no_str = 2;
    memset(pes->strs,'\0',sizeof(pes->strs));
    strncpy(pes->strs[0],prec->znam,sizeof(prec->znam));
    if(*prec->znam!=0) pes->no_str=1;
    strncpy(pes->strs[1],prec->onam,sizeof(prec->onam));
    if(*prec->onam!=0) pes->no_str=2;
    return(0);
}

static long put_enum_str(const DBADDR *paddr, const char *pstring)
{
    biRecord     *prec=(biRecord *)paddr->precord;

    if(strncmp(pstring,prec->znam,sizeof(prec->znam))==0) prec->val = 0;
    else  if(strncmp(pstring,prec->onam,sizeof(prec->onam))==0) prec->val = 1;
    else return(S_db_badChoice);
    prec->udf=FALSE;
    return(0);
}


static void checkAlarms(biRecord *prec)
{
    unsigned short val = prec->val;


    if(prec->udf == TRUE){
            recGblSetSevr(prec,UDF_ALARM,prec->udfs);
            return;
    }

    if(val>1)return;
    /* check for  state alarm */
    if (val == 0){
        recGblSetSevr(prec,STATE_ALARM,prec->zsv);
    }else{
        recGblSetSevr(prec,STATE_ALARM,prec->osv);
    }

        /* check for cos alarm */
    if(val == prec->lalm) return;
        recGblSetSevr(prec,COS_ALARM,prec->cosv);
    prec->lalm = val;
    return;
}

static void monitor(biRecord *prec)
{
    unsigned short  monitor_mask;

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
        db_post_events(prec,&prec->rval,
            monitor_mask|DBE_VALUE|DBE_LOG);
        prec->oraw = prec->rval;
    }
    return;
}

static long readValue(biRecord *prec)
{
    bidset *pdset = (bidset *)prec->dset;
    long status = 0;

    if (!prec->pact) {
        status = recGblGetSimm((dbCommon *)prec, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);
        if (status) return status;
    }

    switch (prec->simm) {
    case menuSimmNO:
        status = pdset->read_bi(prec);
        break;

    case menuSimmYES:
    case menuSimmRAW: {
        recGblSetSevr(prec, SIMM_ALARM, prec->sims);
        if (prec->pact || (prec->sdly < 0.)) {
            status = dbGetLink(&prec->siol, DBR_ULONG, &prec->sval, 0, 0);
            if (status == 0) {
                if (prec->simm == menuSimmYES) {
                    prec->val = prec->sval;
                    status = 2;   /* Don't convert */
                } else {
                    prec->rval = (unsigned short) prec->sval;
                }
                prec->udf = FALSE;
            }
            prec->pact = FALSE;
        } else { /* !prec->pact && delay >= 0. */
            epicsCallback *pvt = prec->simpvt;
            if (!pvt) {
                pvt = calloc(1, sizeof(epicsCallback)); /* very lazy allocation of callback structure */
                prec->simpvt = pvt;
            }
            if (pvt) callbackRequestProcessCallbackDelayed(pvt, prec->prio, prec, prec->sdly);
            prec->pact = TRUE;
        }
        break;
    }

    default:
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        status = -1;
    }

    return status;
}
