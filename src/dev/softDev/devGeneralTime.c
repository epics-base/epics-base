/***************************************************************************
 *   File:		devGeneralTime.c
 *   Author:		Sheng Peng
 *   Institution:	Oak Ridge National Laboratory / SNS Project
 *   Date:		07/2004
 *   Version:		1.2
 *
 *   EPICS device layer support for general EPICS timestamp support
 *
 *   Integrated into base by Peter Denison, Diamond Light Source
 ****************************************************************************/

/* Include header files	*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <dbCommon.h>
#include <dbDefs.h>
#include <dbScan.h>
#include <dbAccess.h>
#include <recSup.h>
#include <recGbl.h>
#include <devSup.h>
#include <devLib.h>
#include <drvSup.h>

#include <alarm.h>
#include <epicsExport.h>

#include <aiRecord.h>
#include <boRecord.h>
#include <longinRecord.h>
#include <stringinRecord.h>

#include	"epicsGeneralTime.h"

int     GENERALTIME_DEV_DEBUG=0;

/*	define function flags	*/
typedef enum {
	GTIM_AI_CURRENT,
	GTIM_BO_RSTERRCNT,
	GTIM_LI_GETERRCNT,
	GTIM_SI_BESTTCP,
	GTIM_SI_BESTTEP
}	GTIMFUNC;


/*	define parameter check for convinence */
#define CHECK_AIPARM(PARM,VAL)\
	if (!strncmp(pai->inp.value.instio.string,(PARM),strlen((PARM)))) {\
		pai->dpvt=(void *)VAL;\
		return (0);\
	}
#define CHECK_BOPARM(PARM,VAL)\
	if (!strncmp(pbo->out.value.instio.string,(PARM),strlen((PARM)))) {\
		pbo->dpvt=(void *)VAL;\
		paramOK=1;\
	}
#define CHECK_LIPARM(PARM,VAL)\
	if (!strncmp(pli->inp.value.instio.string,(PARM),strlen((PARM)))) {\
		pli->dpvt=(void *)VAL;\
		return (0);\
	}
#define CHECK_SIPARM(PARM,VAL)\
	if (!strncmp(psi->inp.value.instio.string,(PARM),strlen((PARM)))) {\
		psi->dpvt=(void *)VAL;\
		return (0);\
	}

/* function prototypes */
static long init(int pass);

static long init_ai(struct aiRecord *pai);
static long read_ai(struct aiRecord *pai);

static long init_bo(struct boRecord *pbo);
static long write_bo(struct boRecord *pbo);

static long init_li(struct longinRecord *pli);
static long read_li(struct longinRecord *pli);

static long init_si(struct stringinRecord *psi);
static long read_si(struct stringinRecord *psi);

/* global struct for devSup */
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	DEVSUPFUN	special_linconv;} GTIM_DEV_SUP_SET;

GTIM_DEV_SUP_SET devAiGeneralTime=   {6, NULL, init, init_ai,  NULL, read_ai,  NULL};
GTIM_DEV_SUP_SET devBoGeneralTime=   {6, NULL, NULL, init_bo,  NULL, write_bo, NULL};
GTIM_DEV_SUP_SET devLiGeneralTime=   {6, NULL, NULL, init_li,  NULL, read_li,  NULL};
GTIM_DEV_SUP_SET devSiGeneralTime=   {6, NULL, NULL, init_si,  NULL, read_si, NULL};

epicsExportAddress(dset, devAiGeneralTime);
epicsExportAddress(dset, devBoGeneralTime);
epicsExportAddress(dset, devLiGeneralTime);
epicsExportAddress(dset, devSiGeneralTime);

/* function implementation */

static long init(int pass)
{
	if(pass) return 0;
	return 0;
}

