/* recDfanout.c */
/* share/src/rec @(#)recDfanout.c	1.16     6/4/93 */

/* recDfanout.c - Record Support Routines for Dfanout records */
/*
 * Author: 	Matt Bickley
 * Date:	Sometime in 1994
 *
 * Modification Log:
 * -----------------
 * .01  1994        mhb     Started with longout record to make the data fanout
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>

#include        <alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include	<dfanoutRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
static long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units();
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();

struct rset dfanoutRSET={
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


static void alarm();
static void monitor();
static long push_values();

#define OUT_ARG_MAX 8

static char Ofldnames[OUT_ARG_MAX][FLDNAME_SZ] =
   {"OUTA", "OUTB", "OUTC", "OUTD", "OUTE", "OUTF", "OUTG", "OUTH"};


static long init_record(pdfanout,pass)
    struct dfanoutRecord	*pdfanout;
    int pass;
{
    struct link *plink;
    long status=0;
    int i;

    if (pass==0) return(0);


    plink = &pdfanout->outa;
    for (i=0; i<OUT_ARG_MAX; i++, plink++) {
        if (plink->type == PV_LINK) {
            status = dbCaAddOutlink(plink, (void *) pdfanout, Ofldnames[i]);
            if (status) return(status);
        }
    }

    /* get the initial value dol is a constant*/
    if (pdfanout->dol.type == CONSTANT){
        pdfanout->val = pdfanout->dol.value.value;
	pdfanout->udf=FALSE;
    }
    if (pdfanout->dol.type == PV_LINK)
    {
        status = dbCaAddInlink(&(pdfanout->dol), (void *) pdfanout, "VAL");
        if(status) return(status);
    } /* endif */

    return(0);
}

static long process(pdfanout)
        struct dfanoutRecord     *pdfanout;
{
	long		 status=0;
	unsigned char    pact=pdfanout->pact;

        if (!pdfanout->pact && pdfanout->omsl == CLOSED_LOOP){
		long options=0;
		long nRequest=1;

		status = recGblGetLinkValue(&(pdfanout->dol),(void *)pdfanout,
			DBR_LONG,&(pdfanout->val),&options,&nRequest);
		if(RTN_SUCCESS(status)) pdfanout->udf=FALSE;
	}

	pdfanout->pact = TRUE;

	recGblGetTimeStamp(pdfanout);

    /* Push out the data to all the forward links */
   status = push_values(pdfanout);
	/* check for alarms */
	alarm(pdfanout);
	/* check event list */
	monitor(pdfanout);

	/* process the forward scan link record */
	recGblFwdLink(pdfanout);


	pdfanout->pact=FALSE;
	return(status);
}

static long get_value(pdfanout,pvdes)
    struct dfanoutRecord             *pdfanout;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_LONG;
    pvdes->no_elements=1;
    (long *)(pvdes->pvalue) = &pdfanout->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct dfanoutRecord	*pdfanout=(struct dfanoutRecord *)paddr->precord;

    strncpy(units,pdfanout->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct dfanoutRecord	*pdfanout=(struct dfanoutRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pdfanout->val
    || paddr->pfield==(void *)&pdfanout->hihi
    || paddr->pfield==(void *)&pdfanout->high
    || paddr->pfield==(void *)&pdfanout->low
    || paddr->pfield==(void *)&pdfanout->lolo){
        pgd->upper_disp_limit = pdfanout->hopr;
        pgd->lower_disp_limit = pdfanout->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct dfanoutRecord	*pdfanout=(struct dfanoutRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pdfanout->val
    || paddr->pfield==(void *)&pdfanout->hihi
    || paddr->pfield==(void *)&pdfanout->high
    || paddr->pfield==(void *)&pdfanout->low
    || paddr->pfield==(void *)&pdfanout->lolo){
        pcd->upper_ctrl_limit = pdfanout->hopr;
        pcd->lower_ctrl_limit = pdfanout->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}
static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct dfanoutRecord	*pdfanout=(struct dfanoutRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pdfanout->val){
         pad->upper_alarm_limit = pdfanout->hihi;
         pad->upper_warning_limit = pdfanout->high;
         pad->lower_warning_limit = pdfanout->low;
         pad->lower_alarm_limit = pdfanout->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void alarm(pdfanout)
    struct dfanoutRecord	*pdfanout;
{
	double		val;
	float		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if(pdfanout->udf == TRUE ){
 		recGblSetSevr(pdfanout,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = pdfanout->hihi; lolo = pdfanout->lolo; high = pdfanout->high; low = pdfanout->low;
	hhsv = pdfanout->hhsv; llsv = pdfanout->llsv; hsv = pdfanout->hsv; lsv = pdfanout->lsv;
	val = pdfanout->val; hyst = pdfanout->hyst; lalm = pdfanout->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(pdfanout,HIHI_ALARM,pdfanout->hhsv)) pdfanout->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(pdfanout,LOLO_ALARM,pdfanout->llsv)) pdfanout->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(pdfanout,HIGH_ALARM,pdfanout->hsv)) pdfanout->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(pdfanout,LOW_ALARM,pdfanout->lsv)) pdfanout->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	pdfanout->lalm = val;
	return;
}

static void monitor(pdfanout)
    struct dfanoutRecord	*pdfanout;
{
	unsigned short	monitor_mask;

	long		delta;

        monitor_mask = recGblResetAlarms(pdfanout);
        monitor_mask |= (DBE_LOG|DBE_VALUE);
        if(monitor_mask)
           db_post_events(pdfanout,&pdfanout->val,monitor_mask);

        /* check for value change */
        delta = pdfanout->mlst - pdfanout->val;
        if(delta<0) delta = -delta;
        if (delta > pdfanout->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                pdfanout->mlst = pdfanout->val;
        }
        /* check for archive change */
        delta = pdfanout->alst - pdfanout->val;
        if(delta<0) delta = -delta;
        if (delta > pdfanout->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                pdfanout->alst = pdfanout->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pdfanout,&pdfanout->val,monitor_mask);
	}
	return;
}

static long push_values(pdfanout)
struct dfanoutRecord *pdfanout;
{
        struct link     *plink; /* structure of the link field  */
        long            nRequest=1;
        int             i;
        long            status;

        for(i=0, plink=&(pdfanout->outa); i<OUT_ARG_MAX; i++, plink++) {
                nRequest=1;
                status=recGblPutLinkValue(plink,(void *)pdfanout,DBR_LONG,
                        &(pdfanout->val),&nRequest);
                if (!RTN_SUCCESS(status)) return(-1);
        }
        return(0);
}
