/* recWaveform.c */
/* share/src/rec $Id$ */

/* recWaveform.c - Record Support Routines for Waveform records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            7-14-89
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
 * .01  11-27-89        lrd     using one more buffer entry than allocated
 * .02  11-27-89        lrd     throttle on monitor requests
 * .03  02-27-90        lrd     handle character value links for Joerger
 *                              digitizer (convert to short array for access
 * .04  03-05-90        lrd     add averaging of entire waveforms
 * .05  04-02-90        ba/lrd  fix the joerger processing and
 *                              add get_a_byte macro
 *                              fix the determination of buff_size
 * .06  04-11-90        lrd     make locals static
 * .07  05-02-90        lrd     fix mdct so that it would remain 0 on the
 *                              pass where the monitors are sent
 * .08  05-08-90        ba      mdct is never equal to mcnt during record pro-
 *                              cessing, causing some code never to run. Fix
 *                              is to check (mdct==mcnt || mdct==0) to indicate
 *                              first time through an averaging loop.
 * .09  07-26-90        lrd     fixed the N-to-1 character waveformion
 *                              value was not initialized
 * .10  10-11-90	mrk	Made changes for new record support
 * .11  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .12  12-18-91        jba     Changed E_IO_INTERRUPT to SCAN_IO_EVENT, added dbScan.h
 * .13  02-05-92	jba	Changed function arguments from paddr to precord 
 * .14  02-28-92        jba     Changed get_precision,get_graphic_double,get_control_double
 * .15  02-28-92	jba	ANSI C changes
 * .16  04-10-92        jba     pact now used to test for asyn processing, not status
 * .17  04-18-92        jba     removed process from dev init_record parms
 * .18  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .19  08-14-92        jba     Added simulation processing
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<stdlib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<dbScan.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<waveformRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
static long get_value();
static long cvt_dbaddr();
static long get_array_info();
static long put_array_info();
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
#define get_alarm_double NULL

struct rset waveformRSET={
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

struct wfdset { /* waveform dset */
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_wf; /*returns: (-1,0)=>(failure,success)*/
};

/*sizes of field types*/
static int sizeofTypes[] = {0,1,1,2,2,4,4,4,8,2};

static void monitor();
static long readValue();

/*Following from timing system          */
extern unsigned int     gts_trigger_counter;


static long init_record(pwf,pass)
    struct waveformRecord	*pwf;
    int pass;
{
    struct wfdset *pdset;
    long status;

    if (pass==0){
	if(pwf->nelm<=0) pwf->nelm=1;
	if(pwf->ftvl == 0) {
		pwf->bptr = (char *)calloc(pwf->nelm,MAX_STRING_SIZE);
	} else {
		if(pwf->ftvl<0|| pwf->ftvl>DBF_ENUM) pwf->ftvl=2;
		pwf->bptr = (char *)calloc(pwf->nelm,sizeofTypes[pwf->ftvl]);
	}
	pwf->nord = 0;
	return(0);
    }

    /* wf.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    switch (pwf->siml.type) {
    case (CONSTANT) :
        pwf->simm = pwf->siml.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pwf->siml), (void *) pwf, "SIMM");
        if(status) return(status);
        break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pwf,
                "wf: init_record Illegal SIML field");
        return(S_db_badField);
    }

    /* wf.siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    switch (pwf->siol.type) {
    case (CONSTANT) :
        pwf->nord = 0;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pwf->siol), (void *) pwf, "VAL");
        if(status) return(status);
        break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pwf,
                "wf: init_record Illegal SIOL field");
        return(S_db_badField);
    }

    /* must have dset defined */
    if(!(pdset = (struct wfdset *)(pwf->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)pwf,"wf: init_record");
        return(S_dev_noDSET);
    }
    /* must have read_wf function defined */
    if( (pdset->number < 5) || (pdset->read_wf == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)pwf,"wf: init_record");
        return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
        if((status=(*pdset->init_record)(pwf))) return(status);
    }
    return(0);
}

static long process(pwf)
	struct waveformRecord	*pwf;
{
        struct wfdset   *pdset = (struct wfdset *)(pwf->dset);
	long		 status;
	unsigned char    pact=pwf->pact;

        if( (pdset==NULL) || (pdset->read_wf==NULL) ) {
                pwf->pact=TRUE;
                recGblRecordError(S_dev_missingSup,(void *)pwf,"read_wf");
                return(S_dev_missingSup);
        }
        /* event throttling */
        if (pwf->scan == SCAN_IO_EVENT){
                if ((pwf->evnt != 0)  && (gts_trigger_counter != 0)){
                        if ((gts_trigger_counter % pwf->evnt) != 0){
	                        status=readValue(pwf);
                                return(0);
                        }
                }
        }

