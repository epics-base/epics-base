/* recGbl.c - Global record processing routines */
/* base/src/db $Id$ */

/*
 *      Author:          Marty Kraimer
 *      Date:            11-7-90
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
 * .01  11-16-91        jba     Added recGblGetGraphicDouble, recGblGetControlDouble
 * .02  02-28-92        jba     ANSI C changes
 * .03	05-19-92	mrk	Changes for internal database structure changes
 * .04  07-16-92        jba     changes made to remove compile warning msgs
 * .05  07-21-92        jba     Added recGblGetAlarmDouble
 * .06  08-07-92        jba     Added recGblGetLinkValue, recGblPutLinkValue
 * .07  08-07-92        jba     Added RTN_SUCCESS check for status
 * .08  09-15-92        jba     changed error parm in recGblRecordError calls
 * .09  09-17-92        jba     ANSI C changes
 * .10  01-27-93        jba     set pact to true during calls to dbPutLink
 * .11  03-21-94        mcn     Added fast link routines
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "dbDefs.h"
#include "osiThread.h"
#include "tsStamp.h"
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
#include "dbAccess.h"
#include "dbNotify.h"
#include "dbCa.h"
#include "dbEvent.h"
#include "dbScan.h"
#include "recGbl.h"


/* local routines */
static void getMaxRangeValues();



void epicsShareAPI recGblDbaddrError(
    long status,struct dbAddr *paddr,char *pcaller_name)
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

void epicsShareAPI recGblRecordError(long status,void *pdbc,char *pcaller_name)
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

void epicsShareAPI recGblRecSupError(
    long status,struct dbAddr *paddr,char *pcaller_name,
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

	sscanf(plink->value.constantStr,"%hd",&value);
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
	sscanf(plink->value.constantStr,"%hd",(short *)pdest);
	break;
    case DBF_USHORT : 
    case DBF_ENUM : 
    case DBF_MENU : 
    case DBF_DEVICE : 
	sscanf(plink->value.constantStr,"%hu",(unsigned short *)pdest);
	break;
    case DBF_LONG : 
	sscanf(plink->value.constantStr,"%ld",(long *)pdest);
	break;
    case DBF_ULONG : 
	sscanf(plink->value.constantStr,"%lu",(unsigned long *)pdest);
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

unsigned short epicsShareAPI recGblResetAlarms(void *precord)
{
    struct dbCommon *pdbc = precord;
    unsigned short mask,stat,sevr,nsta,nsev,ackt,acks;

    mask = 0;
    stat=pdbc->stat; sevr=pdbc->sevr;
    nsta=pdbc->nsta; nsev=pdbc->nsev;
    pdbc->stat=nsta; pdbc->sevr=nsev;
    pdbc->nsta=0; pdbc->nsev=0;
    /* alarm condition changed this scan?*/
    if (stat!=nsta) {
	mask = DBE_ALARM;
	db_post_events(pdbc,&pdbc->stat,DBE_VALUE);
    }
    if (sevr!=nsev) {
	mask = DBE_ALARM;
	db_post_events(pdbc,&pdbc->sevr,DBE_VALUE);
    }
    if(sevr!=nsev || stat!=nsta) {
	ackt = pdbc->ackt; acks = pdbc->acks;
	if(!ackt || nsev>=acks){
	    pdbc->acks=nsev;
	    db_post_events(pdbc,&pdbc->acks,DBE_VALUE);
	}
    }
    return(mask);
}

void epicsShareAPI recGblFwdLink(void *precord)
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


void epicsShareAPI recGblGetTimeStamp(void* prec)
{
    struct dbCommon* pr = (struct dbCommon*)prec;
    int status;
 
    if(pr->tsel.type!=CONSTANT)
    {
        dbGetLink(&(pr->tsel), DBR_SHORT,&(pr->tse),0,0);
        status = tsStampGetEvent(&pr->time,(unsigned)pr->tse);
    }
    else
        status = tsStampGetEvent(&pr->time,(unsigned)pr->tse);
    if(status) errlogPrintf("%s recGblGetTimeStamp failed\n",pr->name);
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