static long init_ai( struct aiRecord	*pai)
{
	if (pai->inp.type!=INST_IO)
	{
		recGblRecordError(S_db_badField, (void *)pai,
			"devAiGeneralTime Init_record, Illegal INP");
		pai->pact=TRUE;
		return (S_db_badField);
    	}

	CHECK_AIPARM("TIME",       GTIM_AI_CURRENT)
	/* reach here, bad parm */
	recGblRecordError(S_db_badField, (void *)pai,
			"devAiGeneralTime Init_record, bad parm");
	pai->pact=TRUE;
	return (S_db_badField);
}

static long read_ai(struct aiRecord	*pai)
{
	int status=-1;
	double	temp;

	switch ((int)pai->dpvt)
	{
	case GTIM_AI_CURRENT:
		status=generalTimeGetCurrentDouble(&temp);
		break;
	default:
		return -1;
	}
	if (status==0)
	{
		pai->val=temp;
		pai->udf=FALSE;
		return 2;/******** not convert ****/
	}
	else
	{
		pai->udf=TRUE;
		recGblSetSevr(pai,READ_ALARM,INVALID_ALARM);
		return -1;
	}
}

/*********bo record **********/
static long init_bo(struct boRecord *pbo)
{
	int paramOK=0;

	if (pbo->out.type!=INST_IO)
	{
		recGblRecordError(S_db_badField, (void *)pbo,
			"devBoGeneralTime Init_record, Illegal OUT");
		pbo->pact=TRUE;
		return (S_db_badField);
	}
	CHECK_BOPARM("RSTERRCNT",	GTIM_BO_RSTERRCNT)
	if (!paramOK)
	{
		recGblRecordError(S_db_badField, (void *)pbo,
			"devBoGeneralTime Init_record, bad parm");
		pbo->pact=TRUE;
		return (S_db_badField);
	}

	pbo->mask=0; /** when convert rval from val,keep them same **/

	return 2; /** don't convert rval to val **/
}

static long write_bo(struct boRecord *pbo)
{
	switch ((int)pbo->dpvt)
	{
	case GTIM_BO_RSTERRCNT:
		generalTimeResetErrorCounts();
		break;
	}
	return 0;
}


/*******longin	record *************/
static long init_li(struct longinRecord *pli)
{
	if (pli->inp.type!=INST_IO)
	{
		recGblRecordError(S_db_badField, (void *)pli,
			"devLiGeneralTime Init_record, Illegal INP");
		pli->pact=TRUE;
		return (S_db_badField);
	}
	CHECK_LIPARM("GETERRCNT",       GTIM_LI_GETERRCNT)
	/* reach here, bad parm */
	recGblRecordError(S_db_badField, (void *)pli,
			"devLiGeneralTime Init_record, bad parm");
	pli->pact=TRUE;
	return (S_db_badField);
}

static long read_li(struct longinRecord *pli)
{
	switch ((int)pli->dpvt)
	{
	case GTIM_LI_GETERRCNT:
		pli->val=generalTimeGetErrorCounts();
		break;
	default:
		return -1;
	}
	return 0;
}


/**********	stringin record	**********/
static long init_si(struct stringinRecord *psi)
{
	if (psi->inp.type!=INST_IO)
	{
		recGblRecordError(S_db_badField, (void *)psi,
			"devSiGeneralTime Init_record, Illegal INP");
		psi->pact=TRUE;
		return (S_db_badField);
	}
	CHECK_SIPARM("BESTTCP",   GTIM_SI_BESTTCP)
	CHECK_SIPARM("BESTTEP",   GTIM_SI_BESTTEP)
	/* reach here, bad parm */
	recGblRecordError(S_db_badField, (void *)psi,
			"devSiGeneralTime Init_record, bad parm");
	psi->pact=TRUE;
	return (S_db_badField);
}

static long read_si(struct stringinRecord *psi)
{
	switch ((int)psi->dpvt)
	{
	case GTIM_SI_BESTTCP:
		generalTimeGetBestTcp(psi->val);
		break;
	case GTIM_SI_BESTTEP:
		generalTimeGetBestTep(psi->val);
		break;
	}
	return 0;
}

