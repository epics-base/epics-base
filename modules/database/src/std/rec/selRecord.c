/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* selRecord.c - Record Support Routines for Select records */
/*
 *      Original Author: Bob Dalesio
 *      Date:            6-2-89
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "epicsMath.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"

#define GEN_SIZE_OFFSET
#include "selRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units(DBADDR *, char *);
static long get_precision(const DBADDR *, long *);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *, struct dbr_grDouble *);
static long get_control_double(DBADDR *, struct dbr_ctrlDouble *);
static long get_alarm_double(DBADDR *, struct dbr_alDouble *);

rset selRSET={
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
epicsExportAddress(rset,selRSET);

#define	SEL_MAX	12

static void checkAlarms(selRecord *);
static void do_sel(selRecord *);
static int fetch_values(selRecord *);
static void monitor(selRecord *);


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct selRecord *prec = (struct selRecord *)pcommon;
    struct link *plink;
    int i;
    double *pvalue;

    if (pass==0)
        return 0;

    /* get seln initial value if nvl is a constant*/
    recGblInitConstantLink(&prec->nvl, DBF_USHORT, &prec->seln);

    plink = &prec->inpa;
    pvalue = &prec->a;
    for (i=0; i<SEL_MAX; i++, plink++, pvalue++) {
        *pvalue = epicsNAN;
        recGblInitConstantLink(plink, DBF_DOUBLE, pvalue);
    }
    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct selRecord *prec = (struct selRecord *)pcommon;
    prec->pact = TRUE;
    if ( RTN_SUCCESS(fetch_values(prec)) ) {
	do_sel(prec);
    }

    recGblGetTimeStamp(prec);
    /* check for alarms */
    checkAlarms(prec);


    /* check event list */
    monitor(prec);

    /* process the forward scan link record */
    recGblFwdLink(prec);

    prec->pact=FALSE;
    return(0);
}


#define indexof(field) selRecord##field

static long get_units(DBADDR *paddr, char *units)
{
    selRecord	*prec=(selRecord *)paddr->precord;

    if(paddr->pfldDes->field_type == DBF_DOUBLE) {
        strncpy(units,prec->egu,DB_UNITS_SIZE);
    }
    return(0);
}

static long get_precision(const DBADDR *paddr, long *precision)
{
    selRecord	*prec=(selRecord *)paddr->precord;
    double *pvalue,*plvalue;
    int i;

    *precision = prec->prec;
    if(paddr->pfield==(void *)&prec->val){
        return(0);
    }
    pvalue = &prec->a;
    plvalue = &prec->la;
    for(i=0; i<SEL_MAX; i++, pvalue++, plvalue++) {
	if(paddr->pfield==(void *)&pvalue
	|| paddr->pfield==(void *)&plvalue){
	    return(0);
	}
    }
    recGblGetPrec(paddr,precision);
    return(0);
}


static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    selRecord	*prec=(selRecord *)paddr->precord;
    int index = dbGetFieldIndex(paddr);
    
    switch (index) {
        case indexof(VAL):
        case indexof(HIHI):
        case indexof(HIGH):
        case indexof(LOW):
        case indexof(LOLO):
        case indexof(LALM):
        case indexof(ALST):
        case indexof(MLST):
#ifdef __GNUC__
        case indexof(A) ... indexof(L):
        case indexof(LA) ... indexof(LL):
            break;
        default:
#else
            break;
        default:
            if((index >= indexof(A) && index <= indexof(L))
            || (index >= indexof(LA) && index <= indexof(LL)))
                break;
#endif
            recGblGetGraphicDouble(paddr,pgd);
            return(0);
    }
    pgd->upper_disp_limit = prec->hopr;
    pgd->lower_disp_limit = prec->lopr;
    return(0);
}

static long get_control_double(struct dbAddr *paddr, struct dbr_ctrlDouble *pcd)
{
    selRecord	*prec=(selRecord *)paddr->precord;
    int index = dbGetFieldIndex(paddr);
    
    switch (index) {
        case indexof(VAL):
        case indexof(HIHI):
        case indexof(HIGH):
        case indexof(LOW):
        case indexof(LOLO):
        case indexof(LALM):
        case indexof(ALST):
        case indexof(MLST):
#ifdef __GNUC__
        case indexof(A) ... indexof(L):
        case indexof(LA) ... indexof(LL):
            break;
        default:
#else
            break;
        default:
            if((index >= indexof(A) && index <= indexof(L))
            || (index >= indexof(LA) && index <= indexof(LL)))
                break;
#endif
            recGblGetControlDouble(paddr,pcd);
            return(0);
    }
    pcd->upper_ctrl_limit = prec->hopr;
    pcd->lower_ctrl_limit = prec->lopr;
    return(0);
}