	if ( pact && pwf->busy ) return(0);

	status=readValue(pwf); /* read the new value */
	/* check if device support set pact */
	if ( !pact && pwf->pact ) return(0);
        pwf->pact = TRUE;

	pwf->udf=FALSE;
	tsLocalTime(&pwf->time);

	monitor(pwf);
        /* process the forward scan link record */
        recGblFwdLink(pwf);

        pwf->pact=FALSE;
        return(0);
}

static long get_value(pwf,pvdes)
    struct waveformRecord		*pwf;
    struct valueDes	*pvdes;
{

    pvdes->no_elements=pwf->nord;
    pvdes->pvalue = pwf->bptr;
    pvdes->field_type = pwf->ftvl;
    return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct waveformRecord *pwf=(struct waveformRecord *)paddr->precord;

    paddr->pfield = (void *)(pwf->bptr);
    paddr->no_elements = pwf->nelm;
    paddr->field_type = pwf->ftvl;
    if(pwf->ftvl==0)  paddr->field_size = MAX_STRING_SIZE;
    else paddr->field_size = sizeofTypes[pwf->ftvl];
    paddr->dbr_field_type = pwf->ftvl;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long	  *no_elements;
    long	  *offset;
{
    struct waveformRecord	*pwf=(struct waveformRecord *)paddr->precord;

    *no_elements =  pwf->nord;
    *offset = 0;
    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
{
    struct waveformRecord	*pwf=(struct waveformRecord *)paddr->precord;

    pwf->nord = nNew;
    if(pwf->nord > pwf->nelm) pwf->nord = pwf->nelm;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct waveformRecord	*pwf=(struct waveformRecord *)paddr->precord;

    strncpy(units,pwf->egu,sizeof(pwf->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct waveformRecord	*pwf=(struct waveformRecord *)paddr->precord;

    *precision = pwf->prec;
    if(paddr->pfield==(void *)pwf->bptr) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct waveformRecord     *pwf=(struct waveformRecord *)paddr->precord;

    if(paddr->pfield==(void *)pwf->bptr){
        pgd->upper_disp_limit = pwf->hopr;
        pgd->lower_disp_limit = pwf->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}
static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct waveformRecord     *pwf=(struct waveformRecord *)paddr->precord;

    if(paddr->pfield==(void *)pwf->bptr){
        pcd->upper_ctrl_limit = pwf->hopr;
        pcd->lower_ctrl_limit = pwf->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static void monitor(pwf)
    struct waveformRecord	*pwf;
{
	unsigned short	monitor_mask;
        short           stat,sevr,nsta,nsev;

        /* get previous stat and sevr  and new stat and sevr*/
        recGblResetSevr(pwf,stat,sevr,nsta,nsev);

        /* Flags which events to fire on the value field */
        monitor_mask = 0;

        /* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev) {
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and nsev fields */
                db_post_events(pwf,&pwf->stat,DBE_VALUE);
                db_post_events(pwf,&pwf->sevr,DBE_VALUE);
        }
	monitor_mask |= (DBE_LOG|DBE_VALUE);
        if(monitor_mask) db_post_events(pwf,pwf->bptr,monitor_mask);
	return;

}

static long readValue(pwf)
        struct waveformRecord *pwf;
{
        long            status;
        struct wfdset   *pdset = (struct wfdset *) (pwf->dset);
	long            nRequest=1;
	long            options=0;


        if (pwf->pact == TRUE){
                status=(*pdset->read_wf)(pwf);
                return(status);
        }

        status=recGblGetLinkValue(&(pwf->siml),
                (void *)pwf,DBR_ENUM,&(pwf->simm),&options,&nRequest);
        if (status)
                return(status);

        if (pwf->simm == NO){
                status=(*pdset->read_wf)(pwf);
                return(status);
        }
        if (pwf->simm == YES){
        	nRequest=pwf->nelm;
                status=recGblGetLinkValue(&(pwf->siol),
                                (void *)pwf,pwf->ftvl,pwf->bptr,&options,&nRequest);
        	pwf->nord = nRequest;
                if (status==0){
                         pwf->udf=FALSE;
                }
        } else {
                status=-1;
                recGblSetSevr(pwf,SOFT_ALARM,INVALID_ALARM);
                return(status);
        }
        recGblSetSevr(pwf,SIMM_ALARM,pwf->sims);

        return(status);
}

