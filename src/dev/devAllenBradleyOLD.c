/* devAllenBradley.c */
/* base/src/dev $Id$ */

/* devAllenBradley.c - Device Support Routines for  Allen Bradley*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author: Marty Kraimer
 *      Date:  3/6/91
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  08-27-92	mrk	Combined all Allen Bradley devive support
 * .02  02-08-94	mrk	Issue Hardware Errors BUT prevent Error Message Storms
 * .03  03-30-94        mcn     support for devMbboDirect and devMbbiDirect
 * .04  04-13-94	mrk	Fixed IXE problems
 * 	...
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<dbScan.h>
#include	<link.h>
#include	<module_types.h>
#include	<aiRecord.h>
#include        <aoRecord.h>
#include        <biRecord.h>
#include        <boRecord.h>
#include        <mbbiRecord.h>
#include        <mbbiDirectRecord.h>
#include        <mbboRecord.h>
#include        <mbboDirectRecord.h>
#include        <drvAb.h>

/* Create the dsets*/
static long init_1771Ife();
static long read_1771Ife();
static long linconv_1771Ife();
static long init_1771Ife0to5V();
static long read_1771Ife0to5V();
static long linconv_1771Ife0to5V();
static long init_1771IfeMa();
static long read_1771IfeMa();
static long linconv_1771IfeMa();
static long init_1771IfeSe();
static long read_1771IfeSe();
static long linconv_1771IfeSe();
static long init_1771Il();
static long read_1771Il();
static long linconv_1771Il();
static long init_1771Ixe();
static long read_1771Ixe();
static long linconv_1771Ixe();
static long init_1771IrPlatinum();
static long read_1771IrPlatinum();
static long init_1771IrCopper();
static long read_1771IrCopper();
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;} ABAIDSET;
ABAIDSET devAiAb1771Ife=	{6, NULL, NULL, init_1771Ife,	NULL,
				read_1771Ife, linconv_1771Ife};
ABAIDSET devAiAb1771Ife0to5V=	{6, NULL, NULL, init_1771Ife0to5V,NULL,
				read_1771Ife0to5V, linconv_1771Ife0to5V};
ABAIDSET devAiAb1771IfeMa=	{6, NULL, NULL, init_1771IfeMa,NULL,
				read_1771IfeMa, linconv_1771IfeMa};
ABAIDSET devAiAb1771IfeSe=	{6, NULL, NULL, init_1771IfeSe, NULL,
				read_1771IfeSe, linconv_1771IfeSe};
ABAIDSET devAiAb1771Il=		{6, NULL, NULL, init_1771Il, NULL,
				read_1771Il, linconv_1771Il};
ABAIDSET devAiAb1771Ixe=	{6, NULL, NULL, init_1771Ixe, NULL,
				read_1771Ixe, linconv_1771Ixe};
ABAIDSET devAiAb1771IrPlatinum=	{6, NULL, NULL, init_1771IrPlatinum, NULL,
				read_1771IrPlatinum, NULL};
ABAIDSET devAiAb1771IrCopper=	{6, NULL, NULL, init_1771IrCopper, NULL,
				read_1771IrCopper, NULL};

static long init_1771Ofe();
static int read_1771Ofe();
static long write_1771Ofe();
static long linconv_1771Ofe();
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;} ABAODSET;
ABAIDSET devAoAb1771Ofe={ 6, NULL, NULL, init_1771Ofe, NULL,
			write_1771Ofe, linconv_1771Ofe};

static long init_bi();
static long ioinfo_bi();
static long read_bi();
static long init_bi16();
static long ioinfo_bi16();
static long read_bi16();
static long init_mbbi();
static long init_mbbiDirect();
static long ioinfo_mbbi();
static long ioinfo_mbbiDirect();
static long read_mbbi();
static long read_mbbiDirect();
static long init_mbbi16();
static long init_mbbiDirect16();
static long ioinfo_mbbi16();
static long ioinfo_mbbiDirect16();
static long read_mbbi16();
static long read_mbbiDirect16();
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;} ABBIDSET;

ABBIDSET devBiAb={ 5, NULL, NULL, init_bi, ioinfo_bi, read_bi};
ABBIDSET devBiAb16={ 5, NULL, NULL, init_bi16, ioinfo_bi16, read_bi16};
ABBIDSET devMbbiAb={ 5, NULL, NULL, init_mbbi, ioinfo_mbbi, read_mbbi};
ABBIDSET devMbbiDirectAb={ 5, NULL, NULL, init_mbbiDirect, ioinfo_mbbiDirect, read_mbbiDirect};
ABBIDSET devMbbiAb16={ 5, NULL, NULL, init_mbbi16, ioinfo_mbbi16, read_mbbi16};
ABBIDSET devMbbiDirectAb16={ 5, NULL, NULL, init_mbbiDirect16, ioinfo_mbbiDirect16, read_mbbiDirect16};

