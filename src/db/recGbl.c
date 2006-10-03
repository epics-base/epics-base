/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recGbl.c - Global record processing routines */
/* base/src/db $Id$ */

/*
 *      Author:          Marty Kraimer
 *      Date:            11-7-90
 */

#include <stddef.h>
#include <epicsStdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "dbDefs.h"
#include "epicsTime.h"
#include "epicsPrint.h"
#include "dbBase.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbAddr.h"
#include "db_field_log.h"
#include "errlog.h"
#include "devSup.h"
#include "dbCommon.h"
#include "caeventmask.h"
#define epicsExportSharedSymbols
#include "dbAccessDefs.h"
#include "dbNotify.h"
#include "dbCa.h"
#include "dbEvent.h"
#include "dbScan.h"
#include "recGbl.h"


/* Hook Routines */

RECGBL_ALARM_HOOK_ROUTINE recGblAlarmHook = NULL;

/* local routines */
static void getMaxRangeValues();



void epicsShareAPI recGblDbaddrError(
    long status,struct dbAddr *paddr,char *pmessage)
{
    dbCommon *precord = 0;
    dbFldDes	*pdbFldDes = 0;

    if(paddr) {
        pdbFldDes = paddr->pfldDes;
        precord = paddr->precord;
    }
    errPrintf(status,0,0,
        "PV: %s.%s "
        "error detected in routine: %s\n",
        (paddr ? precord->name : "Unknown"),
        (pdbFldDes ? pdbFldDes->name : ""),
        (pmessage ? pmessage : "Unknown"));
    return;
}

void epicsShareAPI recGblRecordError(long status,void *pdbc,char *pmessage)
{
    dbCommon	*precord = pdbc;

    errPrintf(status,0,0,
        "PV: %s %s\n",
        (precord ? precord->name : "Unknown"),
        (pmessage ? pmessage : ""));
    return;
}

void epicsShareAPI recGblRecSupError(
    long status,struct dbAddr *paddr,char *pmessage,
    char *psupport_name)
{
    dbCommon *precord = 0;
    dbFldDes	*pdbFldDes = 0;
    dbRecordType	*pdbRecordType = 0;

    if(paddr) {
        precord = paddr->precord;
        pdbFldDes=(dbFldDes *)(paddr->pfldDes);
        if(pdbFldDes) pdbRecordType = pdbFldDes->pdbRecordType;
    }
    errPrintf(status,0,0,
        "Record Support Routine (%s) "
        "Record Type %s "
        "PV %s.%s "
        " %s\n",
        (psupport_name ? psupport_name : "Unknown"),
        (pdbRecordType ? pdbRecordType->name : "Unknown"),
        (paddr ? precord->name : "Unknown"),
        (pdbFldDes ? pdbFldDes->name : ""),
        (pmessage ? pmessage : ""));
    return;
}

void epicsShareAPI recGblGetPrec(struct dbAddr *paddr,long *precision)
{
    dbFldDes               *pdbFldDes=(dbFldDes *)(paddr->pfldDes);

    switch(pdbFldDes->field_type){
    case(DBF_SHORT):
         *precision = 0;
         break;
    case(DBF_USHORT):
         *precision = 0;
         break;
    case(DBF_LONG):
         *precision = 0;
         break;
    case(DBF_ULONG):
         *precision = 0;
         break;
    case(DBF_FLOAT):
    case(DBF_DOUBLE):
	 if(*precision<0 || *precision>15) *precision=15;
         break;
    default:
         break;
    }
    return;
}

void epicsShareAPI recGblGetGraphicDouble(
    struct dbAddr *paddr,struct dbr_grDouble *pgd)
{
    dbFldDes               *pdbFldDes=(dbFldDes *)(paddr->pfldDes);

    getMaxRangeValues(pdbFldDes->field_type,&pgd->upper_disp_limit,
	&pgd->lower_disp_limit);

    return;
}

void epicsShareAPI recGblGetAlarmDouble(
    struct dbAddr *paddr,struct dbr_alDouble *pad)
{
    pad->upper_alarm_limit = 0;
    pad->upper_warning_limit = 0;
    pad->lower_warning_limit = 0;
    pad->lower_alarm_limit = 0;

    return;
}

void epicsShareAPI recGblGetControlDouble(
    struct dbAddr *paddr,struct dbr_ctrlDouble *pcd)
{
    dbFldDes               *pdbFldDes=(dbFldDes *)(paddr->pfldDes);

    getMaxRangeValues(pdbFldDes->field_type,&pcd->upper_ctrl_limit,
	&pcd->lower_ctrl_limit);

    return;
}

int  epicsShareAPI recGblInitConstantLink(
    struct link *plink,short dbftype,void *pdest)
{
    if(plink->type != CONSTANT) return(FALSE);
    if(!plink->value.constantStr) return(FALSE);
    switch(dbftype) {
    case DBF_STRING:
	strcpy((char *)pdest,plink->value.constantStr);
	break;
    case DBF_CHAR : {
	short	value;
	char	*pvalue = (char *)pdest;

	sscanf(plink->value.constantStr,"%hi",&value);
	*pvalue = value;
	}
	break;
    case DBF_UCHAR : {
	unsigned short	value;
	unsigned char	*pvalue = (unsigned char *)pdest;

	sscanf(plink->value.constantStr,"%hu",&value);
	*pvalue = value;
	}
	break;
    case DBF_SHORT : 
	sscanf(plink->value.constantStr,"%hi",(short *)pdest);
	break;
    case DBF_USHORT : 
    case DBF_ENUM : 
    case DBF_MENU : 
    case DBF_DEVICE : 
	sscanf(plink->value.constantStr,"%hu",(unsigned short *)pdest);
	break;
    case DBF_LONG : 
	sscanf(plink->value.constantStr,"%li",(long *)pdest);
	break;
    case DBF_ULONG : 
	sscanf(plink->value.constantStr,"%lu",(unsigned long *)pdest);
	break;
    case DBF_FLOAT : 
	epicsScanFloat(plink->value.constantStr, (float *)pdest);
	break;
    case DBF_DOUBLE : 
	epicsScanDouble(plink->value.constantStr, (double *)pdest);
	break;
    default:
	epicsPrintf("Error in recGblInitConstantLink: Illegal DBF type\n");
	return(FALSE);
    }
    return(TRUE);
}

