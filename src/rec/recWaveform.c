/* recWaveform.c - Record Support Routines for Waveform records
/* share/src/rec $Id$ */
 *
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
#include	<math.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<waveformRecord.h>

/* Create RSET - Record Support Entry Table*/
long report();
#define initialize NULL
long init_record();
long process();
#define special NULL
long get_precision();
long get_value();
long cvt_dbaddr();
long get_array_info();
long put_array_info();
#define get_enum_str NULL
long get_units();
long get_graphic_double();
long get_control_double();
#define get_enum_strs NULL

struct rset waveformRSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_precision,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_enum_str,
	get_units,
	get_graphic_double,
	get_control_double,
	get_enum_strs };
/* because the driver does all the work just declare device support here*/
struct dset devWaveformSoft={4,NULL,NULL,NULL,NULL};
struct dset devXycom566SingleChan={4,NULL,NULL,NULL,NULL};
struct dset devLecroy8837={4,NULL,NULL,NULL,NULL};
struct dset devJoergerVtr1={4,NULL,NULL,NULL,NULL};


static long report(fp,paddr)
    FILE	  *fp;
    struct dbAddr *paddr;
{
    struct waveformRecord	*pwaveform=(struct waveformRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    return(0);
}

static long init_record(pwaveform)
    struct waveformRecord	*pwaveform;
{
    long status;

    /* This routine may get called twice. Once by cvt_dbaddr. Once by iocInit*/
    /* allocate bptr and sptr */
    if(pwaveform->bptr==NULL) {
	if(pwaveform->nsam<=0) pwaveform->nsam=1;
	pwaveform->bptr = (char *)malloc(pwaveform->nsam * pwaveform->selm);
    }
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct waveformRecord	*pwaveform=(struct waveformRecord *)paddr->precord;

    *precision = pwaveform->prec;
    return(0L);
}

static long get_value(pwaveform,pvdes)
    struct waveformRecord		*pwaveform;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_FLOAT;
    pvdes->no_elements=pwaveform->nsam;
    (float *)(pvdes->pvalue) = pwaveform->bptr;
    return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct waveformRecord *pwaveform=(struct waveformRecord *)paddr->precord;

    /* This may get called before init_record. If so just call it*/
    if(pwaveform->bptr==NULL) init_record(paddr);

    paddr->pfield = (caddr_t)(pwaveform->bptr);
    paddr->no_elements = pwaveform->nsam;
    paddr->field_type = DBF_FLOAT;
    paddr->field_size = sizeof(float);
    paddr->dbr_field_type = DBF_FLOAT;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long	  *no_elements;
    long	  *offset;
{
    struct waveformRecord	*pwaveform=(struct waveformRecord *)paddr->precord;

    *no_elements =  pwaveform->nuse;
    *offset = inx+1;
    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
    long	  offset;
{
    struct waveformRecord	*pwaveform=(struct waveformRecord *)paddr->precord;

    pwaveform->inx = (pwaveform->inx + nNew) % (pwaveform->nsam);
    pwaveform->nuse = (pwaveform->nuse + nNew);
    if(pwaveform->nuse > pwaveform->nsam) pwaveform->nuse = pwaveform->nsam;
    return(0);
}


static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct waveformRecord	*pwaveform=(struct waveformRecord *)paddr->precord;

    strncpy(units,pwaveform->egu,sizeof(pwaveform->egu));
    return(0L);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct waveformRecord     *pwaveform=(struct waveformRecord *)paddr->precord;

    pgd->upper_disp_limit = pwaveform->hopr;
    pgd->lower_disp_limit = pwaveform->lopr;
    pgd->upper_alarm_limit = 0.0;
    pgd->upper_warning_limit = 0.0;
    pgd->lower_warning_limit = 0.0;
    pgd->lower_alarm_limit = 0.0;
    return(0L);
}
static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct waveformRecord     *pwaveform=(struct waveformRecord *)paddr->precord;

    pgd->upper_disp_limit = pwaveform->hopr;
    pgd->lower_disp_limit = pwaveform->lopr;
    return(0L);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct waveformRecord	*pwaveform=(struct waveformRecord *)(paddr->precord);
	struct waveformdset	*pdset = (struct waveformdset *)(pwaveform->dset);
	long		 status;

	pwaveform->achn = 0;
	/* read inputs  */
	if (do_waveformion(pwaveform) < 0){
		if (pwaveform->stat != READ_ALARM){
			pwaveform->stat = READ_ALARM;
			pwaveform->sevr = MAJOR_ALARM;
			pwaveform->achn = 1;
			monitor_waveform(pwaveform);
		}
		pwaveform->pact = 0;
		return(0);
	}else{
		if (pwaveform->stat == READ_ALARM){
			pwaveform->stat = NO_ALARM;
			pwaveform->sevr = NO_ALARM;
			pwaveform->achn = 1;
		}
	}


	/* check event list */
	if(!pwaveform->disa) status = monitor(pwaveform);

	/* process the forward scan link record */
	if (pwaveform->flnk.type==DB_LINK) dbScanPassive(&pwaveform->flnk.value);

	pwaveform->pact=FALSE;
	return(status);
}