static long init_bo();
static long write_bo();
static long init_bo16();
static long write_bo16();
static long init_mbbo();
static long init_mbboDirect();
static long write_mbbo();
static long write_mbboDirect();
static long init_mbbo16();
static long init_mbboDirect16();
static long write_mbbo16();
static long write_mbboDirect16();
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;} ABBODSET;

ABBODSET devBoAb={ 5, NULL, NULL, init_bo, NULL, write_bo};
ABBODSET devBoAb16={ 5, NULL, NULL, init_bo16, NULL, write_bo16};
ABBODSET devMbboAb={ 5, NULL, NULL, init_mbbo, NULL, write_mbbo};
ABBODSET devMbboDirectAb={ 5, NULL, NULL, init_mbboDirect, NULL, write_mbboDirect};
ABBODSET devMbboAb16={ 5, NULL, NULL, init_mbbo16, NULL, write_mbbo16};
ABBODSET devMbboDirectAb16={ 5, NULL, NULL, init_mbboDirect16, NULL, write_mbboDirect16};


static long init_1771Ife(struct aiRecord	*pai)
{
    unsigned short value;
    struct abio *pabio;

    /* ai.inp must be an AB_IO */
    switch (pai->inp.type) {
    case (AB_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiAb1771Ife (init_record) Illegal INP field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;

    /* call driver so that it configures card */
    /* The driver returns error for first call for a card. Ignore it. */
    pabio = (struct abio *)&(pai->inp.value);
    ab_aidriver(AB1771IFE,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,pai->linr);
    return(0);
}

static long read_1771Ife(struct aiRecord	*pai)
{
	struct abio *pabio;
	int	    status;
	unsigned short	    value;

	
	pabio = (struct abio *)&(pai->inp.value);
	status = ab_aidriver(AB1771IFE,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,pai->linr);
	if(status==0 || status==-2) pai->rval = value;
	if(status==-1) {
                if(recGblSetSevr(pai,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pai->stat!=READ_ALARM || pai->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pai,"ab_aidriver Error");
		status=2; /*dont convert*/
	}else if(status==-2) {
		status=0;
                recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
	}
	return(status);
}


static long linconv_1771Ife(struct aiRecord	*pai, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;
    return(0);
}

static long init_1771Ife0to5V(struct aiRecord	*pai)
{
    unsigned short value;
    struct abio *pabio;

    /* ai.inp must be an AB_IO */
    switch (pai->inp.type) {
    case (AB_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiAb1771Ife0to5V (init_record) Illegal INP field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;

    /* call driver so that it configures card */
    /* The driver returns error for first call for a card. Ignore it. */
    pabio = (struct abio *)&(pai->inp.value);
    ab_aidriver(AB1771IFE_0to5V,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,pai->linr);
    return(0);
}

static long read_1771Ife0to5V(struct aiRecord	*pai)
{
	struct abio *pabio;
	int	    status;
	unsigned short value;

	
	pabio = (struct abio *)&(pai->inp.value);
	status=ab_aidriver(AB1771IFE_0to5V,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,pai->linr);
	if(status==0 || status==-2) pai->rval = value;
        if(status==-1) {
                if(recGblSetSevr(pai,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pai->stat!=READ_ALARM || pai->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pai,"ab_aidriver Error");
		status=2; /*dont convert*/
        }else if(status==-2) {
                status=0;
                recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
        }
	return(status);
}


static long linconv_1771Ife0to5V(struct aiRecord *pai, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;
    return(0);
}

static long init_1771IfeMa(struct aiRecord	*pai)
{
    unsigned short value;
    struct abio *pabio;

    /* ai.inp must be an AB_IO */
    switch (pai->inp.type) {
    case (AB_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiAb1771IfeMa (init_record) Illegal INP field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;

    /* call driver so that it configures card */
    /* The driver returns error for first call for a card. Ignore it. */
    pabio = (struct abio *)&(pai->inp.value);
    ab_aidriver(AB1771IFE_4to20MA,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,pai->linr);
    return(0);
}

static long read_1771IfeMa(struct aiRecord *pai)
{
	struct abio *pabio;
	int	    status;
	unsigned short value;

	
	pabio = (struct abio *)&(pai->inp.value);
	status=ab_aidriver(AB1771IFE_4to20MA,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,pai->linr);
	if(status==0 || status==-2) pai->rval = value;
        if(status==-1) {
                if(recGblSetSevr(pai,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pai->stat!=READ_ALARM || pai->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pai,"ab_aidriver Error");
		status=2; /*dont convert*/
        }else if(status==-2) {
                status=0;
                recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
        }
	return(status);
}


static long linconv_1771IfeMa(struct aiRecord *pai, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;
    return(0);
}

static long init_1771IfeSe(struct aiRecord *pai)
{
    unsigned short value;
    struct abio *pabio;

    /* ai.inp must be an AB_IO */
    switch (pai->inp.type) {
    case (AB_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiAb1771IfeSe (init_record) Illegal INP field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;

    /* call driver so that it configures card */
    /* The driver returns error for first call for a card. Ignore it. */
    pabio = (struct abio *)&(pai->inp.value);
    ab_aidriver(AB1771IFE_SE,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,pai->linr);
    return(0);
}

static long read_1771IfeSe(struct aiRecord *pai)
{
	struct abio *pabio;
	int	    status;
	unsigned short value;

	
	pabio = (struct abio *)&(pai->inp.value);
	status = ab_aidriver(AB1771IFE_SE,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,pai->linr);
        if(status==0 || status==-2) pai->rval = value;
        if(status==-1) {
                if(recGblSetSevr(pai,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pai->stat!=READ_ALARM || pai->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pai,"ab_aidriver Error");
		status = 2; /*don`t convert*/
        }else if(status==-2) {
                status=0;
                recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
        }
	return(status);
}


static long linconv_1771IfeSe(struct aiRecord *pai, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;
    return(0);
}

static long init_1771Il(struct aiRecord	*pai)
{
    unsigned short value;
    struct abio *pabio;

    /* ai.inp must be an AB_IO */
    switch (pai->inp.type) {
    case (AB_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiAb1771Il (init_record) Illegal INP field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;

    /* call driver so that it configures card */
    /* The driver returns error for first call for a card. Ignore it. */
    pabio = (struct abio *)&(pai->inp.value);
    ab_aidriver(AB1771IL,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,pai->linr);
    return(0);
}

static long read_1771Il(struct aiRecord	*pai)
{
	struct abio *pabio;
        int         status;
	unsigned short value;

	
	pabio = (struct abio *)&(pai->inp.value);
	status = ab_aidriver(AB1771IL,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,pai->linr);
        if(status==0 || status==-2) pai->rval = value;
        if(status==-1) {
                if(recGblSetSevr(pai,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pai->stat!=READ_ALARM || pai->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pai,"ab_aidriver Error");
		status = 2; /*don't convert*/
        }else if(status==-2) {
                status=0;
                recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
        }
	return(status);
}

static long linconv_1771Il(struct aiRecord *pai, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/4095.0;
    return(0);
}

static long init_1771Ixe(struct aiRecord *pai)
{
    short value;
    struct abio *pabio;
    short linr;

    /* ai.inp must be an AB_IO */
    switch (pai->inp.type) {
    case (AB_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiAb1771Ixe (init_record) Illegal INP field");
	return(S_db_badField);
    }


    /* If conversion type is 2,...,13 the Ixe performs the conversion*/
    /* In this case lets set linr = no conversion and use lbrk to tell*/
    /* allen bradley driver the correct conversion.                   */
    /* THIS SHOULD BE CHANGED 						*/
    linr=pai->linr;
    if(linr==0 || linr>13 ) {
	pai->eslo=0.0;
	pai->lbrk=0;
    }
    else if (linr==1) {
        /* set linear conversion slope*/
        pai->eslo = (pai->eguf -pai->egul)/20000.0;
    }
    else if( linr>=2 && linr<=13 ) {
	pai->linr=0; /* make sure record processing routine does not convert*/
	pai->lbrk=linr;
    }
	
    /* call driver so that it configures card */
    /* The driver returns error for first call for a card. Ignore it. */
    pabio = (struct abio *)&(pai->inp.value);
    linr=pai->linr;
    if(linr==0) linr=pai->lbrk;
    ab_aidriver(AB1771IXE,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,linr);
    return(0);
}

static long read_1771Ixe(struct aiRecord *pai)
{
	short linr;
	struct abio *pabio;
	long status;
	short  value;

	
	linr=pai->linr;
	if(linr==0) linr=pai->lbrk;
	else if(linr>13) linr = 0; /*use breakpoint table*/
	pabio = (struct abio *)&(pai->inp.value);
	status=ab_aidriver(AB1771IXE,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,linr);
	if(status==-2) {
                recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
		status = 0;
	}
	if(status==0) {
		if(linr<=1)pai->rval = value + 10000;
		else {
			pai->val = value;
			pai->udf = FALSE;
			status=2; /*don't convert*/
		}
        } else if(status==-1) {
                if(recGblSetSevr(pai,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pai->stat!=READ_ALARM || pai->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pai,"ab_aidriver Error");
		status = 2; /*don't convert*/
        }
	return(status);
}

static long linconv_1771Ixe(struct aiRecord *pai, int after)
{
    short lbrk = pai->lbrk;

    if(lbrk>=2 && lbrk<=13) {
	recGblRecordError(S_db_badField,(void *)pai,"DSET: special_linconv");
	return(S_db_badField);
    }
    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/20000.0;
    return(0);
}

static long init_1771IrPlatinum(struct aiRecord *pai)
{
    short value;
    struct abio *pabio;
    short conversion;

    /* ai.inp must be an AB_IO */
    switch (pai->inp.type) {
    case (AB_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiAb1771IrPlatinum (init_record) Illegal INP field");
	return(S_db_badField);
    }
    pabio = (struct abio *)&(pai->inp.value);
    if(pabio->parm[0]=='C') {
	conversion=IR_degC;
	strcpy(pai->egu,"degC");
    } else if(pabio->parm[0]=='O') {
	conversion = IR_Ohms;
	strcpy(pai->egu,"ohms");
    } else {
	conversion = 0;
	strcpy(pai->egu,"degF");
    }
    pai->linr = 0;
    /* call driver so that it configures card */
    /* The driver returns error for first call for a card. Ignore it. */
    ab_aidriver(AB1771IrPlatinum,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,conversion);
    return(0);
}

static long read_1771IrPlatinum(struct aiRecord *pai)
{
	struct abio *pabio;
	long status;
	short  value;
	short conversion;

	
	pabio = (struct abio *)&(pai->inp.value);
	if(pabio->parm[0]=='C') conversion=IR_degC;
	else if(pabio->parm[0]=='O') conversion = IR_Ohms;
	else conversion = 0;
	status=ab_aidriver(AB1771IrPlatinum,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,conversion);
	if(status==-2) {
                recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
		status = 0;
	}
	if(status==0) {
		if(conversion==IR_Ohms) pai->val = ((double)value)/100.0;
		else  pai->val = ((double)value)/10.0;
		pai->udf = FALSE;
		status=2; /*don't convert*/
        } else if(status==-1) {
                if(recGblSetSevr(pai,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pai->stat!=READ_ALARM || pai->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pai,"ab_aidriver Error");
		status = 2; /*don't convert*/
        }
	return(status);
}

static long init_1771IrCopper(struct aiRecord *pai)
{
    short value;
    struct abio *pabio;
    short conversion;

    /* ai.inp must be an AB_IO */
    switch (pai->inp.type) {
    case (AB_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiAb1771IrCopper (init_record) Illegal INP field");
	return(S_db_badField);
    }
    pabio = (struct abio *)&(pai->inp.value);
    if(pabio->parm[0]=='C') {
	conversion=IR_degC;
	strcpy(pai->egu,"degC");
    } else if(pabio->parm[0]=='O') {
	conversion = IR_Ohms;
	strcpy(pai->egu,"ohms");
    } else {
	conversion = 0;
	strcpy(pai->egu,"degF");
    }
    pai->linr = 0;
    /* call driver so that it configures card */
    /* The driver returns error for first call for a card. Ignore it. */
    ab_aidriver(AB1771IrCopper,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,conversion);
    return(0);
}

static long read_1771IrCopper(struct aiRecord *pai)
{
	struct abio *pabio;
	long status;
	short  value;
	short conversion;

	
	pabio = (struct abio *)&(pai->inp.value);
	if(pabio->parm[0]=='C') conversion=IR_degC;
	else if(pabio->parm[0]=='O') conversion = IR_Ohms;
	else conversion = 0;
	status=ab_aidriver(AB1771IrCopper,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,&value,conversion);
	if(status==-2) {
                recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
		status = 0;
	}
	if(status==0) {
		if(conversion==IR_Ohms) pai->val = ((double)value)/100.0;
		else  pai->val = ((double)value)/10.0;
		pai->udf = FALSE;
		status=2; /*don't convert*/
        } else if(status==-1) {
                if(recGblSetSevr(pai,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pai->stat!=READ_ALARM || pai->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pai,"ab_aidriver Error");
		status = 2; /*don't convert*/
        }
	return(status);
}

static long init_1771Ofe(struct aoRecord *pao)
{

    /* ao.out must be an AB_IO */
    switch (pao->out.type) {
    case (AB_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pao,
		"devAoAb1771Ofe (init_record) Illegal OUT field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pao->eslo = (pao->eguf -pao->egul)/4095.0;

    /* call driver so that it configures card */
    if (read_1771Ofe(pao)==0) return(0);
    else return(2);
    return(0);
}

static long write_1771Ofe(struct aoRecord *pao)
{
	struct abio *pabio;
	int	    status;
	int	    value;
	
	pabio = (struct abio *)&(pao->out.value);
	value = pao->rval;
	status = ab_aodriver(AB1771OFE,pabio->link,pabio->adapter,
	   pabio->card,pabio->signal,pabio->plc_flag,value);
	if(status==-1) {
                if(recGblSetSevr(pao,WRITE_ALARM,INVALID_ALARM) && errVerbose
		&& (pao->stat!=WRITE_ALARM || pao->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pao,"ab_aodriver Error");
	}else if(status==-2) {
		status=0;
                recGblSetSevr(pao,HW_LIMIT_ALARM,INVALID_ALARM);
	}
	return(0);
}


static long linconv_1771Ofe(struct aoRecord *pao, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pao->eslo = (pao->eguf -pao->egul)/4095.0;
    return(0);
}

static int read_1771Ofe(pao)
struct aoRecord      *pao;
{
	unsigned short          value;
	struct abio    		*pabio = &pao->out.value.abio;
	short          		failed;

	/* get the value from the ao driver */
	/* if this is the first channel on the card being read, */
	/* the block transfer from the ever so wonderful, Allen */
	/* Bradley IO, will not be ready. So we wait. The read  */
	/* should only fail when the first channel on the card  */
	/* is accessed. It typically fails only once.           */
	failed = 0;
	while ((ab_aoread(AB1771OFE,pabio->link,pabio->adapter,
	    pabio->card,pabio->signal,pabio->plc_flag,&value) < 0)
	    && (failed < 3)){
		failed++;
		taskDelay(10);
	}
	if (failed >= 3) {
                recGblSetSevr(pao,READ_ALARM,INVALID_ALARM);
		return(-1);
	}
	pao->rval = value;
	return(0);
}

static long init_bi(struct biRecord *pbi)
{
    struct abio *pabio;

    /* bi.inp must be an AB_IO */
    switch (pbi->inp.type) {
    case (AB_IO) :
	pabio = (struct abio *)&(pbi->inp.value);
	pbi->mask=1;
	pbi->mask <<= pabio->signal;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pbi,
		"devBiAb (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long ioinfo_bi(
    int			cmd,
    struct biRecord	*pbi,
    IOSCANPVT		*ppvt)
{
    struct abio *pabio;
    long        value;

    pabio = (struct abio *)&(pbi->inp.value);
    ab_bi_getioscanpvt(pabio->link,pabio->adapter,pabio->card,ppvt);
    /*call ab_bidriver so that it knows it has a binary input*/
    if(cmd==0) {
	(void) ab_bidriver(ABBI_08_BIT,pabio->link,pabio->adapter,
		pabio->card,pabio->plc_flag,pbi->mask,&value);
    }
    return(0);
}

static long read_bi(struct biRecord *pbi)
{
	struct abio *pabio;
	int	    status;
	long	    value;

	
	pabio = (struct abio *)&(pbi->inp.value);
	status = ab_bidriver(ABBI_08_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pbi->mask,&value);
	if(status==0) {
		pbi->rval = value;
		return(0);
	} else {
                if(recGblSetSevr(pbi,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pbi->stat!=READ_ALARM || pbi->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pbi,"ab_bidriver Error");
		return(2);
	}
}

static long init_bi16(struct biRecord *pbi)
{
    struct abio *pabio;

    /* bi.inp must be an AB_IO */
    switch (pbi->inp.type) {
    case (AB_IO) :
	pabio = (struct abio *)&(pbi->inp.value);
	pbi->mask=1;
	pbi->mask <<= pabio->signal;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pbi,
		"devBiAb16 (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long ioinfo_bi16(
    int			cmd,
    struct biRecord	*pbi,
    IOSCANPVT		*ppvt)
{
    struct abio *pabio;
    long        value;

    pabio = (struct abio *)&(pbi->inp.value);
    ab_bi_getioscanpvt(pabio->link,pabio->adapter,pabio->card,ppvt);
    /*call ab_bidriver so that it knows it has a binary input*/
    if(cmd==0) {
	(void) ab_bidriver(ABBI_16_BIT,pabio->link,pabio->adapter,
		pabio->card,pabio->plc_flag,pbi->mask,&value);
    }
    return(0);
}

static long read_bi16(struct biRecord *pbi)
{
	struct abio *pabio;
	int	    status;
	long	    value;

	
	pabio = (struct abio *)&(pbi->inp.value);
	status = ab_bidriver(ABBI_16_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pbi->mask,&value);
	if(status==0) {
		pbi->rval = value;
		return(0);
	} else {
                if(recGblSetSevr(pbi,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pbi->stat!=READ_ALARM || pbi->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pbi,"ab_bidriver Error");
		return(2);
	}
}

static long init_mbbi(struct mbbiRecord	*pmbbi)
{

    /* mbbi.inp must be an AB_IO */
    switch (pmbbi->inp.type) {
    case (AB_IO) :
	pmbbi->shft = pmbbi->inp.value.abio.signal;
	pmbbi->mask <<= pmbbi->shft;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
	    "devMbbiAb (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long ioinfo_mbbi(
    int			cmd,
    struct mbbiRecord	*pmbbi,
    IOSCANPVT		*ppvt)
{
    struct abio *pabio;
    unsigned long value;

    pabio = (struct abio *)&(pmbbi->inp.value);
    ab_bi_getioscanpvt(pabio->link,pabio->adapter,pabio->card,ppvt);
    /*call ab_bidriver so that it knows it has a binary input*/
    if(cmd==0) {
	(void) ab_bidriver(ABBI_08_BIT,pabio->link,pabio->adapter,
		pabio->card,pabio->plc_flag,pmbbi->mask,&value);
    }
    return(0);
}

static long read_mbbi(struct mbbiRecord	*pmbbi)
{
	struct abio *pabio;
	int	    status;
	unsigned long value;

	
	pabio = (struct abio *)&(pmbbi->inp.value);
	status = ab_bidriver(ABBI_08_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pmbbi->mask,&value);
	if(status==0) {
		pmbbi->rval = value;
		return(0);
	} else {
                if(recGblSetSevr(pmbbi,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pmbbi->stat!=READ_ALARM || pmbbi->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pmbbi,"ab_bidriver Error");
		return(2);
	}
}

static long init_mbbiDirect(struct mbbiDirectRecord	*pmbbi)
{

    /* mbbi.inp must be an AB_IO */
    switch (pmbbi->inp.type) {
    case (AB_IO) :
	pmbbi->shft = pmbbi->inp.value.abio.signal;
	pmbbi->mask <<= pmbbi->shft;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
	    "devMbbiDirectAb (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long ioinfo_mbbiDirect(
    int			cmd,
    struct mbbiDirectRecord	*pmbbi,
    IOSCANPVT		*ppvt)
{
    struct abio *pabio;
    unsigned long value;

    pabio = (struct abio *)&(pmbbi->inp.value);
    ab_bi_getioscanpvt(pabio->link,pabio->adapter,pabio->card,ppvt);
    /*call ab_bidriver so that it knows it has a binary input*/
    if(cmd==0) {
	(void) ab_bidriver(ABBI_08_BIT,pabio->link,pabio->adapter,
		pabio->card,pabio->plc_flag,pmbbi->mask,&value);
    }
    return(0);
}

static long read_mbbiDirect(struct mbbiDirectRecord	*pmbbi)
{
	struct abio *pabio;
	int	    status;
	unsigned long value;

	
	pabio = (struct abio *)&(pmbbi->inp.value);
	status = ab_bidriver(ABBI_08_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pmbbi->mask,&value);
	if(status==0) {
		pmbbi->rval = value;
		return(0);
	} else {
                if(recGblSetSevr(pmbbi,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pmbbi->stat!=READ_ALARM || pmbbi->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pmbbi,"ab_bidriver Error");
		return(2);
	}
}

static long init_mbbi16(struct mbbiRecord *pmbbi)
{

    /* mbbi.inp must be an AB_IO */
    switch (pmbbi->inp.type) {
    case (AB_IO) :
	pmbbi->shft = pmbbi->inp.value.abio.signal;
	pmbbi->mask <<= pmbbi->shft;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
	    "devMbbiAb16 (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long ioinfo_mbbi16(
    int			cmd,
    struct mbbiRecord	*pmbbi,
    IOSCANPVT		*ppvt)
{
    struct abio *pabio;
    unsigned long value;

    pabio = (struct abio *)&(pmbbi->inp.value);
    ab_bi_getioscanpvt(pabio->link,pabio->adapter,pabio->card,ppvt);
    /*call ab_bidriver so that it knows it has a binary input*/
    if(cmd==0) {
	(void) ab_bidriver(ABBI_16_BIT,pabio->link,pabio->adapter,
		pabio->card,pabio->plc_flag,pmbbi->mask,&value);
    }
    return(0);
}

static long read_mbbi16(struct mbbiRecord *pmbbi)
{
	struct abio *pabio;
	int	    status;
	unsigned long value;
	
	pabio = (struct abio *)&(pmbbi->inp.value);
	status = ab_bidriver(ABBI_16_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pmbbi->mask,&value);
	if(status==0) {
		pmbbi->rval = value;
		return(0);
	} else {
                if(recGblSetSevr(pmbbi,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pmbbi->stat!=READ_ALARM || pmbbi->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pmbbi,"ab_bidriver Error");
		return(2);
	}
}

static long init_mbbiDirect16(struct mbbiDirectRecord *pmbbi)
{

    /* mbbi.inp must be an AB_IO */
    switch (pmbbi->inp.type) {
    case (AB_IO) :
	pmbbi->shft = pmbbi->inp.value.abio.signal;
	pmbbi->mask <<= pmbbi->shft;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
	    "devMbbiDirectAb16 (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long ioinfo_mbbiDirect16(
    int			cmd,
    struct mbbiDirectRecord	*pmbbi,
    IOSCANPVT		*ppvt)
{
    struct abio *pabio;
    unsigned long value;

    pabio = (struct abio *)&(pmbbi->inp.value);
    ab_bi_getioscanpvt(pabio->link,pabio->adapter,pabio->card,ppvt);
    /*call ab_bidriver so that it knows it has a binary input*/
    if(cmd==0) {
	(void) ab_bidriver(ABBI_16_BIT,pabio->link,pabio->adapter,
		pabio->card,pabio->plc_flag,pmbbi->mask,&value);
    }
    return(0);
}

static long read_mbbiDirect16(struct mbbiDirectRecord *pmbbi)
{
	struct abio *pabio;
	int	    status;
	unsigned long value;
	
	pabio = (struct abio *)&(pmbbi->inp.value);
	status = ab_bidriver(ABBI_16_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pmbbi->mask,&value);
	if(status==0) {
		pmbbi->rval = value;
		return(0);
	} else {
                if(recGblSetSevr(pmbbi,READ_ALARM,INVALID_ALARM) && errVerbose
		&& (pmbbi->stat!=READ_ALARM || pmbbi->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pmbbi,"ab_bidriver Error");
		return(2);
	}
}

static long init_bo(struct boRecord *pbo)
{
    unsigned long value;
    struct abio *pabio;
    int status=0;

    /* bo.out must be an AB_IO */
    switch (pbo->out.type) {
    case (AB_IO) :
	pabio = (struct abio *)&(pbo->out.value);
        /* read the value via the ab driver */
	pbo->mask = 1;
	pbo->mask <<= pabio->signal;
        status = ab_boread(ABBO_08_BIT,pabio->link,pabio->adapter,
		pabio->card,&value,pbo->mask);
        if(status == 0) pbo->rval = value;
        else status=2;
	break;
    default :
	status=S_db_badField;
	recGblRecordError(status,(void *)pbo,"devBoAb (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_bo(struct boRecord *pbo)
{
	struct abio *pabio;
	int	    status;

	
	pabio = (struct abio *)&(pbo->out.value);
	status = ab_bodriver(ABBO_08_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pbo->rval,pbo->mask);
	if(status!=0) {
                if(recGblSetSevr(pbo,WRITE_ALARM,INVALID_ALARM) && errVerbose
		&& (pbo->stat!=WRITE_ALARM || pbo->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pbo,"ab_bodriver Error");
	}
	return(0);
}

static long init_bo16(struct boRecord *pbo)
{
    unsigned long value;
    struct abio *pabio;
    int status=0;

    /* bo.out must be an AB_IO */
    switch (pbo->out.type) {
    case (AB_IO) :
	pabio = (struct abio *)&(pbo->out.value);
        /* read the value via the ab driver */
	pbo->mask = 1;
	pbo->mask <<= pabio->signal;
        status = ab_boread(ABBO_16_BIT,pabio->link,pabio->adapter,
		pabio->card,&value,pbo->mask);
        if(status == 0) pbo->rval = value;
        else status = 2;
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pbo,
		"devBoAb16 (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_bo16(struct boRecord *pbo)
{
	struct abio *pabio;
	int	    status;

	
	pabio = (struct abio *)&(pbo->out.value);
	status = ab_bodriver(ABBO_16_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pbo->rval,pbo->mask);
	if(status!=0) {
                if(recGblSetSevr(pbo,WRITE_ALARM,INVALID_ALARM) && errVerbose
		&& (pbo->stat!=WRITE_ALARM || pbo->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pbo,"ab_bodriver Error");
	}
	return(0);
}

static long init_mbbo(struct mbboRecord	*pmbbo)
{
    unsigned long value;
    struct abio *pabio;
    int		status=0;

    /* mbbo.out must be an AB_IO */
    switch (pmbbo->out.type) {
    case (AB_IO) :
	pabio = &(pmbbo->out.value.abio);
	pmbbo->shft = pabio->signal;
	pmbbo->mask <<= pmbbo->shft;
	status = ab_boread(ABBO_08_BIT,pabio->link,pabio->adapter,pabio->card,&value,pmbbo->mask);
	if(status==0) pmbbo->rval = value;
	else status = 2;
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pmbbo,
		"devMbboAb (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_mbbo(struct mbboRecord *pmbbo)
{
	struct abio *pabio;
	int	    status;
	unsigned long value;

	
	pabio = &(pmbbo->out.value.abio);
	status = ab_bodriver(ABBO_08_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pmbbo->rval,pmbbo->mask);
	if(status!=0) {
                if(recGblSetSevr(pmbbo,WRITE_ALARM,INVALID_ALARM) && errVerbose
		&& (pmbbo->stat!=WRITE_ALARM || pmbbo->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pmbbo,"ab_bodriver Error");
	}
	return(0);
}

static long init_mbboDirect(struct mbboDirectRecord	*pmbbo)
{
    unsigned long value;
    struct abio *pabio;
    int		status=0;

    /* mbbo.out must be an AB_IO */
    switch (pmbbo->out.type) {
    case (AB_IO) :
	pabio = &(pmbbo->out.value.abio);
	pmbbo->shft = pabio->signal;
	pmbbo->mask <<= pmbbo->shft;
	status = ab_boread(ABBO_08_BIT,pabio->link,pabio->adapter,pabio->card,&value,pmbbo->mask);
	if(status==0) pmbbo->rval = value;
	else status = 2;
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pmbbo,
		"devMbboDirectAb (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_mbboDirect(struct mbboDirectRecord *pmbbo)
{
	struct abio *pabio;
	int	    status;
	unsigned long value;

	
	pabio = &(pmbbo->out.value.abio);
	status = ab_bodriver(ABBO_08_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pmbbo->rval,pmbbo->mask);
	if(status!=0) {
                if(recGblSetSevr(pmbbo,WRITE_ALARM,INVALID_ALARM) && errVerbose
		&& (pmbbo->stat!=WRITE_ALARM || pmbbo->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pmbbo,"ab_bodriver Error");
	}
	return(0);
}

static long init_mbbo16(struct mbboRecord *pmbbo)
{
    unsigned long value;
    struct abio *pabio;
    int		status=0;

    /* mbbo.out must be an AB_IO */
    switch (pmbbo->out.type) {
    case (AB_IO) :
	pabio = &(pmbbo->out.value.abio);
	pmbbo->shft = pabio->signal;
	pmbbo->mask <<= pmbbo->shft;
	status = ab_boread(ABBO_16_BIT,pabio->link,pabio->adapter,pabio->card,&value,pmbbo->mask);
	if(status==0) pmbbo->rval = value;
	else status = 2;
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pmbbo,
		"devMbboAb16 (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_mbbo16(struct mbboRecord *pmbbo)
{
	struct abio *pabio;
	int	    status;

	
	pabio = &(pmbbo->out.value.abio);
	status = ab_bodriver(ABBO_16_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pmbbo->rval,pmbbo->mask);
	if(status!=0) {
                if(recGblSetSevr(pmbbo,WRITE_ALARM,INVALID_ALARM) && errVerbose
		&& (pmbbo->stat!=WRITE_ALARM || pmbbo->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pmbbo,"ab_bodriver Error");
	}
	return(0);
}

static long init_mbboDirect16(struct mbboDirectRecord *pmbbo)
{
    unsigned long value;
    struct abio *pabio;
    int		status=0;

    /* mbbo.out must be an AB_IO */
    switch (pmbbo->out.type) {
    case (AB_IO) :
	pabio = &(pmbbo->out.value.abio);
	pmbbo->shft = pabio->signal;
	pmbbo->mask <<= pmbbo->shft;
	status = ab_boread(ABBO_16_BIT,pabio->link,pabio->adapter,pabio->card,&value,pmbbo->mask);
	if(status==0) pmbbo->rval = value;
	else status = 2;
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pmbbo,
		"devMbboDirectAb16 (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_mbboDirect16(struct mbboDirectRecord *pmbbo)
{
	struct abio *pabio;
	int	    status;

	
	pabio = &(pmbbo->out.value.abio);
	status = ab_bodriver(ABBO_16_BIT,pabio->link,pabio->adapter,
	   pabio->card,pabio->plc_flag,pmbbo->rval,pmbbo->mask);
	if(status!=0) {
                if(recGblSetSevr(pmbbo,WRITE_ALARM,INVALID_ALARM) && errVerbose
		&& (pmbbo->stat!=WRITE_ALARM || pmbbo->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pmbbo,"ab_bodriver Error");
	}
	return(0);
}