static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble *pad)
{
    selRecord	*prec=(selRecord *)paddr->precord;

    if(dbGetFieldIndex(paddr) == indexof(VAL)) {
        pad->upper_alarm_limit = prec->hhsv ? prec->hihi : epicsNAN;
        pad->upper_warning_limit = prec->hsv ? prec->high : epicsNAN;
        pad->lower_warning_limit = prec->lsv ? prec->low : epicsNAN;
        pad->lower_alarm_limit = prec->llsv ? prec->lolo : epicsNAN;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(selRecord *prec)
{
    double val, hyst, lalm;
    double alev;
    epicsEnum16 asev;

    if (prec->udf) {
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);
        return;
    }

    val = prec->val;
    hyst = prec->hyst;
    lalm = prec->lalm;

    /* alarm condition hihi */
    asev = prec->hhsv;
    alev = prec->hihi;
    if (asev && (val >= alev || ((lalm == alev) && (val >= alev - hyst)))) {
        if (recGblSetSevr(prec, HIHI_ALARM, asev))
            prec->lalm = alev;
        return;
    }

    /* alarm condition lolo */
    asev = prec->llsv;
    alev = prec->lolo;
    if (asev && (val <= alev || ((lalm == alev) && (val <= alev + hyst)))) {
        if (recGblSetSevr(prec, LOLO_ALARM, asev))
            prec->lalm = alev;
        return;
    }

    /* alarm condition high */
    asev = prec->hsv;
    alev = prec->high;
    if (asev && (val >= alev || ((lalm == alev) && (val >= alev - hyst)))) {
        if (recGblSetSevr(prec, HIGH_ALARM, asev))
            prec->lalm = alev;
        return;
    }

    /* alarm condition low */
    asev = prec->lsv;
    alev = prec->low;
    if (asev && (val <= alev || ((lalm == alev) && (val <= alev + hyst)))) {
        if (recGblSetSevr(prec, LOW_ALARM, asev))
            prec->lalm = alev;
        return;
    }

    /* we get here only if val is out of alarm by at least hyst */
    prec->lalm = val;
    return;
}

static void monitor(selRecord *prec)
{
    unsigned    monitor_mask;
    double		*pnew;
    double		*pprev;
    int			i;

    monitor_mask = recGblResetAlarms(prec);

    /* check for value change */
    recGblCheckDeadband(&prec->mlst, prec->val, prec->mdel, &monitor_mask, DBE_VALUE);

    /* check for archive change */
    recGblCheckDeadband(&prec->alst, prec->val, prec->adel, &monitor_mask, DBE_ARCHIVE);

    /* send out monitors connected to the value field */
    if (monitor_mask)
        db_post_events(prec, &prec->val, monitor_mask);

    monitor_mask |= DBE_VALUE|DBE_LOG;

    /* trigger monitors of the SELN field */
    if (prec->nlst != prec->seln) {
	prec->nlst = prec->seln;
	db_post_events(prec, &prec->seln, monitor_mask);
    }

    /* check all input fields for changes, even if VAL hasn't changed */
    for(i=0, pnew=&prec->a, pprev=&prec->la; i<SEL_MAX; i++, pnew++, pprev++) {
	if(*pnew != *pprev) {
	    db_post_events(prec, pnew, monitor_mask);
	    *pprev = *pnew;
	}
    }
    return;
}

static void do_sel(selRecord *prec)
{
    double		*pvalue;
    double		order[SEL_MAX];
    unsigned short	count;
    unsigned short	i,j;
    double		val;

    /* selection mechanism */
    pvalue = &prec->a;
    switch (prec->selm){
    case (selSELM_Specified):
        if (prec->seln >= SEL_MAX) {
            recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
            return;
        }
        val = *(pvalue+prec->seln);
        break;
    case (selSELM_High_Signal):
        val = -epicsINF;
        for (i = 0; i < SEL_MAX; i++,pvalue++){
            if (!isnan(*pvalue) && val < *pvalue) {
                val = *pvalue;
                prec->seln = i;
            }
        }
        break;
    case (selSELM_Low_Signal):
        val = epicsINF;
        for (i = 0; i < SEL_MAX; i++,pvalue++){
            if (!isnan(*pvalue) && val > *pvalue) {
                val = *pvalue;
                prec->seln = i;
            }
        }
        break;
    case (selSELM_Median_Signal):
        count = 0;
        order[0] = epicsNAN;
        for (i = 0; i < SEL_MAX; i++,pvalue++){
            if (!isnan(*pvalue)){
                /* Insertion sort */
                j = count;
                while ((j > 0) && (order[j-1] > *pvalue)){
                    order[j] = order[j-1];
                    j--;
                }
                order[j] = *pvalue;
                count++;
            }
        }
        prec->seln = count;
        val = order[count / 2];
        break;
    default:
        recGblSetSevr(prec,CALC_ALARM,INVALID_ALARM);
        return;
    }
    prec->val = val;
    prec->udf = isnan(prec->val);
    return;
}

/*
 * FETCH_VALUES
 *
 * fetch the values for the variables from which to select
 */
static int fetch_values(selRecord *prec)
{
    struct link	*plink;
    double	*pvalue;
    int		i;
    long	status;

    plink = &prec->inpa;
    pvalue = &prec->a;
    /* If mechanism is selSELM_Specified, only get the selected input*/
    if(prec->selm == selSELM_Specified) {
	/* fetch the select index */
	status=dbGetLink(&(prec->nvl),DBR_USHORT,&(prec->seln),0,0);
	if (!RTN_SUCCESS(status) || (prec->seln >= SEL_MAX))
	    return(status);

	plink += prec->seln;
	pvalue += prec->seln;

	status=dbGetLink(plink,DBR_DOUBLE, pvalue,0,0);
	return(status);
    }
    /* fetch all inputs*/
    for(i=0; i<SEL_MAX; i++, plink++, pvalue++) {
	status=dbGetLink(plink,DBR_DOUBLE, pvalue,0,0);
    }
    return(status);
}
