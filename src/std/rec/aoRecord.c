/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* aoRecord.c - Record Support Routines for Analog Output records */
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
#include "epicsMath.h"
#include "alarm.h"
#include "cvtTable.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "special.h"
#include "recSup.h"
#include "recGbl.h"
#include "menuConvert.h"
#include "menuOmsl.h"
#include "menuYesNo.h"
#include "menuIvoa.h"

#define GEN_SIZE_OFFSET
#include "aoRecord.h"
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
static long get_units(DBADDR *, char *);
static long get_precision(const DBADDR *, long *);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *, struct dbr_grDouble *);
static long get_control_double(DBADDR *, struct dbr_ctrlDouble *);
static long get_alarm_double(DBADDR *, struct dbr_alDouble *);

rset aoRSET={
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

struct aodset { /* analog input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (0,2)=>(success,success no convert)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;/*(0)=>(success ) */
	DEVSUPFUN	special_linconv;
};
epicsExportAddress(rset,aoRSET);



static void checkAlarms(aoRecord *);
static long fetch_value(aoRecord *, double *);
static void convert(aoRecord *, double);
static void monitor(aoRecord *);
static long writeValue(aoRecord *);

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct aoRecord *prec = (struct aoRecord *)pcommon;
    struct aodset  *pdset;
    double 	eoff = prec->eoff, eslo = prec->eslo;
    double	value;

    if (pass==0) return(0);

    recGblInitConstantLink(&prec->siml,DBF_USHORT,&prec->simm);

    if(!(pdset = (struct aodset *)(prec->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)prec,"ao: init_record");
	return(S_dev_noDSET);
    }
    /* get the initial value if dol is a constant*/
    if (recGblInitConstantLink(&prec->dol,DBF_DOUBLE,&prec->val))
        prec->udf = isnan(prec->val);

    /* must have write_ao function defined */
    if ((pdset->number < 6) || (pdset->write_ao ==NULL)) {
	recGblRecordError(S_dev_missingSup,(void *)prec,"ao: init_record");
	return(S_dev_missingSup);
    }
    prec->init = TRUE;
    /*The following is for old device support that doesnt know about eoff*/
    if ((prec->eslo==1.0) && (prec->eoff==0.0)) {
	prec->eoff = prec->egul;
    }

    if (pdset->init_record) {
	long status=(*pdset->init_record)(prec);
	if (prec->linr == menuConvertSLOPE) {
	    prec->eoff = eoff;
	    prec->eslo = eslo;
	}
        switch(status){
        case(0): /* convert */
	    value = (double)prec->rval + (double)prec->roff;
	    if(prec->aslo!=0.0) value *= prec->aslo;
	    value += prec->aoff;
            if (prec->linr == menuConvertNO_CONVERSION){
		; /*do nothing*/
            } else if ((prec->linr == menuConvertLINEAR) ||
		      (prec->linr == menuConvertSLOPE)) {
                value = value*prec->eslo + prec->eoff;
            }else{
                if(cvtRawToEngBpt(&value,prec->linr,prec->init,
			(void *)&prec->pbrk,&prec->lbrk)!=0) break;
            }
	    prec->val = value;
	    prec->udf = isnan(value);
        break;
        case(2): /* no convert */
        break;
        default:
	     recGblRecordError(S_dev_badInitRet,(void *)prec,"ao: init_record");
	     return(S_dev_badInitRet);
        }
    }
    prec->oval = prec->pval = prec->val;
    prec->mlst = prec->val;
    prec->alst = prec->val;
    prec->lalm = prec->val;
    prec->oraw = prec->rval;
    prec->orbv = prec->rbv;
    return(0);
}

static long process(struct dbCommon *pcommon)
{
    struct aoRecord *prec = (struct aoRecord *)pcommon;
    struct aodset  *pdset = (struct aodset *)(prec->dset);
	long		 status=0;
	unsigned char    pact=prec->pact;
	double		value;

	if ((pdset==NULL) || (pdset->write_ao==NULL)) {
		prec->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)prec,"write_ao");
		return(S_dev_missingSup);
	}

	/* fetch value and convert*/
	if (prec->pact == FALSE) {
                if (!dbLinkIsConstant(&prec->dol) &&
                    prec->omsl == menuOmslclosed_loop) {
                    status = fetch_value(prec, &value);
                }
                else {
                    value = prec->val;
                }
		if(!status) convert(prec, value);
		prec->udf = isnan(prec->val);
	}

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
	                if(prec->pact == FALSE){
			 	prec->val=prec->ivov;
			 	value=prec->ivov;
				convert(prec,value);
                        }
			status=writeValue(prec); /* write the new value */
		        break;
		    default :
			status=-1;
		        recGblRecordError(S_db_badField,(void *)prec,
		                "ao:process Illegal IVOA field");
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

	prec->init=FALSE;
	prec->pact=FALSE;
	return(status);
}