unsigned short epicsShareAPI recGblResetAlarms(void *precord)
{
    dbCommon *pdbc = precord;
    unsigned short prev_stat = pdbc->stat;
    unsigned short prev_sevr = pdbc->sevr;
    unsigned short new_stat = pdbc->nsta;
    unsigned short new_sevr = pdbc->nsev;
    unsigned short val_mask = 0;
    unsigned short stat_mask = 0;

    pdbc->stat = new_stat;
    pdbc->sevr = new_sevr;
    pdbc->nsta = 0;
    pdbc->nsev = 0;

    if (prev_sevr != new_sevr) {
	stat_mask = DBE_ALARM;
	db_post_events(pdbc, &pdbc->sevr, DBE_VALUE);
    }
    if (prev_stat != new_stat) {
	stat_mask |= DBE_VALUE;
    }
    if (stat_mask) {
	db_post_events(pdbc, &pdbc->stat, stat_mask);
	val_mask = DBE_ALARM;

	if (!pdbc->ackt || new_sevr >= pdbc->acks) {
	    pdbc->acks = new_sevr;
	    db_post_events(pdbc, &pdbc->acks, DBE_VALUE);
	}

	if (recGblAlarmHook) {
	    (*recGblAlarmHook)(pdbc, prev_sevr, prev_stat);
	}
    }
    return val_mask;
}

void epicsShareAPI recGblFwdLink(void *precord)
{
    dbCommon *pdbc = precord;

    dbScanFwdLink(&pdbc->flnk);
    /*Handle dbPutFieldNotify record completions*/
    if(pdbc->ppn) dbNotifyCompletion(pdbc);
    if(pdbc->rpro) {
	/*If anyone requested reprocessing do it*/
	pdbc->rpro = FALSE;
	scanOnce(pdbc);
    }
    /*In case putField caused put we are all done */
    pdbc->putf = FALSE;
}

void epicsShareAPI recGblGetTimeStamp(void* prec)
{
    dbCommon* pr = (dbCommon*)prec;
    struct link *plink = &pr->tsel;
 
    if(plink->type!=CONSTANT) {
        struct pv_link *ppv_link = &plink->value.pv_link;

        if(ppv_link->pvlMask&pvlOptTSELisTime) {
            long status = dbGetTimeStamp(plink,&pr->time);
            if(status)
                errlogPrintf("%s recGblGetTimeStamp dbGetTimeStamp failed\n",
                    pr->name);
            return;
        }
        dbGetLink(&(pr->tsel), DBR_SHORT,&(pr->tse),0,0);
    }
    if(pr->tse!=epicsTimeEventDeviceTime) {
        int status;
        status = epicsTimeGetEvent(&pr->time,pr->tse);
        if(status) errlogPrintf("%s recGblGetTimeStamp failed\n",pr->name);
    }
}

void epicsShareAPI recGblTSELwasModified(struct link *plink)
{
    struct pv_link *ppv_link = &plink->value.pv_link;
    char *pfieldname;

    if(plink->type!=PV_LINK) {
        errlogPrintf("recGblTSELwasModified called for non PV_LINK\n");
        return;
    }
    /*If pvname ends in .TIME then just ask for VAL*/
    /*Note that the VAL value will not be used*/
    pfieldname = strstr(ppv_link->pvname,".TIME");
    if(pfieldname) {
        strcpy(pfieldname,".VAL");
        ppv_link->pvlMask |= pvlOptTSELisTime;
    }
}

static void getMaxRangeValues(field_type,pupper_limit,plower_limit)
    short           field_type;
    double          *pupper_limit;
    double          *plower_limit;
{
    switch(field_type){
    case(DBF_CHAR):
         *pupper_limit = -128.0;
         *plower_limit = 127.0;
         break;
    case(DBF_UCHAR):
         *pupper_limit = 255.0;
         *plower_limit = 0.0;
         break;
    case(DBF_SHORT):
         *pupper_limit = (double)SHRT_MAX;
         *plower_limit = (double)SHRT_MIN;
         break;
    case(DBF_ENUM):
    case(DBF_USHORT):
         *pupper_limit = (double)USHRT_MAX;
         *plower_limit = (double)0;
         break;
    case(DBF_LONG):
	/* long did not work using cast to double */
         *pupper_limit = 2147483647.;
         *plower_limit = -2147483648.;
         break;
    case(DBF_ULONG):
         *pupper_limit = (double)ULONG_MAX;
         *plower_limit = (double)0;
         break;
    case(DBF_FLOAT):
         *pupper_limit = (double)1e+30;
         *plower_limit = (double)-1e30;
         break;
    case(DBF_DOUBLE):
         *pupper_limit = (double)1e+30;
         *plower_limit = (double)-1e30;
         break;
    }
    return;
}
