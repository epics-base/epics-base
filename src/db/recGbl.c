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

#include	<vxWorks.h>
#include	<limits.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<strLib.h>

#include	<dbDefs.h>
#include	<dbBase.h>
#include	<epicsPrint.h>
#include	<dbEvent.h>
#include	<dbAccess.h>
#include	<dbConvert.h>
#include	<dbScan.h>
#include	<devSup.h>
#include	<recGbl.h>
#include	<dbCommon.h>
#include	<drvTS.h>


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
	dbFldDes	*pdbFldDes=(dbFldDes *)(paddr->pfldDes);
	dbRecDes	*pdbRecDes = pdbFldDes->pdbRecDes;

	buffer[0]=0;
	strcat(buffer,"Record Support Routine (");
	if(psupport_name)
		strcat(buffer,psupport_name);
	else
		strcat(buffer,"Unknown");
	strcat(buffer,") not available.\nRecord Type is ");
	strcat(buffer,pdbRecDes->name);
	if(paddr) { /* print process variable name */
		precord=(struct dbCommon *)(paddr->precord);
		strcat(buffer,", PV is ");
		strcat(buffer,precord->name);
		strcat(buffer,".");
		strcat(buffer,pdbFldDes->name);
		strcat(buffer,"  ");
	}
	if(pcaller_name) {
		strcat(buffer,"\nerror detected in routine: ");
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
         break;
    case(DBF_DOUBLE):
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
	sscanf(plink->value.constantStr,"%d",(long *)pdest);
	break;
    case DBF_ULONG : 
	sscanf(plink->value.constantStr,"%u",(unsigned long *)pdest);
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

long recGblGetLinkValue(struct link *plink,void *pdbc,short dbrType,
	void *pdest,long *poptions,long	*pnRequest)
{
	struct dbCommon	*precord = pdbc;
	long		status=0;
	unsigned char   pact;

	pact = precord->pact;
	precord->pact = TRUE;
	switch (plink->type){
		case(CONSTANT):
			*pnRequest = 0;
			break;
		case(DB_LINK):
			status=dbGetLink(&(plink->value.db_link),
				precord,dbrType,pdest,poptions,pnRequest);
			if(status)
				recGblSetSevr(precord,LINK_ALARM,INVALID_ALARM);
			break;
		case(CA_LINK):
			status=dbCaGetLink(plink);
			if(status)
				recGblSetSevr(precord,LINK_ALARM,INVALID_ALARM);
			break;
		default:
			status=-1;
			recGblSetSevr(precord,SOFT_ALARM,INVALID_ALARM);
	}
	precord->pact = pact;
	return(status);
}

long recGblPutLinkValue(struct link *plink,void *pdbc,short dbrType,
	void *psource,long *pnRequest)
{
	struct dbCommon *precord = pdbc;
	long		options=0;
	long		status=0;
	unsigned char   pact;

	pact = precord->pact;
	precord->pact = TRUE;
	switch (plink->type){
		case(CONSTANT):
			break;
		case(DB_LINK):
			status=dbPutLink(&(plink->value.db_link),
				precord,dbrType,psource,*pnRequest);
			if(status)
				recGblSetSevr(precord,LINK_ALARM,INVALID_ALARM);
			break;
		case(CA_LINK):
			status = dbCaPutLink(plink, &options, pnRequest);
			if(status)
				recGblSetSevr(precord,LINK_ALARM,INVALID_ALARM);
			break;
		default:
			status=-1;
			recGblSetSevr(precord,SOFT_ALARM,INVALID_ALARM);
	}
	precord->pact = pact;
	return(status);
}

unsigned short recGblResetAlarms(void *precord)
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

void recGblFwdLink(void *precord)
{
    struct dbCommon *pdbc = precord;

    if(pdbc->flnk.type==DB_LINK ) {
	struct dbAddr	*paddr = pdbc->flnk.value.db_link.pdbAddr;
	dbScanPassive(precord,paddr->precord);
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
    long nRequest=1;
    long options=0;
 
    if(pr->tsel.type!=CONSTANT)
    {
        recGblGetLinkValue(&(pr->tsel),(void*)pr,
            DBR_SHORT,&(pr->tse),&options,&nRequest);
 
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

/*  Fast link initialization routines  */
 
/*
 *  String if bad database request type chosen
 */
static char *bad_in_req_type = "recGblInitFastInLink:  Bad database request type";
static char *bad_out_req_type = "recGblInitFastInLink:  Bad database request type";
 
/*
 *  Initialize fast input links.
 */
long recGblInitFastInLink(
     struct link *plink,
     void *precord,
     short dbrType,
     char *fld_name)
{
  long status = 0;
  struct db_link *pdb_link = &(plink->value.db_link);
  struct dbAddr *pdb_addr = (struct dbAddr *) (pdb_link->pdbAddr);
  long (*cvt_func)();
 
 /*
  *  Check for CA_LINK
  */
  if (plink->type == PV_LINK) {
      status = dbCaAddInlink(plink, (struct dbCommon *) precord, fld_name);
      return(status);
  }
 
 /*
  *  Return if not database link (A constant link, for example)
  */
  if (plink->type != DB_LINK)
      return(0);
 
 /*
  *  Check for legal conversion range...
  */
  if ((pdb_addr->field_type < DBF_STRING) ||
      (pdb_addr->field_type > DBF_DEVICE) ||
      (       dbrType       < DBR_STRING) ||
      (       dbrType       > DBR_ENUM)) {
 
      pdb_link->conversion = cvt_dummy;
      recGblDbaddrError(S_db_badDbrtype, pdb_addr, bad_in_req_type);
      return(S_db_badDbrtype);
  }
 
 /*
  *  Lookup conversion function
  */
  cvt_func = dbFastGetConvertRoutine[pdb_addr->field_type][dbrType];
 
  if (cvt_func == NULL) {
      pdb_link->conversion = cvt_dummy;
      recGblDbaddrError(S_db_badDbrtype, pdb_addr, bad_in_req_type);
      return(S_db_badDbrtype);
  }
 
 /*
  *  Put function it into conversion field (Run Time Link)
  */
  pdb_link->conversion = cvt_func;
 
  return(0);
}
 
/*
 *  Initialize fast output links.
 */
long recGblInitFastOutLink(
     struct link *plink,
     void *precord,
     short dbrType,
     char *fld_name)
{
  long status = 0;
  struct db_link *pdb_link = &(plink->value.db_link);
  struct dbAddr *pdb_addr = (struct dbAddr *) (pdb_link->pdbAddr);
  long (*cvt_func)();
 
 /*
  *  Check for CA_LINK
  */
  if (plink->type == PV_LINK) {
      status = dbCaAddOutlink(plink, (struct dbCommon *) precord, fld_name);
      return(status);
  }
 
 /*
  *  Return if not database link (A constant link, for example)
  */
  if (plink->type != DB_LINK)
      return(0);
 
 /*
  *  Check for legal conversion range...
  */
  if ((pdb_addr->field_type < DBF_STRING) ||
      (pdb_addr->field_type > DBF_DEVICE) ||
      (       dbrType       < DBR_STRING) ||
      (       dbrType       > DBR_ENUM)) {
 
      pdb_link->conversion = cvt_dummy;
      recGblDbaddrError(S_db_badDbrtype, pdb_addr, bad_out_req_type);
      return(S_db_badDbrtype);
  }
 /*
  *  Lookup conversion function
  */
  cvt_func = dbFastPutConvertRoutine[dbrType][pdb_addr->field_type];
 
  if (cvt_func == NULL) {
      pdb_link->conversion = cvt_dummy;
      recGblDbaddrError(S_db_badDbrtype, pdb_addr, bad_out_req_type);
      return(S_db_badDbrtype);
  }
 
 /*
  *  Put function it into conversion field (Run Time Link)
  */
  pdb_link->conversion = cvt_func;
 
  return(0);
}