static long special(DBADDR *paddr, int after)
{
    aoRecord     *prec = (aoRecord *)(paddr->precord);
    struct aodset       *pdset = (struct aodset *) (prec->dset);
    int                 special_type = paddr->special;

    switch(special_type) {
    case(SPC_LINCONV):
        if(pdset->number<6 ) {
            recGblDbaddrError(S_db_noMod,paddr,"ao: special");
            return(S_db_noMod);
        }
	prec->init=TRUE;
        if ((prec->linr == menuConvertLINEAR) && pdset->special_linconv) {
	    double eoff = prec->eoff;
	    double eslo = prec->eslo;
	    long status;
	    prec->eoff = prec->egul;
	    status = (*pdset->special_linconv)(prec,after);
	    if (eoff != prec->eoff)
		db_post_events(prec, &prec->eoff, DBE_VALUE|DBE_LOG);
            if (eslo != prec->eslo)
		db_post_events(prec, &prec->eslo, DBE_VALUE|DBE_LOG);
	    return (status);
	}
	return (0);
    default:
        recGblDbaddrError(S_db_badChoice,paddr,"ao: special");
        return(S_db_badChoice);
    }
}

#define indexof(field) aoRecord##field

static long get_units(DBADDR * paddr,char *units)
{
    aoRecord	*prec=(aoRecord *)paddr->precord;

    if(paddr->pfldDes->field_type == DBF_DOUBLE) {
        switch (dbGetFieldIndex(paddr)) {
            case indexof(ASLO):
            case indexof(AOFF):
                break;
            default:
                strncpy(units,prec->egu,DB_UNITS_SIZE);
        }
    }
    return(0);
}

static long get_precision(const DBADDR *paddr,long *precision)
{
    aoRecord	*prec=(aoRecord *)paddr->precord;

    *precision = prec->prec;
    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
        case indexof(OVAL):
        case indexof(PVAL):
            break;
        default:
            recGblGetPrec(paddr,precision);
    }
    return(0);
}

