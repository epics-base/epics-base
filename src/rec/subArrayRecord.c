/*************************************************************************\
* Copyright (c) 2002 Lawrence Berkeley Laboratory,The Control Systems
*     Group, Systems Engineering Department
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recSubArray.c */
/* recSubArray.c - Record Support Routines for SubArray records 
 *
 *
 *      Author:         Carl Lionberger
 *      Date:           090293
 *
 *      NOTES:
 * Derived from waveform record. 
 * Modification Log:
 * -----------------
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
#include "dbScan.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#define GEN_SIZE_OFFSET
#include "subArrayRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
#define get_value NULL
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

rset subArrayRSET={
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
epicsExportAddress(rset,subArrayRSET);

struct sadset { /* subArray dset */
        long            number;
        DEVSUPFUN       dev_report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_sa; /*returns: (-1,0)=>(failure,success)*/
};

/*sizes of field types*/
static int sizeofTypes[] = {MAX_STRING_SIZE,1,1,2,2,4,4,4,8,2};

static void monitor();
static long readValue();

/*Following from timing system          */
/*
extern unsigned int     gts_trigger_counter;
*/


static long init_record(psa,pass)
    struct subArrayRecord	*psa;
    int pass;
{
    struct sadset *pdset;
    long status;

    if (pass==0){
        if(psa->malm<=0) psa->malm=1;
	
	if(psa->ftvl>DBF_ENUM) psa->ftvl=2;
	psa->bptr = (char *)calloc(psa->malm,sizeofTypes[psa->ftvl]);
        psa->nord = 0;
	return(0);
    }

    /* must have dset defined */
    if(!(pdset = (struct sadset *)(psa->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)psa,"sa: init_record");
        return(S_dev_noDSET);
    }
    /* must have read_sa function defined */
    if( (pdset->number < 5) || (pdset->read_sa == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)psa,"sa: init_record");
        return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
        if((status=(*pdset->init_record)(psa))) return(status);
    }
    return(0);
}

static long process(psa)
	struct subArrayRecord	*psa;
{
        struct sadset   *pdset = (struct sadset *)(psa->dset);
	long		 status;
        unsigned char  	 pact=psa->pact;

        if( (pdset==NULL) || (pdset->read_sa==NULL) ) {
                psa->pact=TRUE;
                recGblRecordError(S_dev_missingSup,(void *)psa,"read_sa");
                return(S_dev_missingSup);
        }

        if ( pact && psa->busy ) return(0); 
	status=readValue(psa); /* read the new value */
        if (!pact && psa->pact) return(0);
        psa->pact = TRUE;

        psa->udf=FALSE;
        recGblGetTimeStamp(psa);
	monitor(psa);

        /* process the forward scan link record */
        recGblFwdLink(psa);

        psa->pact=FALSE;
        return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct subArrayRecord *psa=(struct subArrayRecord *)paddr->precord;

    paddr->pfield = psa->bptr;
    if (!psa->udf && psa->nelm > psa->nord)
       paddr->no_elements = psa->nord;
    else
       paddr->no_elements = psa->nelm;
    paddr->field_type = psa->ftvl;
    paddr->field_size = sizeofTypes[psa->ftvl];
    paddr->dbr_field_type = psa->ftvl;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long	  *no_elements;
    long	  *offset;
{
    struct subArrayRecord	*psa=(struct subArrayRecord *)paddr->precord;

    if (!psa->udf && psa->nelm > psa->nord)
       *no_elements = psa->nord;
    else
       *no_elements = psa->nelm;
    *offset = 0;
    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
{
    struct subArrayRecord	*psa=(struct subArrayRecord *)paddr->precord;

    if(nNew > psa->malm) 
       psa->nord = psa->malm;
    else
       psa->nord = nNew;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct subArrayRecord	*psa=(struct subArrayRecord *)paddr->precord;

    strncpy(units,psa->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct subArrayRecord	*psa=(struct subArrayRecord *)paddr->precord;
    int				fieldIndex = dbGetFieldIndex(paddr);

    *precision = psa->prec;
    if(fieldIndex == subArrayRecordVAL) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct subArrayRecord     *psa=(struct subArrayRecord *)paddr->precord;
    int				fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == subArrayRecordVAL) {
        pgd->upper_disp_limit = psa->hopr;
        pgd->lower_disp_limit = psa->lopr;
        return(0);
    } 
    if(fieldIndex == subArrayRecordINDX) {
        pgd->upper_disp_limit = psa->malm - 1;
        pgd->lower_disp_limit = 0;
        return(0);
    } 
    if(fieldIndex == subArrayRecordNELM) {
        pgd->upper_disp_limit = psa->malm;
        pgd->lower_disp_limit = 1;
        return(0);
    } 
    recGblGetGraphicDouble(paddr,pgd);
    return(0);
}
static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct subArrayRecord     *psa=(struct subArrayRecord *)paddr->precord;
    int				fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == subArrayRecordVAL) {
        pcd->upper_ctrl_limit = psa->hopr;
        pcd->lower_ctrl_limit = psa->lopr;
        return(0);
    } 
    if(fieldIndex == subArrayRecordINDX) {
        pcd->upper_ctrl_limit = psa->malm - 1;
        pcd->lower_ctrl_limit = 0;
        return(0);
    } 
    if(fieldIndex == subArrayRecordNELM) {
        pcd->upper_ctrl_limit = psa->malm;
        pcd->lower_ctrl_limit = 1;
        return(0);
    } 
    recGblGetControlDouble(paddr,pcd);
    return(0);
}

static void monitor(psa)
    struct subArrayRecord	*psa;
{
	unsigned short	monitor_mask;

        /* get previous stat and sevr  and new stat and sevr*/
        monitor_mask = recGblResetAlarms(psa);

	monitor_mask |= (DBE_LOG|DBE_VALUE);
        if(monitor_mask)
            db_post_events(psa, psa->bptr, monitor_mask);
	return;
}

static long readValue(psa)
        struct subArrayRecord *psa;
{
        long            status;
        struct sadset   *pdset = (struct sadset *) (psa->dset);

        if (psa->nelm > psa->malm)
        {
           psa->nelm = psa->malm;
        }
        if (psa->indx >= psa->malm)
        {
           psa->indx = psa->malm - 1;
        }
        status = (*pdset->read_sa)(psa);

        if (psa->nord <= 0)
        {
           status = -1;
           psa->indx = 0;
        }
        return(status);
}

