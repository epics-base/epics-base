/* recWaveform.c - Record Support Routines for Waveform records
/* share/src/rec $Id$ */
/*
 * Author:      Bob Dalesio
 * Date:        7-14-89

 *
 *	Control System Software for the GTA Project
 *
 *	Copyright 1988, 1989, the Regents of the University of California.
 *
 *	This software was produced under a U.S. Government contract
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *	operated by the University of California for the U.S. Department
 *	of Energy.
 *
 *	Developed by the Controls and Automation Group (AT-8)
 *	Accelerator Technology Division
 *	Los Alamos National Laboratory
 *
 *	Direct inqueries to:
 *	Bob Dalesio, AT-8, Mail Stop H820
 *	Los Alamos National Laboratory
 *	Los Alamos, New Mexico 87545
 *	Phone: (505) 667-3414
 *	E-mail: dalesio@luke.lanl.gov
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
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<waveformRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
long init_record();
long process();
#define special NULL
long get_value();
long cvt_dbaddr();
long get_array_info();
long put_array_info();
long get_units();
long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
long get_graphic_double();
long get_control_double();
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
        DEVSUPFUN       read_wf;/*(0,1)=> success and */
                        /*(convert,don't continue)*/
};

/*sizes of field types*/
static int sizeofTypes[] = {0,1,1,2,2,4,4,4,8,2};
void monitor();


static long init_record(pwf)
    struct waveformRecord	*pwf;
{
    struct wfdset *pdset;
    long status;


    /* This routine may get called twice. Once by cvt_dbaddr. Once by iocInit*/
    if(pwf->bptr==NULL) {
	if(pwf->nelm<=0) pwf->nelm=1;
	if(pwf->ftvl == 0) {
		pwf->bptr = (char *)calloc(pwf->nelm,MAX_STRING_SIZE);
	} else {
		if(pwf->ftvl<0|| pwf->ftvl>DBF_ENUM) pwf->ftvl=2;
		pwf->bptr = (char *)calloc(pwf->nelm,sizeofTypes[pwf->ftvl]);
	}
	pwf->nord = 0;
	/* must have read_wf function defined */
	if(!(pdset = (struct wfdset *)(pwf->dset))) {
	    recGblRecordError(S_dev_noDSET,pwf,"wf: init_record");
	    return(S_dev_noDSET);
	}
	if( (pdset->number < 5) || (pdset->read_wf == NULL) ) {
	    recGblRecordError(S_dev_missingSup,pwf,"wf: init_record");
	    return(S_dev_missingSup);
	}
	if( pdset->init_record ) {
	    if((status=(*pdset->init_record)(pwf,process))) return(status);
	}
    }
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
	struct waveformRecord	*pwf=(struct waveformRecord *)(paddr->precord);
        struct wfdset   *pdset = (struct wfdset *)(pwf->dset);
	long		 status;

        if( (pdset==NULL) || (pdset->read_wf==NULL) ) {
                pwf->pact=TRUE;
                recGblRecordError(S_dev_missingSup,pwf,"read_wf");
                return(S_dev_missingSup);
        }
        /*pact must not be set true until read_wf completes*/
        status=(*pdset->read_wf)(pwf); /* read the new value */
        pwf->pact = TRUE;
        /* status is one if an asynchronous record is being processed*/
        if(status==1) return(0);
	tsLocalTime(&pwf->time);

	monitor(pwf);
        /* process the forward scan link record */
        if (pwf->flnk.type==DB_LINK) dbScanPassive(pwf->flnk.value.db_link.pdbAddr);

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

    /* This may get called before init_record. If so just call it*/
    if(pwf->bptr==NULL) init_record(pwf);
    paddr->pfield = (caddr_t)(pwf->bptr);
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
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct waveformRecord     *pwf=(struct waveformRecord *)paddr->precord;

    pgd->upper_disp_limit = pwf->hopr;
    pgd->lower_disp_limit = pwf->lopr;
    return(0);
}
static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct waveformRecord     *pwf=(struct waveformRecord *)paddr->precord;

    pcd->upper_ctrl_limit = pwf->hopr;
    pcd->lower_ctrl_limit = pwf->lopr;
    return(0);
}

static void monitor(pwf)
    struct waveformRecord	*pwf;
{
	unsigned short	monitor_mask;
        short           stat,sevr,nsta,nsev;

        /* get previous stat and sevr  and new stat and sevr*/
        stat=pwf->stat;
        sevr=pwf->sevr;
        nsta=pwf->nsta;
        nsev=pwf->nsev;
        /*set current stat and sevr*/
        pwf->stat = nsta;
        pwf->sevr = nsev;
        pwf->nsta = 0;
        pwf->nsev = 0;

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
