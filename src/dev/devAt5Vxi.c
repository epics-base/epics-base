/* devAt5Vxi.c */
/* share/src/dev $Id$ */

/* devAt5Vxi.c - Device Support Routines */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
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
 * .01  08-21-92	mrk	Replaces individual At5Vxi modules
 * .02 	05-27-92	joh	changed linear conversion
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<dbScan.h>
#include	<link.h>
#include	<module_types.h>
#include	<aiRecord.h>
#include	<aoRecord.h>
#include	<biRecord.h>
#include	<boRecord.h>
#include	<mbbiRecord.h>
#include	<mbboRecord.h>

/* The following must match the definition in choiceGbl.ascii */
#define LINEAR 1

static long init_ai();
static long init_ao();
static long init_bi();
static long init_bo();
static long init_mbbi();
static long init_mbbo();
static long ai_ioinfo();
static long bi_ioinfo();
static long mbbi_ioinfo();
static long read_ai();
static long write_ao();
static long read_bi();
static long write_bo();
static long read_mbbi();
static long write_mbbo();
static long ai_lincvt();
static long ao_lincvt();


typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	DEVSUPFUN	special_linconv;} AT5VXIDSET;


AT5VXIDSET devAiAt5Vxi=  {6,NULL,NULL,init_ai,  ai_ioinfo,  read_ai, ai_lincvt};
AT5VXIDSET devAoAt5Vxi=  {6,NULL,NULL,init_ao,       NULL,  write_ao,ao_lincvt};
AT5VXIDSET devBiAt5Vxi=  {6,NULL,NULL,init_bi,  bi_ioinfo,  read_bi,   NULL};
AT5VXIDSET devBoAt5Vxi=  {6,NULL,NULL,init_bo,       NULL,  write_bo,  NULL};
AT5VXIDSET devMbbiAt5Vxi={6,NULL,NULL,init_mbbi,mbbi_ioinfo,read_mbbi, NULL};
AT5VXIDSET devMbboAt5Vxi={6,NULL,NULL,init_mbbo,       NULL,write_mbbo,NULL};