static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd)
{
    aoRecord	*prec=(aoRecord *)paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
        case indexof(OVAL):
        case indexof(PVAL):
        case indexof(HIHI):
        case indexof(HIGH):
        case indexof(LOW):
        case indexof(LOLO):
        case indexof(LALM):
        case indexof(ALST):
        case indexof(MLST):
        case indexof(IVOV):
            pgd->upper_disp_limit = prec->hopr;
            pgd->lower_disp_limit = prec->lopr;
            break;
        default:
            recGblGetGraphicDouble(paddr,pgd);
    }
    return(0);
}

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    aoRecord	*prec=(aoRecord *)paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
        case indexof(OVAL):
        case indexof(PVAL):
        case indexof(HIHI):
        case indexof(HIGH):
        case indexof(LOW):
        case indexof(LOLO):
        case indexof(LALM):
        case indexof(ALST):
        case indexof(MLST):
            pcd->upper_ctrl_limit = prec->drvh;
            pcd->lower_ctrl_limit = prec->drvl;
            break;
        default:
            recGblGetControlDouble(paddr,pcd);
    }
    return(0);
}
static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble *pad)
{
    aoRecord	*prec=(aoRecord *)paddr->precord;

    if(dbGetFieldIndex(paddr) == indexof(VAL)){
        pad->upper_alarm_limit = prec->hhsv ? prec->hihi : epicsNAN;
        pad->upper_warning_limit = prec->hsv ? prec->high : epicsNAN;
        pad->lower_warning_limit = prec->lsv ? prec->low : epicsNAN;
        pad->lower_alarm_limit = prec->llsv ? prec->lolo : epicsNAN;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(aoRecord *prec)
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

static long fetch_value(aoRecord *prec,double *pvalue)
{
	short		save_pact;
	long		status;

	save_pact = prec->pact;
	prec->pact = TRUE;

	/* don't allow dbputs to val field */
	prec->val=prec->pval;

        status = dbGetLink(&prec->dol,DBR_DOUBLE,pvalue,0,0);
        prec->pact = save_pact;

	if (status) {
           recGblSetSevr(prec,LINK_ALARM,INVALID_ALARM);
           return(status);
	}

        if (prec->oif == aoOIF_Incremental)
           *pvalue += prec->val;

	return(0);
}

static void convert(aoRecord *prec, double value)
{
    /* check drive limits */
    if (prec->drvh > prec->drvl) {
        if (value > prec->drvh)
            value = prec->drvh;
        else if (value < prec->drvl)
            value = prec->drvl;
    }
    prec->val = value;
    prec->pval = value;

    /* now set value equal to desired output value */
    /* apply the output rate of change */
    if (prec->oroc != 0){/*must be defined and >0*/
        double diff;

        diff = value - prec->oval;
        if (diff < 0) {
            if (prec->oroc < -diff)
                value = prec->oval - prec->oroc;
        } else if (prec->oroc < diff)
            value = prec->oval + prec->oroc;
    }
    prec->omod = (prec->oval!=value);
    prec->oval = value;

    /* convert */
    switch (prec->linr) {
    case menuConvertNO_CONVERSION:
        break; /* do nothing*/
    case menuConvertLINEAR:
    case menuConvertSLOPE:
        if (prec->eslo == 0.0) value = 0;
        else value = (value - prec->eoff) / prec->eslo;
        break;
    default:
        if (cvtEngToRawBpt(&value, prec->linr, prec->init,
            (void *)&prec->pbrk, &prec->lbrk) != 0) {
            recGblSetSevr(prec, SOFT_ALARM, MAJOR_ALARM);
            return;
        }
    }
    value -= prec->aoff;
    if (prec->aslo != 0) value /= prec->aslo;

    /* Apply raw offset and limits, round to 32-bit integer */
    value -= prec->roff;
    if (value >= 0.0) {
        if (value >= (0x7fffffff - 0.5))
            prec->rval = 0x7fffffff;
        else
            prec->rval = (epicsInt32)(value + 0.5);
    } else {
        if (value > (0.5 - 0x80000000))
            prec->rval = (epicsInt32)(value - 0.5);
        else
            prec->rval = 0x80000000;
    }
}


static void monitor(aoRecord *prec)
{
    unsigned monitor_mask = recGblResetAlarms(prec);

    /* check for value change */
    recGblCheckDeadband(&prec->mlst, prec->val, prec->mdel, &monitor_mask, DBE_VALUE);

    /* check for archive change */
    recGblCheckDeadband(&prec->alst, prec->val, prec->adel, &monitor_mask, DBE_ARCHIVE);

    /* send out monitors connected to the value field */
    if (monitor_mask){
        db_post_events(prec,&prec->val,monitor_mask);
    }

	if(prec->omod) monitor_mask |= (DBE_VALUE|DBE_LOG);
	if(monitor_mask) {
		prec->omod = FALSE;
		db_post_events(prec,&prec->oval,monitor_mask);
		if(prec->oraw != prec->rval) {
                	db_post_events(prec,&prec->rval,
			    monitor_mask|DBE_VALUE|DBE_LOG);
			prec->oraw = prec->rval;
		}
		if(prec->orbv != prec->rbv) {
                	db_post_events(prec,&prec->rbv,
			    monitor_mask|DBE_VALUE|DBE_LOG);
			prec->orbv = prec->rbv;
		}
	}
	return;
}

static long writeValue(aoRecord *prec)
{
	long		status;
        struct aodset 	*pdset = (struct aodset *) (prec->dset);

	if (prec->pact == TRUE){
		status=(*pdset->write_ao)(prec);
		return(status);
	}

        status = dbGetLink(&prec->siml,DBR_USHORT,&(prec->simm),0,0);
	if (status)
		return(status);

	if (prec->simm == menuYesNoNO){
		status=(*pdset->write_ao)(prec);
		return(status);
	}
	if (prec->simm == menuYesNoYES){
		status = dbPutLink(&(prec->siol),DBR_DOUBLE,&(prec->oval),1);
	} else {
		status=-1;
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(prec,SIMM_ALARM,prec->sims);

	return(status);
}
