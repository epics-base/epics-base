/* recCalc.c */
/* share/src/rec $Id$ */

/* recCalc.c - Record Support Routines for Calculation records */
/*
 *      Original Author: Julie Sander and Bob Dalesio
 *      Current  Author: Marty Kraimer
 *      Date:            7-27-87
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  5-18-88         lrd     modified modulo and power to avoid math library
 * .02  5-19-88         lrd     modified absolute value to avoid math library
 *                              defined unary math lib routines as doubles
 *                              removed include math.h
 *                              stopped loading dinglers math routines (ml)
 *                              wrote a random number generator to return a
 *                                      double between 0 and 1
 * .03  12-09-88        lrd     fixed modulo not to perform zero division
 * .04  12-12-88        lrd     lock the record while processing
 * .05  12-13-88        lrd     made an alarm for math error
 * .06  12-15-88        lrd     Process the forward scan link
 * .07  12-23-88        lrd     Alarm on locked MAX_LOCKED times
 * .08  01-11-89        lrd     Add Right and Left Shift
 * .09  02-01-89        lrd     Add Trig functions
 * .10  03-14-89        lrd     fix true on C question mark operator
 * .11  03-29-89        lrd     make hardware errors MAJOR
 *                              remove hw severity spec from database
 * .12  04-06-89        lrd     add monitor detection
 * .13  05-03-89        lrd     removed process mask from arg list
 * .14  06-05-89        lrd     check for negative square root
 * .15  08-01-89        lrd     full range of exponentiation using pow(x,y)
 * .16  04-04-90        lrd     fix post events for read and calc alarms
 *                              fix neg base raised to integer exponent
 * .17  04-06-90        lrd     change conditional to check for 0 and non-zero
 *                              instead of 0 and 1 (more 'C' like)
 * .18  10-10-90	mrk	Made changes for new record support
 *				Note that all calc specific details moved
 *				to libCalc
 * .19  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .20  02-05-92	jba	Changed function arguments from paddr to precord 
 * .21  02-28-92        jba     Changed get_precision,get_graphic_double,get_control_double
 * .22  02-28-92	jba	ANSI C changes
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include	<calcRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
static long get_value();
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

struct rset calcRSET={
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

void alarm();
void monitor();
long calcPerform();
long postfix();
int fetch_values();
#define ARG_MAX 12
/* Database Channel Access Link Functions */
void dbCaAddInlink();
long dbCaGetLink();

 /* Fldnames should have as many as ARG_MAX */
 static char Fldnames[ARG_MAX][FLDNAME_SZ] =
     {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L"};


static long init_record(pcalc)
    struct calcRecord	*pcalc;
{
    long status;
    struct link *plink;
    int i;
    double *pvalue;
    short error_number;
    char rpbuf[80];

char dest_pvarname[((PVNAME_SZ)+(FLDNAME_SZ)+2)];
struct dbAddr dest_dbaddr;

    plink = &pcalc->inpa;
    pvalue = &pcalc->a;
    for(i=0; i<ARG_MAX; i++, plink++, pvalue++) {
        if(plink->type==CONSTANT) *pvalue = plink->value.value;
        if (plink->type == PV_LINK)
        {
            sprintf(dest_pvarname, "%s.%s", pcalc->name, Fldnames[i]);

            if (dbNameToAddr(dest_pvarname, &dest_dbaddr))
        printf("ERROR: recCalc.c init_record() problem in dbNameToAddr()\n");
            else
            dbCaAddInlink(plink, (void *) pcalc, Fldnames[i]);
        } /* endif */
    }
    status=postfix(pcalc->calc,rpbuf,&error_number);
    if(status) return(status);
    memcpy(pcalc->rpcl,rpbuf,sizeof(pcalc->rpcl));
    return(0);
}

static long process(pcalc)
	struct calcRecord     *pcalc;
{

	pcalc->pact = TRUE;
	if(fetch_values(pcalc)==0) {
		if(calcPerform(&pcalc->a,&pcalc->val,pcalc->rpcl)) {
			recGblSetSevr(pcalc,CALC_ALARM,VALID_ALARM);
		} else pcalc->udf = FALSE;
	}
	tsLocalTime(&pcalc->time);
	/* check for alarms */
	alarm(pcalc);
	/* check event list */
	monitor(pcalc);
	/* process the forward scan link record */
	if (pcalc->flnk.type==DB_LINK) dbScanPassive(((struct dbAddr *)pcalc->flnk.value.db_link.pdbAddr)->precord);
	pcalc->pact = FALSE;
	return(0);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int	   	  after;
{
    long		status;
    struct calcRecord  	*pcalc = (struct calcRecord *)(paddr->precord);
    int           	special_type = paddr->special;
    short error_number;
    char rpbuf[80];

    if(!after) return(0);
    switch(special_type) {
    case(SPC_CALC):
	status=postfix(pcalc->calc,rpbuf,&error_number);
	if(status) return(status);
	memcpy(pcalc->rpcl,rpbuf,sizeof(pcalc->rpcl));
	db_post_events(pcalc,pcalc->calc,DBE_VALUE);
	return(0);
    default:
	recGblDbaddrError(S_db_badChoice,paddr,"calc: special");
	return(S_db_badChoice);
    }
}

static long get_value(pcalc,pvdes)
    struct calcRecord		*pcalc;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_DOUBLE;
    pvdes->no_elements=1;
    (double *)(pvdes->pvalue) = &pcalc->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    strncpy(units,pcalc->egu,sizeof(pcalc->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    *precision = pcalc->prec;
    if(paddr->pfield == (void *)&pcalc->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pcalc->val){
        pgd->upper_disp_limit = pcalc->hopr;
        pgd->lower_disp_limit = pcalc->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pcalc->val){
        pcd->upper_ctrl_limit = pcalc->hopr;
        pcd->lower_ctrl_limit = pcalc->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}
static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    pad->upper_alarm_limit = pcalc->hihi;
    pad->upper_warning_limit = pcalc->high;
    pad->lower_warning_limit = pcalc->low;
    pad->lower_alarm_limit = pcalc->lolo;
    return(0);
}


static void alarm(pcalc)
    struct calcRecord	*pcalc;
{
	double	ftemp;
	double	val=pcalc->val;

	if(pcalc->udf == TRUE ) {
		recGblSetSevr(pcalc,UDF_ALARM,VALID_ALARM);
		return;
	}
        /* if difference is not > hysterisis use lalm not val */
        ftemp = pcalc->lalm - pcalc->val;
        if(ftemp<0.0) ftemp = -ftemp;
        if (ftemp < pcalc->hyst) val=pcalc->lalm;

        /* alarm condition hihi */
        if (val > pcalc->hihi && recGblSetSevr(pcalc,HIHI_ALARM,pcalc->hhsv)){
                pcalc->lalm = val;
                return;
        }

        /* alarm condition lolo */
        if (val < pcalc->lolo && recGblSetSevr(pcalc,LOLO_ALARM,pcalc->llsv)){
                pcalc->lalm = val;
                return;
        }

        /* alarm condition high */
        if (val > pcalc->high && recGblSetSevr(pcalc,HIGH_ALARM,pcalc->hsv)){
                pcalc->lalm = val;
                return;
        }

        /* alarm condition low */
        if (val < pcalc->low && recGblSetSevr(pcalc,LOW_ALARM,pcalc->lsv)){
                pcalc->lalm = val;
                return;
        }
        return;
}

static void monitor(pcalc)
    struct calcRecord	*pcalc;
{
	unsigned short	monitor_mask;
	double		delta;
        short           stat,sevr,nsta,nsev;
	double		*pnew;
	double		*pprev;
	int		i;

        /* get previous stat and sevr  and new stat and sevr*/
        recGblResetSevr(pcalc,stat,sevr,nsta,nsev);

        monitor_mask = 0;

        /* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev) {
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and nsev fields */
                db_post_events(pcalc,&pcalc->stat,DBE_VALUE);
                db_post_events(pcalc,&pcalc->sevr,DBE_VALUE);
        }
        /* check for value change */
        delta = pcalc->mlst - pcalc->val;
        if(delta<0.0) delta = -delta;
        if (delta > pcalc->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                pcalc->mlst = pcalc->val;
        }
        /* check for archive change */
        delta = pcalc->alst - pcalc->val;
        if(delta<0.0) delta = -delta;
        if (delta > pcalc->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                pcalc->alst = pcalc->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pcalc,&pcalc->val,monitor_mask);
		/* check all input fields for changes*/
		for(i=0, pnew=&pcalc->a, pprev=&pcalc->la; i<ARG_MAX; i++, pnew++, pprev++) {
			if(*pnew != *pprev) {
				db_post_events(pcalc,pnew,monitor_mask|DBE_VALUE);
				*pprev = *pnew;
			}
		}
        }
        return;
}

static int fetch_values(pcalc)
struct calcRecord *pcalc;
{
	struct link	*plink;	/* structure of the link field  */
	double		*pvalue;
	long		options,nRequest;
	int		i;
	long	status;

	for(i=0, plink=&pcalc->inpa, pvalue=&pcalc->a; i<ARG_MAX; i++, plink++, pvalue++) {
                if (plink->type == CA_LINK)
                {
                    if (dbCaGetLink(plink))
                        printf("fetch_values() problem in dbCaGetLink()\n");
                }
                else
                {
                    if(plink->type==DB_LINK)
                    {
                        options=0;
                        nRequest=1;
			status = dbGetLink(&plink->value.db_link,(struct dbCommon *)pcalc,DBR_DOUBLE, pvalue,&options,&nRequest);
                        if(status!=0) {
                                recGblSetSevr(pcalc,LINK_ALARM,VALID_ALARM);
                                return(-1);
                        }
                    } /* endif */
                } /* endif */
	}
	return(0);
}