static long init_ai( struct aiRecord	*pai)
{
    unsigned short value;
    struct vmeio *pvmeio;
    long  status;

    /* ai.inp must be an VME_IO */
    switch (pai->inp.type) {
    case (VME_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiAt5Vxi (init_record) Illegal INP field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/0xffff;

    /* call driver so that it configures card */
    pvmeio = (struct vmeio *)&(pai->inp.value);
    if(status=at5vxi_ai_driver(pvmeio->card,pvmeio->signal,&value)) {
	recGblRecordError(status,(void *)pai,
		"devAiAt5Vxi (init_record) at5vxi_ai_driver error");
	return(status);
    }
    return(0);
}

static long ai_ioinfo(
    int               cmd,
    struct aiRecord     *pai,
    IOSCANPVT		*ppvt)
{
    at5vxi_getioscanpvt(pai->inp.value.vmeio.card,ppvt);
    return(0);
}

static long read_ai(struct aiRecord	*pai)
{
	struct vmeio *pvmeio;
	int	     status;
	unsigned short value;

	
	pvmeio = (struct vmeio *)&(pai->inp.value);
	status = at5vxi_ai_driver(pvmeio->card,pvmeio->signal,&value);
	if(status==0 || status==-2) pai->rval = value;
        if(status==-1) {
		status = 2; /*don't convert*/
                recGblSetSevr(pai,READ_ALARM,INVALID_ALARM);
        }else if(status==-2) {
                status=0;
                recGblSetSevr(pai,HW_LIMIT_ALARM,INVALID_ALARM);
        }
	return(status);
}

static long ai_lincvt(struct aiRecord	*pai, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pai->eslo = (pai->eguf -pai->egul)/0xffff;
    return(0);
}

static void read_ao(); /* forward reference*/

static long init_ao(struct aoRecord	*pao)
{

    /* ao.out must be an VME_IO */
    switch (pao->out.type) {
    case (VME_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pao,
		"devAoAt5Vxi (init_record) Illegal OUT field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pao->eslo = (pao->eguf -pao->egul)/0xffff;

    /* call driver so that it configures card */
    read_ao(pao);
    return(0);
}

static long write_ao(struct aoRecord	*pao)
{
	struct vmeio 	*pvmeio;
	int	    	status;
	unsigned short  value,rbvalue;

	
	pvmeio = (struct vmeio *)&(pao->out.value);
	value = pao->rval;
	status = at5vxi_ao_driver(pvmeio->card,pvmeio->signal,&value,&rbvalue);
	if(status==0 || status==-2) pao->rbv = rbvalue;
	if(status==-1) {
                recGblSetSevr(pao,WRITE_ALARM,INVALID_ALARM);
	}else if(status==-2) {
		status=0;
                recGblSetSevr(pao,HW_LIMIT_ALARM,INVALID_ALARM);
	}
	return(status);
}

static long ao_lincvt( struct aoRecord	*pao, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pao->eslo = (pao->eguf -pao->egul)/0xffff;
    return(0);
}

static void read_ao(pao)
struct aoRecord      *pao;
{
	unsigned short          value;
	struct vmeio    		*pvmeio = &pao->out.value.vmeio;

	/* get the value from the ao driver */
	at5vxi_ao_read(pvmeio->card,pvmeio->signal,&value);
	pao->rbv = pao->rval = value;
	return;
}

static long init_bi( struct biRecord	*pbi)
{
    struct vmeio *pvmeio;


    /* bi.inp must be an VME_IO */
    switch (pbi->inp.type) {
    case (VME_IO) :
	pvmeio = (struct vmeio *)&(pbi->inp.value);
	pbi->mask=1;
	pbi->mask <<= pvmeio->signal;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pbi,
		"devBiAt5Vxi (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long bi_ioinfo(
    int               cmd,
    struct biRecord     *pbi,
    IOSCANPVT		*ppvt)
{
    at5vxi_getioscanpvt(pbi->inp.value.vmeio.card,ppvt);
    return(0);
}

static long read_bi(struct biRecord	*pbi)
{
	struct vmeio *pvmeio;
	int	    status;
	long	    value;

	
	pvmeio = (struct vmeio *)&(pbi->inp.value);
	status = at5vxi_bi_driver(pvmeio->card,pbi->mask,&value);
	if(status==0) {
		pbi->rval = value;
		return(0);
	} else {
                recGblSetSevr(pbi,READ_ALARM,INVALID_ALARM);
		return(2);
	}
}

static long init_bo(struct boRecord	*pbo)
{
    unsigned int value;
    int status=0;
    struct vmeio *pvmeio;

    /* bo.out must be an VME_IO */
    switch (pbo->out.type) {
    case (VME_IO) :
	pvmeio = (struct vmeio *)&(pbo->out.value);
	pbo->mask = 1;
	pbo->mask <<= pvmeio->signal;
        status = at5vxi_bi_driver(pvmeio->card,pbo->mask,&value);
        if(status == 0) pbo->rbv = pbo->rval = value;
        else status = 2;
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pbo,
	    "devBoAt5Vxi (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_bo(struct boRecord	*pbo)
{
	struct vmeio *pvmeio;
	int	    status;

	
	pvmeio = (struct vmeio *)&(pbo->out.value);
	status = at5vxi_bo_driver(pvmeio->card,pbo->rval,pbo->mask);
	if(status!=0) {
                recGblSetSevr(pbo,WRITE_ALARM,INVALID_ALARM);
	}
	return(status);
}

static long init_mbbi(struct mbbiRecord	*pmbbi)
{

    /* mbbi.inp must be an VME_IO */
    switch (pmbbi->inp.type) {
    case (VME_IO) :
	pmbbi->shft = pmbbi->inp.value.vmeio.signal;
	pmbbi->mask <<= pmbbi->shft;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
		"devMbbiAt5Vxi (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long mbbi_ioinfo(
    int               cmd,
    struct mbbiRecord     *pmbbi,
    IOSCANPVT		*ppvt)
{
    at5vxi_getioscanpvt(pmbbi->inp.value.vmeio.card,ppvt);
    return(0);
}

static long read_mbbi(struct mbbiRecord	*pmbbi)
{
	struct vmeio	*pvmeio;
	int		status;
	unsigned long	value;

	
	pvmeio = (struct vmeio *)&(pmbbi->inp.value);
	status = at5vxi_bi_driver(pvmeio->card,pmbbi->mask,&value);
	if(status==0) {
		pmbbi->rval = value;
	} else {
                recGblSetSevr(pmbbi,READ_ALARM,INVALID_ALARM);
	}
	return(status);
}

static long init_mbbo(struct mbboRecord	*pmbbo)
{
    unsigned long value;
    struct vmeio *pvmeio;
    int		status = 0;

    /* mbbo.out must be an VME_IO */
    switch (pmbbo->out.type) {
    case (VME_IO) :
	pvmeio = &(pmbbo->out.value.vmeio);
	pmbbo->shft = pvmeio->signal;
	pmbbo->mask <<= pmbbo->shft;
	status = at5vxi_bi_driver(pvmeio->card,pmbbo->mask,&value);
	if(status==0) pmbbo->rbv = pmbbo->rval = value;
	else status = 2;
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pmbbo,
		"devMbboAt5Vxi (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_mbbo(struct mbboRecord	*pmbbo)
{
	struct vmeio *pvmeio;
	int	    status;
	unsigned long value;

	
	pvmeio = &(pmbbo->out.value.vmeio);
	status = at5vxi_bo_driver(pvmeio->card,pmbbo->rval,pmbbo->mask);
	if(status==0) {
		status = at5vxi_bi_driver(pvmeio->card,pmbbo->mask,&value);
		if(status==0) pmbbo->rbv = value;
                else recGblSetSevr(pmbbo,READ_ALARM,INVALID_ALARM);
	} else {
                recGblSetSevr(pmbbo,WRITE_ALARM,INVALID_ALARM);
	}
	return(status);
}
