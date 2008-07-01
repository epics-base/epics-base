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
 * Author: 	Janet Anderson
 * Date:	9/23/91
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
#define GEN_SIZE_OFFSET
#include "longoutRecord.h"
#undef  GEN_SIZE_OFFSET
#include "menuIvoa.h"
#include "menuOmsl.h"
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(longoutRecord *, int);
static long process(longoutRecord *);
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units(DBADDR *, char *);
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *, struct dbr_grDouble *);
static long get_control_double(DBADDR *, struct dbr_ctrlDouble *);
static long get_alarm_double(DBADDR *, struct dbr_alDouble *);

rset longoutRSET={
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
epicsExportAddress(rset,longoutRSET);


struct longoutdset { /* longout input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_longout;/*(-1,0)=>(failure,success*/
};
static void checkAlarms(longoutRecord *plongout);
static void monitor(longoutRecord *plongout);
static long writeValue(longoutRecord *plongout);
static void convert(longoutRecord *plongout, epicsInt32 value);


static long init_record(longoutRecord *plongout, int pass)
{
    struct longoutdset *pdset;
    long status=0;

    if (pass==0) return(0);
    if (plongout->siml.type == CONSTANT) {
	recGblInitConstantLink(&plongout->siml,DBF_USHORT,&plongout->simm);
    }
    if(!(pdset = (struct longoutdset *)(plongout->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)plongout,"longout: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_longout functions defined */
    if( (pdset->number < 5) || (pdset->write_longout == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)plongout,"longout: init_record");
	return(S_dev_missingSup);
    }
    if (plongout->dol.type == CONSTANT) {
	if(recGblInitConstantLink(&plongout->dol,DBF_LONG,&plongout->val))
	    plongout->udf=FALSE;
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(plongout))) return(status);
    }
    return(0);
}

static long process(longoutRecord *plongout)
{
	struct longoutdset	*pdset = (struct longoutdset *)(plongout->dset);
	long		 status=0;
	epicsInt32	 value;
	unsigned char    pact=plongout->pact;

	if( (pdset==NULL) || (pdset->write_longout==NULL) ) {
		plongout->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)plongout,"write_longout");
		return(S_dev_missingSup);
	}
	if (!plongout->pact) {
		if((plongout->dol.type != CONSTANT)
                && (plongout->omsl == menuOmslclosed_loop)) {
			status = dbGetLink(&(plongout->dol),DBR_LONG,
				&value,0,0);
			if (plongout->dol.type!=CONSTANT && RTN_SUCCESS(status))
				plongout->udf=FALSE;
		}
		else {
			value = plongout->val;
		}
		if (!status) convert(plongout,value);
	}

	/* check for alarms */
	checkAlarms(plongout);

        if (plongout->nsev < INVALID_ALARM )
                status=writeValue(plongout); /* write the new value */
        else {
                switch (plongout->ivoa) {
                    case (menuIvoaContinue_normally) :
                        status=writeValue(plongout); /* write the new value */
                        break;
                    case (menuIvoaDon_t_drive_outputs) :
                        break;
                    case (menuIvoaSet_output_to_IVOV) :
                        if(plongout->pact == FALSE){
                                plongout->val=plongout->ivov;
                        }
                        status=writeValue(plongout); /* write the new value */
                        break;
                    default :
                        status=-1;
                        recGblRecordError(S_db_badField,(void *)plongout,
                                "longout:process Illegal IVOA field");
                }
        }

	/* check if device support set pact */
	if ( !pact && plongout->pact ) return(0);
	plongout->pact = TRUE;

	recGblGetTimeStamp(plongout);

	/* check event list */
	monitor(plongout);

	/* process the forward scan link record */
	recGblFwdLink(plongout);

	plongout->pact=FALSE;
	return(status);
}

static long get_units(DBADDR *paddr,char *units)
{
    longoutRecord *plongout=(longoutRecord *)paddr->precord;

    strncpy(units,plongout->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd)
{
    longoutRecord *plongout=(longoutRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == longoutRecordVAL
    || fieldIndex == longoutRecordHIHI
    || fieldIndex == longoutRecordHIGH
    || fieldIndex == longoutRecordLOW
    || fieldIndex == longoutRecordLOLO) {
        pgd->upper_disp_limit = plongout->hopr;
        pgd->lower_disp_limit = plongout->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}


static long get_control_double(DBADDR *paddr,struct dbr_ctrlDouble *pcd)
{
    longoutRecord *plongout=(longoutRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == longoutRecordVAL
    || fieldIndex == longoutRecordHIHI
    || fieldIndex == longoutRecordHIGH
    || fieldIndex == longoutRecordLOW
    || fieldIndex == longoutRecordLOLO) {
        /* do not change pre drvh/drvl behavior */
        if(plongout->drvh > plongout->drvl) {
            pcd->upper_ctrl_limit = plongout->drvh;
            pcd->lower_ctrl_limit = plongout->drvl;
        } else {
            pcd->upper_ctrl_limit = plongout->hopr;
            pcd->lower_ctrl_limit = plongout->lopr;
        }
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}
static long get_alarm_double(DBADDR *paddr,struct dbr_alDouble *pad)
{
    longoutRecord    *plongout=(longoutRecord *)paddr->precord;
    int     fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == longoutRecordVAL) {
         pad->upper_alarm_limit = plongout->hihi;
         pad->upper_warning_limit = plongout->high;
         pad->lower_warning_limit = plongout->low;
         pad->lower_alarm_limit = plongout->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(longoutRecord *prec)
{
    epicsInt32 val, hyst, lalm;
    epicsInt32 alev;
    epicsEnum16 asev;

    if (prec->udf) {
        recGblSetSevr(prec, UDF_ALARM, INVALID_ALARM);
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

static void monitor(longoutRecord *plongout)
{
	unsigned short	monitor_mask;
	epicsInt32	delta;

        monitor_mask = recGblResetAlarms(plongout);
        /* check for value change */
        delta = plongout->mlst - plongout->val;
        if(delta<0) delta = -delta;
        if (delta > plongout->mdel || delta==0x80000000) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                plongout->mlst = plongout->val;
        }
        /* check for archive change */
        delta = plongout->alst - plongout->val;
        if(delta<0) delta = -delta;
        if (delta > plongout->adel || delta==0x80000000) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                plongout->alst = plongout->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(plongout,&plongout->val,monitor_mask);
	}
	return;
}

static long writeValue(longoutRecord *plongout)
{
	long status;
        struct longoutdset *pdset = (struct longoutdset *) (plongout->dset);

	if (plongout->pact == TRUE){
		status=(*pdset->write_longout)(plongout);
		return(status);
	}

	status=dbGetLink(&(plongout->siml),DBR_USHORT,&(plongout->simm),0,0);
	if (!RTN_SUCCESS(status))
		return(status);

	if (plongout->simm == NO){
		status=(*pdset->write_longout)(plongout);
		return(status);
	}
	if (plongout->simm == YES){
		status=dbPutLink(&plongout->siol,DBR_LONG,&plongout->val,1);
	} else {
		status=-1;
		recGblSetSevr(plongout,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(plongout,SIMM_ALARM,plongout->sims);

	return(status);
}

static void convert(longoutRecord *plongout, epicsInt32 value)
{
        /* check drive limits */
	if(plongout->drvh > plongout->drvl) {
        	if (value > plongout->drvh) value = plongout->drvh;
        	else if (value < plongout->drvl) value = plongout->drvl;
	}
	plongout->val = value;
}
