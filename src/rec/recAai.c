/* recAai.c */

/* recAai.c - Record Support Routines for Array Analog In records */
/*
 *      Original Author: Dave Barker
 *      Current Author:  Dave Barker
 *      Date:            10/24/93
 *
 *      C  E  B  A  F
 *     
 *      Continuous Electron Beam Accelerator Facility
 *      Newport News, Virginia, USA.
 *
 *      Copyright SURA CEBAF 1993.
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
#include	<aaiRecord.h>

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

struct rset aaiRSET={
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

struct aaidset { /* aai dset */
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_aai; /*returns: (-1,0)=>(failure,success)*/
};

/*sizes of field types*/
static int sizeofTypes[] = {0,1,1,2,2,4,4,4,8,2};

static void monitor();
static long readValue();

/*Following from timing system          */
extern unsigned int     gts_trigger_counter;


static long init_record(paai,pass)
    struct aaiRecord	*paai;
    int pass;
{
    struct aaidset *pdset;
    long status;

    if (pass==0){
	if(paai->nelm<=0) paai->nelm=1;
	return(0);
    }

    /* aai.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    switch (paai->siml.type) {
    case (CONSTANT) :
        paai->simm = paai->siml.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(paai->siml), (void *) paai, "SIMM");
        if(status) return(status);
        break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)paai,
                "aai: init_record Illegal SIML field");
        return(S_db_badField);
    }

    /* aai.siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    switch (paai->siol.type) {
    case (CONSTANT) :
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(paai->siol), (void *) paai, "VAL");
        if(status) return(status);
        break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)paai,
                "aai: init_record Illegal SIOL field");
        return(S_db_badField);
    }

    /* must have dset defined */
    if(!(pdset = (struct aaidset *)(paai->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)paai,"aai: init_record");
        return(S_dev_noDSET);
    }
    /* must have read_aai function defined */
    if( (pdset->number < 5) || (pdset->read_aai == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)paai,"aai: init_record");
        return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	/* init records sets the bptr to point to the data */
        if((status=(*pdset->init_record)(paai))) return(status);
    }
    return(0);
}

static long process(paai)
	struct aaiRecord	*paai;
{
        struct aaidset   *pdset = (struct aaidset *)(paai->dset);
	long		 status;
	unsigned char    pact=paai->pact;

        if( (pdset==NULL) || (pdset->read_aai==NULL) ) {
                paai->pact=TRUE;
                recGblRecordError(S_dev_missingSup,(void *)paai,"read_aai");
                return(S_dev_missingSup);
        }
        /* event throttling */
        if (paai->scan == SCAN_IO_EVENT){
                if ((paai->evnt != 0)  && (gts_trigger_counter != 0)){
                        if ((gts_trigger_counter % paai->evnt) != 0){
	                        status=readValue(paai);
                                return(0);
                        }
                }
        }

	if ( pact ) return(0);

	status=readValue(paai); /* read the new value */

	paai->udf=FALSE;
	tsLocalTime(&paai->time);

	monitor(paai);
        /* process the forward scan link record */
        recGblFwdLink(paai);

        paai->pact=FALSE;
        return(0);
}

static long get_value(paai,pvdes)
    struct aaiRecord		*paai;
    struct valueDes	*pvdes;
{

    pvdes->no_elements=paai->nelm;
    pvdes->pvalue = paai->bptr;
    pvdes->field_type = paai->ftvl;
    return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct aaiRecord *paai=(struct aaiRecord *)paddr->precord;

    paddr->pfield = (void *)(paai->bptr);
    paddr->no_elements = paai->nelm;
    paddr->field_type = paai->ftvl;
    if(paai->ftvl==0)  paddr->field_size = MAX_STRING_SIZE;
    else paddr->field_size = sizeofTypes[paai->ftvl];
    paddr->dbr_field_type = paai->ftvl;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long	  *no_elements;
    long	  *offset;
{
    struct aaiRecord	*paai=(struct aaiRecord *)paddr->precord;

    *no_elements =  paai->nelm;
    *offset = 0;
    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
{
    struct aaiRecord	*paai=(struct aaiRecord *)paddr->precord;

    paai->nelm = nNew;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct aaiRecord	*paai=(struct aaiRecord *)paddr->precord;

    strncpy(units,paai->egu,sizeof(paai->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct aaiRecord	*paai=(struct aaiRecord *)paddr->precord;

    *precision = paai->prec;
    if(paddr->pfield==(void *)paai->bptr) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct aaiRecord     *paai=(struct aaiRecord *)paddr->precord;

    if(paddr->pfield==(void *)paai->bptr){
        pgd->upper_disp_limit = paai->hopr;
        pgd->lower_disp_limit = paai->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}
static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct aaiRecord     *paai=(struct aaiRecord *)paddr->precord;

    if(paddr->pfield==(void *)paai->bptr){
        pcd->upper_ctrl_limit = paai->hopr;
        pcd->lower_ctrl_limit = paai->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static void monitor(paai)
    struct aaiRecord	*paai;
{
	unsigned short	monitor_mask;

	monitor_mask = recGblResetAlarms(paai);
	monitor_mask |= (DBE_LOG|DBE_VALUE);
	if(monitor_mask) db_post_events(paai,paai->bptr,monitor_mask);
	return;

}

static long readValue(paai)
        struct aaiRecord *paai;
{
        long            status;
        struct aaidset  *pdset = (struct aaidset *) (paai->dset);
	long            nRequest=1;
	long            options=0;


        if (paai->pact == TRUE){
		/* no asyn allowed, pact true means do not process */
                return(0);
        }

        status=recGblGetLinkValue(&(paai->siml),
                (void *)paai,DBR_ENUM,&(paai->simm),&options,&nRequest);
        if (status)
                return(status);

        if (paai->simm == NO){
		/* Call dev support */
                status=(*pdset->read_aai)(paai);
                return(status);
        }
        if (paai->simm == YES){
                /* Simm processing split performed in devSup */
		/* Call dev support */
                status=(*pdset->read_aai)(paai);
                return(status);
        } else {
                status=-1;
                recGblSetSevr(paai,SOFT_ALARM,INVALID_ALARM);
                return(status);
        }
        recGblSetSevr(paai,SIMM_ALARM,paai->sims);

        return(status);
}

