/*************************************************************************\
* Copyright (c) 2002 Southeastern Universities Research Association, as
*     Operator of Thomas Jefferson National Accelerator Facility.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recAao.c */

/* recAao.c - Record Support Routines for Array Analog Out records */
/*
 *      Original Author: Dave Barker
 *      Current Author:  Dave Barker
 *      Date:            10/28/93
 *
 *      C  E  B  A  F
 *     
 *      Continuous Electron Beam Accelerator Facility
 *      Newport News, Virginia, USA.
 *
 *      Copyright SURA CEBAF 1993.
 *
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
#include "dbFldTypes.h"
#include "dbScan.h"
#include "dbEvent.h"
#include "devSup.h"
#include "recSup.h"
#include "recGbl.h"
#define GEN_SIZE_OFFSET
#include "aaoRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

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

rset aaoRSET={
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
epicsExportAddress(rset,aaoRSET);

struct aaodset { /* aao dset */
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       write_aao; /*returns: (-1,0)=>(failure,success)*/
};

/*sizes of field types*/
static int sizeofTypes[] = {0,1,1,2,2,4,4,4,8,2};

static void monitor();
static long writeValue();



static long init_record(paao,pass)
    struct aaoRecord	*paao;
    int pass;
{
    struct aaodset *pdset;
    long status;

    if (pass==0){
	if(paao->nelm<=0) paao->nelm=1;
	return(0);
    }
    recGblInitConstantLink(&paao->siml,DBF_USHORT,&paao->simm);
    /* must have dset defined */
    if(!(pdset = (struct aaodset *)(paao->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)paao,"aao: init_record");
        return(S_dev_noDSET);
    }
    /* must have write_aao function defined */
    if( (pdset->number < 5) || (pdset->write_aao == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)paao,"aao: init_record");
        return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	/* init records sets the bptr to point to the data */
        if((status=(*pdset->init_record)(paao))) return(status);
    }
    return(0);
}

static long process(paao)
	struct aaoRecord	*paao;
{
        struct aaodset   *pdset = (struct aaodset *)(paao->dset);
	long		 status;
	unsigned char    pact=paao->pact;

        if( (pdset==NULL) || (pdset->write_aao==NULL) ) {
                paao->pact=TRUE;
                recGblRecordError(S_dev_missingSup,(void *)paao,"write_aao");
                return(S_dev_missingSup);
        }

	if ( pact ) return(0);

	status=writeValue(paao); /* write the data */

	paao->udf=FALSE;
	recGblGetTimeStamp(paao);

	monitor(paao);
        /* process the forward scan link record */
        recGblFwdLink(paao);

        paao->pact=FALSE;
        return(0);
}

static long get_value(paao,pvdes)
    struct aaoRecord		*paao;
    struct valueDes	*pvdes;
{

    pvdes->no_elements=paao->nelm;
    pvdes->pvalue = paao->bptr;
    pvdes->field_type = paao->ftvl;
    return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct aaoRecord *paao=(struct aaoRecord *)paddr->precord;

    paddr->pfield = (void *)(paao->bptr);
    paddr->no_elements = paao->nelm;
    paddr->field_type = paao->ftvl;
    if(paao->ftvl==0)  paddr->field_size = MAX_STRING_SIZE;
    else paddr->field_size = sizeofTypes[paao->ftvl];
    paddr->dbr_field_type = paao->ftvl;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long	  *no_elements;
    long	  *offset;
{
    struct aaoRecord	*paao=(struct aaoRecord *)paddr->precord;

    *no_elements =  paao->nelm;
    *offset = 0;
    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
{
    struct aaoRecord	*paao=(struct aaoRecord *)paddr->precord;

    paao->nelm = nNew;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct aaoRecord	*paao=(struct aaoRecord *)paddr->precord;

    strncpy(units,paao->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct aaoRecord	*paao=(struct aaoRecord *)paddr->precord;

    *precision = paao->prec;
    if(paddr->pfield==(void *)paao->bptr) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct aaoRecord     *paao=(struct aaoRecord *)paddr->precord;

    if(paddr->pfield==(void *)paao->bptr){
        pgd->upper_disp_limit = paao->hopr;
        pgd->lower_disp_limit = paao->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}
static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct aaoRecord     *paao=(struct aaoRecord *)paddr->precord;

    if(paddr->pfield==(void *)paao->bptr){
        pcd->upper_ctrl_limit = paao->hopr;
        pcd->lower_ctrl_limit = paao->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static void monitor(paao)
    struct aaoRecord	*paao;
{
	unsigned short	monitor_mask;

	monitor_mask = recGblResetAlarms(paao);
	monitor_mask |= (DBE_LOG|DBE_VALUE);
	if(monitor_mask) db_post_events(paao,paao->bptr,monitor_mask);
	return;

}

static long writeValue(paao)
        struct aaoRecord *paao;
{
        long            status;
        struct aaodset  *pdset = (struct aaodset *) (paao->dset);


        if (paao->pact == TRUE){
		/* no asyn allowed, pact true means do not process */
                return(0);
        }

        status=dbGetLink(&(paao->siml),DBR_ENUM,&(paao->simm),0,0);
        if (status)
                return(status);

        if (paao->simm == NO){
		/* Call dev support */
                status=(*pdset->write_aao)(paao);
                return(status);
        }
        if (paao->simm == YES){
		/* Call dev support */
                status=(*pdset->write_aao)(paao);
                return(status);
        }
        status=-1;
        recGblSetSevr(paao,SIMM_ALARM,INVALID_ALARM);
        return(status);
}

