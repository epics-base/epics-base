/* xxxRecord.c */
/* Example record support module */
  
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "dbEvent.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "special.h"
#define GEN_SIZE_OFFSET
#include "xxxRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table */
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();
 
rset xxxRSET={
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
epicsExportAddress(rset,xxxRSET);

typedef struct xxxset { /* xxx input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_xxx;
}xxxdset;

static void checkAlarms(xxxRecord *pxxx);
static void monitor(xxxRecord *pxxx);

static long init_record(void *precord,int pass)
{
    xxxRecord	*pxxx = (xxxRecord *)precord;
    xxxdset	*pdset;
    long	status;

    if (pass==0) return(0);

    if(!(pdset = (xxxdset *)(pxxx->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pxxx,"xxx: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_xxx function defined */
    if( (pdset->number < 5) || (pdset->read_xxx == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pxxx,"xxx: init_record");
	return(S_dev_missingSup);
    }

    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pxxx))) return(status);
    }
    return(0);
}

static long process(void *precord)
{
	xxxRecord	*pxxx = (xxxRecord *)precord;
	xxxdset		*pdset = (xxxdset *)(pxxx->dset);
	long		 status;
	unsigned char    pact=pxxx->pact;

	if( (pdset==NULL) || (pdset->read_xxx==NULL) ) {
		pxxx->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pxxx,"read_xxx");
		return(S_dev_missingSup);
	}

	/* pact must not be set until after calling device support */
	status=(*pdset->read_xxx)(pxxx);
	/* check if device support set pact */
	if ( !pact && pxxx->pact ) return(0);
	pxxx->pact = TRUE;

	recGblGetTimeStamp(pxxx);
	/* check for alarms */
	checkAlarms(pxxx);
	/* check event list */
	monitor(pxxx);
	/* process the forward scan link record */
        recGblFwdLink(pxxx);

	pxxx->pact=FALSE;
	return(status);
}

static long get_units(DBADDR *paddr, char *units)
{
    xxxRecord	*pxxx=(xxxRecord *)paddr->precord;

    strncpy(units,pxxx->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(DBADDR *paddr, long *precision)
{
    xxxRecord	*pxxx=(xxxRecord *)paddr->precord;

    *precision = pxxx->prec;
    if(paddr->pfield == (void *)&pxxx->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd)
{
    xxxRecord	*pxxx=(xxxRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == xxxRecordVAL
    || fieldIndex == xxxRecordHIHI
    || fieldIndex == xxxRecordHIGH
    || fieldIndex == xxxRecordLOW
    || fieldIndex == xxxRecordLOLO
    || fieldIndex == xxxRecordHOPR
    || fieldIndex == xxxRecordLOPR) {
        pgd->upper_disp_limit = pxxx->hopr;
        pgd->lower_disp_limit = pxxx->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(DBADDR *paddr,struct dbr_ctrlDouble *pcd)
{
    xxxRecord	*pxxx=(xxxRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == xxxRecordVAL
    || fieldIndex == xxxRecordHIHI
    || fieldIndex == xxxRecordHIGH
    || fieldIndex == xxxRecordLOW
    || fieldIndex == xxxRecordLOLO) {
	pcd->upper_ctrl_limit = pxxx->hopr;
	pcd->lower_ctrl_limit = pxxx->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static long get_alarm_double(DBADDR *paddr,struct dbr_alDouble *pad)
{
    xxxRecord	*pxxx=(xxxRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == xxxRecordVAL) {
	pad->upper_alarm_limit = pxxx->hihi;
	pad->upper_warning_limit = pxxx->high;
	pad->lower_warning_limit = pxxx->low;
	pad->lower_alarm_limit = pxxx->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(xxxRecord *pxxx)
{
	double		val;
	float		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if(pxxx->udf == TRUE ){
		recGblSetSevr(pxxx,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = pxxx->hihi; lolo = pxxx->lolo; high = pxxx->high; low = pxxx->low;
	hhsv = pxxx->hhsv; llsv = pxxx->llsv; hsv = pxxx->hsv; lsv = pxxx->lsv;
	val = pxxx->val; hyst = pxxx->hyst; lalm = pxxx->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(pxxx,HIHI_ALARM,pxxx->hhsv)) pxxx->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(pxxx,LOLO_ALARM,pxxx->llsv)) pxxx->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(pxxx,HIGH_ALARM,pxxx->hsv)) pxxx->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(pxxx,LOW_ALARM,pxxx->lsv)) pxxx->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	pxxx->lalm = val;
	return;
}

static void monitor(xxxRecord *pxxx)
{
	unsigned short	monitor_mask;
	double		delta;

        monitor_mask = recGblResetAlarms(pxxx);
	/* check for value change */
	delta = pxxx->mlst - pxxx->val;
	if(delta<0.0) delta = -delta;
	if (delta > pxxx->mdel) {
		/* post events for value change */
		monitor_mask |= DBE_VALUE;
		/* update last value monitored */
		pxxx->mlst = pxxx->val;
	}

	/* check for archive change */
	delta = pxxx->alst - pxxx->val;
	if(delta<0.0) delta = -delta;
	if (delta > pxxx->adel) {
		/* post events on value field for archive change */
		monitor_mask |= DBE_LOG;
		/* update last archive value monitored */
		pxxx->alst = pxxx->val;
	}

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(pxxx,&pxxx->val,monitor_mask);
	}
	return;
}
