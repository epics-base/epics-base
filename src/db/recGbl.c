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
 *
 */

#include	<vxWorks.h>
#include	<limits.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<strLib.h>

#include	"dbDefs.h"
#include	"epicsPrint.h"
#include	"dbBase.h"
#include	"dbEvent.h"
#include	"dbAccess.h"
#include	"dbConvert.h"
#include	"dbScan.h"
#include	"devSup.h"
#include	"recGbl.h"
#include	"dbCommon.h"
#include	"drvTS.h"


/* local routines */
static void getMaxRangeValues();



void recGblDbaddrError(long status,struct dbAddr *paddr,char *pcaller_name)
{
	char		buffer[200];
	struct dbCommon *precord;
	dbFldDes	*pdbFldDes=(dbFldDes *)(paddr->pfldDes);

	buffer[0]=0;
	if(paddr) { /* print process variable name */
		precord=(struct dbCommon *)(paddr->precord);
		strcat(buffer,"PV: ");
		strcat(buffer,precord->name);
		strcat(buffer,".");
		strcat(buffer,pdbFldDes->name);
		strcat(buffer," ");
	}
	if(pcaller_name) {
		strcat(buffer,"error detected in routine: ");
		strcat(buffer,pcaller_name);
	}
	errMessage(status,buffer);
	return;
}

void recGblRecordError(long status,void *pdbc,char *pcaller_name)
{
	struct dbCommon	*precord = pdbc;
	char		buffer[200];

	buffer[0]=0;
	if(precord) { /* print process variable name */
		strcat(buffer,"PV: ");
		strcat(buffer,precord->name);
		strcat(buffer,"  ");
	}
	if(pcaller_name) {
		strcat(buffer,pcaller_name);
	}
	errMessage(status,buffer);
	return;
}

void recGblRecSupError(long status,struct dbAddr *paddr,char *pcaller_name,
	char *psupport_name)
{
	char 		buffer[200];
	struct dbCommon *precord;
	dbFldDes	*pdbFldDes = 0;
	dbRecordType	*pdbRecordType = 0;

	if(paddr) pdbFldDes=(dbFldDes *)(paddr->pfldDes);
	if(pdbFldDes) pdbRecordType = pdbFldDes->pdbRecordType;
	buffer[0]=0;
	strcat(buffer,"Record Support Routine (");
	if(psupport_name)
		strcat(buffer,psupport_name);
	else
		strcat(buffer,"Unknown");
	strcat(buffer,") not available.\n");
	if(pdbRecordType) {
	    strcat(buffer,"Record Type is ");
	    strcat(buffer,pdbRecordType->name);
	    if(paddr) { /* print process variable name */
		precord=(struct dbCommon *)(paddr->precord);
		strcat(buffer,", PV is ");
		strcat(buffer,precord->name);
		strcat(buffer,".");
		strcat(buffer,pdbFldDes->name);
		strcat(buffer,"\n");
	    }
	}
	if(pcaller_name) {
		strcat(buffer,"error detected in routine: ");
		strcat(buffer,pcaller_name);
	}
	errMessage(status,buffer);
	return;
}

void recGblGetPrec(struct dbAddr *paddr,long *precision)
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

void recGblGetGraphicDouble(struct dbAddr *paddr,struct dbr_grDouble *pgd)
{
    dbFldDes               *pdbFldDes=(dbFldDes *)(paddr->pfldDes);

    getMaxRangeValues(pdbFldDes->field_type,&pgd->upper_disp_limit,
	&pgd->lower_disp_limit);

    return;
}

void recGblGetAlarmDouble(struct dbAddr *paddr,struct dbr_alDouble *pad)
{
    pad->upper_alarm_limit = 0;
    pad->upper_warning_limit = 0;
    pad->lower_warning_limit = 0;
    pad->lower_alarm_limit = 0;

    return;
}

void recGblGetControlDouble(struct dbAddr *paddr,struct dbr_ctrlDouble *pcd)
{
    dbFldDes               *pdbFldDes=(dbFldDes *)(paddr->pfldDes);

    getMaxRangeValues(pdbFldDes->field_type,&pcd->upper_ctrl_limit,
	&pcd->lower_ctrl_limit);

    return;
}

int  recGblInitConstantLink(struct link *plink,short dbftype,void *pdest)
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

	sscanf(plink->value.constantStr,"%hi",&value);
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
	sscanf(plink->value.constantStr,"%hi",(unsigned short *)pdest);
	break;
    case DBF_LONG : 
	sscanf(plink->value.constantStr,"%li",(long *)pdest);
	break;
    case DBF_ULONG : 
	sscanf(plink->value.constantStr,"%li",(unsigned long *)pdest);
	break;
    case DBF_FLOAT : 
	sscanf(plink->value.constantStr,"%f",(float *)pdest);
	break;
    case DBF_DOUBLE : 
	sscanf(plink->value.constantStr,"%lf",(double *)pdest);
	break;
    default:
	epicsPrintf("Error in recGblInitConstantLink: Illegal DBF type\n");
	return(FALSE);
    }
    return(TRUE);
}

unsigned short recGblResetAlarms(void *precord)
{
    struct dbCommon *pdbc = precord;
    unsigned short mask,stat,sevr,nsta,nsev,ackt,acks;
    unsigned short stat_mask=0;

    mask = 0;
    stat=pdbc->stat; sevr=pdbc->sevr;
    nsta=pdbc->nsta; nsev=pdbc->nsev;
    pdbc->stat=nsta; pdbc->sevr=nsev;
    pdbc->nsta=0; pdbc->nsev=0;
    /* alarm condition changed this scan?*/
    if (sevr!=nsev) {
        stat_mask = mask = DBE_ALARM;
        db_post_events(pdbc,&pdbc->sevr,DBE_VALUE);
    }
    if(stat!=nsta) {
        stat_mask |= DBE_VALUE;
        mask = DBE_ALARM;
    }
    if(stat_mask)
        db_post_events(pdbc,&pdbc->stat,stat_mask);
    if(sevr!=nsev || stat!=nsta) {
	ackt = pdbc->ackt; acks = pdbc->acks;
	if(!ackt || nsev>=acks){
	    pdbc->acks=nsev;
	    db_post_events(pdbc,&pdbc->acks,DBE_VALUE);
	}
    }
    return(mask);
}

void recGblFwdLink(void *precord)
{
    struct dbCommon *pdbc = precord;
    static short    fwdLinkValue = 1;

    if(pdbc->flnk.type==DB_LINK ) {
	struct dbAddr	*paddr = pdbc->flnk.value.pv_link.pvt;
	dbScanPassive(precord,paddr->precord);
    } else
    if((pdbc->flnk.type==CA_LINK) 
    && (pdbc->flnk.value.pv_link.pvlMask & pvlOptFWD)) {
	dbCaPutLink(&pdbc->flnk,DBR_SHORT,&fwdLinkValue,1);
    }
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


void recGblGetTimeStamp(void* prec)
{
    struct dbCommon* pr = (struct dbCommon*)prec;
 
    if(pr->tsel.type!=CONSTANT)
    {
        dbGetLink(&(pr->tsel), DBR_SHORT,&(pr->tse),0,0);
        TSgetTimeStamp((int)pr->tse,(struct timespec*)&pr->time);
    }
    else
        TSgetTimeStamp((int)pr->tse,(struct timespec*)&pr->time);
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