static long monitor(pwaveform)
    struct waveformRecord	*pwaveform;
{
	unsigned short	monitor_mask;
	float		delta;

	/* anyone wwaveformting for an event on this record */
	if (pwaveform->mlis.count == 0) return(0L);

	/* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (pwaveform->achn){
		/* post events for alarm condition change and value change */
		monitor_mask = DBE_ALARM;

		/* post stat and sevr fields */
		db_post_events(pwaveform,&pwaveform->stat,DBE_VALUE);
		db_post_events(pwaveform,&pwaveform->sevr,DBE_VALUE);

		/* update last value monitored */
		pwaveform->mlst = pwaveform->val;
	}
	monitor_mask |= DBE_LOG|DBE_VALUE;
	db_post_events(pwaveform,&pwaveform->val,monitor_mask);
	return(0L);
}


/*
 * WFCB_READ
 *
 * callback routine when the waveform is read 
 */
static wfcb_read(pwf,no_read,pdata)
register struct waveform	*pwf;
register int			no_read;
register char			*pdata;
{
	if (pwf->inp.type == VME_IO){
		pwf->bptr = pdata;		/* mem addr of buffer */
		pwf->nord = no_read;		/* number of values read */
		pwf->busy = 0;

		/* post events on value, busy and number read */
        	if (pwf->mqct != 0){
			db_post_events(pwf,&pwf->busy,DBE_VALUE);
			db_post_events(pwf,&pwf->nord,DBE_VALUE);
			db_post_events(pwf,&pwf->val,DBE_VALUE|DBE_LOG);
		}
	}	

        /* process the forward scan link record */
        if (pwf->flnk.type == DB_LINK)
                db_scan(&pwf->flnk.value);

	/* determine if the waveform is to be re-armed */
	if (pwf->rarm){
		arm_wf(pwf);
		pwf->rarm = 0;
		if (pwf->mqct)
			db_post_events(pwf,&pwf->rarm,DBE_VALUE);
	}

	/* record is only unlocked when the waveform is read */
	/* unlock the record */
	pwf->lock = 0;

}

/*
 * PROCESS_WAVEFORM
 *
 * process waveform record 
 */
process_waveform(pwf)
register struct waveform	*pwf;	/* pointer to the analog input record */
{
	pwf->achn = 0;		/* flags alarm condition changed */

	/* lock the record */
	if (pwf->lock){
		if (pwf->lcnt >= MAX_LOCK){
			pwf->stat = SCAN_ALARM;
			pwf->sevr = MAJOR;
			pwf->achn = 1;
		}else{
			pwf->lcnt++;
		}
		return;
	}
	pwf->lcnt = 0;
	pwf->lock = 1;

	/* initialize the record */
	if(pwf->init == 0){
		init_wf(pwf);
		pwf->init = 1;
	}

	/* arm the waveform */
	arm_wf(pwf);

	/* monitors on alarm condition */
        if (pwf->mqct != 0){
		if (pwf->achn){
			db_post_events(pwf,&pwf->val,DBE_ALARM);
			db_post_events(pwf,&pwf->stat,DBE_VALUE);
			db_post_events(pwf,&pwf->sevr,DBE_VALUE);
		}
		if (pwf->inp.type == CONSTANT){
			db_post_events(pwf,&pwf->val,DBE_VALUE);
		}
	}
}

/*
 * ARM_WF
 *
 * arm the waveform 
 */
static arm_wf(pwf)
register struct waveform	*pwf;
{
	short	temp;

	switch(pwf->inp.type){
	case(VME_IO):
		if (pwf->busy) return;		/* already armed */

		/* setup callback for data collection complete */
		if (wf_driver(pwf->type,	/* card type */
		  pwf->inp.value.vmeio.card,	/* card number */
		  wfcb_read,			/* callback routine */
		  pwf) < 0){			/* callback argument */
			if (pwf->stat != READ_ALARM){
				pwf->stat = READ_ALARM;
				pwf->sevr = MAJOR;
				pwf->achn = 1;
			}
			return(-1);
		}else{
			pwf->busy = 1;
        		if (pwf->mqct != 0)
				db_post_events(pwf,&pwf->busy,DBE_VALUE);
			if (pwf->stat == READ_ALARM){
				pwf->sevr = pwf->stat = NO_ALARM;
				pwf->achn = 1;
			}
		}
		return(0);

	case(CAMAC_IO):
	case(CONSTANT):
	case(DB_LINK):
	default:
		return(-1);
	}	

}

/*
 * INIT_WF
 *
 * initialize the waveform parameters
 */
static init_wf(pwf)
register struct waveform	*pwf;
{
	short	temp;

	/* initialize the monitor mutual exclusion semaphore */
	semInit(&pwf->msem[0]);
	semGive(&pwf->msem[0]);		/* initially available	*/


	switch(pwf->inp.type){
	case(VME_IO):
		return(0);

	case(CONSTANT):
		pwf->bptr = (char *)malloc(pwf->nelm * pwf->selm);
		pwf->nord = pwf->nelm;
		return(0);

	case(DB_LINK):
	case(CAMAC_IO):
	default:
		return(-1);
	}	

}
