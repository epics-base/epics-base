/* devAB1771IL.c */
/*
 *      Original Author: Bob Dalesio
 *      Current Author: Bob Dalesio, Marty Kraimer
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
 * .01  12-01-94	lrd	combine the device support that was resident
 *				in the driver and device support to
				significantly reduce the amount of code
 */


#include	<vxWorks.h>
#include	<stdlib.h>
#include 	<stdio.h>
#include	<string.h>

#include	<dbDefs.h>
#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<drvAb.h>
#include	<aiRecord.h>

/* Create the dsets*/
LOCAL long ioinfo_ai(int cmd,struct aiRecord  *prec,IOSCANPVT *ppvt);
LOCAL long init_1771Il(struct aiRecord *prec);
LOCAL long linconv_1771Il(struct aiRecord *prec, int after);
LOCAL long read_1771Il(struct aiRecord *prec);

typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;} ABAIDSET;

ABAIDSET devAiAb1771Il={6, 0, 0, init_1771Il,
	ioinfo_ai, read_1771Il, linconv_1771Il};

#define IL_SCAN_RATE 		4
#define	IL_INITMSG_LENGTH	19
#define	IL_READMSG_LENGTH	12

#define IL_RANGE	0xffff	/* volt inp for all channels -10 - 10V */
#define IL_DATA_FORMAT	0x600		/* signed magnitude binary */
struct	ab1771il_read {
	unsigned short	diag;	/* diagnostic word */
	unsigned short	urange;	/* low byte - under range channels */
	unsigned short	orange;	/* low byte - over  range channels */
	unsigned short	sign;	/* low byte - polarity 1 = negative */
	short		data[8];	/* 8 data values (can be signed) */
};

typedef struct {
	void		*drvPvt;
	IOSCANPVT	ioscanpvt;
	unsigned short	read_msg[IL_READMSG_LENGTH];
	unsigned short	*pinit_msg;
} devPvt;

LOCAL unsigned short initMsg[IL_INITMSG_LENGTH] = {
	IL_RANGE,IL_DATA_FORMAT,0x0ff,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095
};

LOCAL void devCallback(void * drvPvt)
{
    devPvt	*pdevPvt;

    pdevPvt = (devPvt *)(*pabDrv->getUserPvt)(drvPvt);
    if(!pdevPvt) return;
    scanIoRequest(pdevPvt->ioscanpvt);
}

LOCAL long ioinfo_ai(int cmd,struct aiRecord  *prec,IOSCANPVT *ppvt)
{
    devPvt	*pdevPvt;

    pdevPvt = (devPvt *)prec->dpvt;
    if(!pdevPvt) return(0);
    *ppvt = pdevPvt->ioscanpvt;
    return(0);
}

LOCAL long read_1771Il(struct aiRecord *prec)
{
	struct abio 		*pabio;
	devPvt			*pdevPvt= (devPvt *)prec->dpvt;
	abStatus		drvStatus;
        struct ab1771il_read	*pdata;

	if(!pdevPvt) {
                recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
		return(2); /*dont convert*/
	}
	pabio = (struct abio *)&(prec->inp.value);
	drvStatus = (*pabDrv->getStatus)(pdevPvt->drvPvt);
	pdata = (struct ab1771il_read *)&pdevPvt->read_msg[0];
	if(drvStatus != abSuccess) {
                recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
		return(2); /*dont convert*/
	}
	prec->rval = pdata->data[pabio->signal];	
	if((pdata->urange&(1<<pabio->signal))
	|| (pdata->orange&(1<<pabio->signal)) )
	    recGblSetSevr(prec,HW_LIMIT_ALARM,INVALID_ALARM);
	return(0);
}

static long linconv_1771Il(struct aiRecord *prec, int after)
{

    /* set linear conversion slope*/
    prec->eslo = (prec->eguf -prec->egul)/4095.0;
    return(0);
}

LOCAL long init_1771Il(struct aiRecord	*prec)
{
    struct abio *pabio;
    devPvt	*pdevPvt;
    abStatus 	drvStatus;
    long	status=0;
    void	*drvPvt;

    /* ai.inp must be an AB_IO */
    if (prec->inp.type != AB_IO){
    	recGblRecordError(S_db_badField,(void *)prec,
    		"devAiAb1771Il (init_record) Illegal INP field");
    	return(S_db_badField);
    }
    prec->eslo = (prec->eguf -prec->egul)/4095.0;
    /* pointer to the data addess structure */
    pabio = (struct abio *)&(prec->inp.value);
    drvStatus = (*pabDrv->registerCard)(pabio->link,pabio->adapter,
    	pabio->card,typeAi,"IL",devCallback,&drvPvt);
    switch(drvStatus) {
    case abSuccess :
        pdevPvt = (devPvt *)(*pabDrv->getUserPvt)(drvPvt);
        prec->dpvt = pdevPvt; 
	break;
    case abNewCard :
        pdevPvt = calloc(1,sizeof(devPvt));
        pdevPvt->drvPvt = drvPvt;
        prec->dpvt = pdevPvt; 
        (*pabDrv->setUserPvt)(drvPvt,(void *)pdevPvt);
        scanIoInit(&pdevPvt->ioscanpvt);
	pdevPvt->pinit_msg = initMsg;
        drvStatus = (*pabDrv->startScan)(drvPvt,IL_SCAN_RATE,
		pdevPvt->pinit_msg,IL_INITMSG_LENGTH,
		&pdevPvt->read_msg[0],IL_READMSG_LENGTH);
	if(drvStatus != abSuccess) {
	    status = S_db_badField;
    	    recGblRecordError(status,(void *)prec,
		"devAiAb1771Il (init_record) startScan");
	}
	break;
    default:
	status = S_db_badField;
    	recGblRecordError(status,(void *)prec,
	    "devAiAb1771Il (init_record) registerCard");
	break;
    }
    return(status);
}
