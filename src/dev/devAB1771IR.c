/* devAB1771IR.c */
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
LOCAL long init_1771Ir(struct aiRecord *prec, const char *card_type,
	unsigned short config);
LOCAL long ioinfo_ai(int cmd,struct aiRecord  *prec,IOSCANPVT *ppvt);
LOCAL long init_1771IrPlatinum(struct aiRecord *prec);
LOCAL long init_1771IrCopper(struct aiRecord *prec);
LOCAL long read_1771Ir(struct aiRecord *prec);

typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;} ABAIDSET;

ABAIDSET devAiAb1771IrPlatinum={6, 0, 0, init_1771IrPlatinum,
	ioinfo_ai, read_1771Ir, 0};
ABAIDSET devAiAb1771IrCopper={6, 0, 0, init_1771IrCopper,
	ioinfo_ai, read_1771Ir, 0};

#define IR_SCAN_RATE 		5
#define	IR_INITMSG_LENGTH	1
#define	IR_READMSG_LENGTH	8

/*Register definitions*/
#define IR_UNITS_DEGF	0x40
#define IR_UNITS_OHMS	0x80
#define IR_COPPER	0x100
#define IR_SIGNED	0x400


struct ab1771Ir_read {
	unsigned short	status;		/* status and over/under range	*/
	unsigned short	pol_over;	/* polarity and overflow	*/
	short		data[6];	/* current values */
};

typedef struct {
	void		*drvPvt;
	IOSCANPVT	ioscanpvt;
	unsigned short	read_msg[IR_READMSG_LENGTH];
	unsigned short	write_msg;
} devPvt;

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

LOCAL long read_1771Ir(struct aiRecord *prec)
{
	struct abio 		*pabio;
	devPvt			*pdevPvt= (devPvt *)prec->dpvt;
	abStatus		drvStatus;
        struct ab1771Ir_read	*pdata;

	if(!pdevPvt) {
                recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
		return(2); /*dont convert*/
	}
	pabio = (struct abio *)&(prec->inp.value);
	drvStatus = (*pabDrv->getStatus)(pdevPvt->drvPvt);
	pdata = (struct ab1771Ir_read *)&pdevPvt->read_msg;
	if(drvStatus != abSuccess) {
                recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
		return(2); /*dont convert*/
	}else{
	    unsigned short	mask;
	    short		value;
	    unsigned short	signal = pabio->signal;

	    value = (short)pdata->data[signal];
	    if(pdata->pol_over& (0x100<<signal)) value = - value;
	    prec->rval = value;
	    mask = (1 << pabio->signal);
	    if((pdata->status & (1<<signal))
	    ||(pdata->status & (0x100<<signal))){
            	recGblSetSevr(prec,HW_LIMIT_ALARM,INVALID_ALARM);
	    }
	    return(0);
	}
}

LOCAL long init_1771Ir(struct aiRecord *prec, const char *card_type,
	unsigned short config)
{
	struct abio 	*pabio;
	devPvt		*pdevPvt;
	abStatus 	drvStatus;
	long		status=0;
	void		*drvPvt;

	if (prec->inp.type != AB_IO){
		recGblRecordError(S_db_badField,(void *)prec,
			"devAiAb1771Ir (init_record) Illegal INP field");
		return(S_db_badField);
	}
	prec->linr = 0;
	pabio = (struct abio *)&(prec->inp.value);
	drvStatus = (*pabDrv->registerCard)(pabio->link,pabio->adapter,
		pabio->card,typeAi,card_type,devCallback,&drvPvt);
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
	    if(pabio->parm[0]=='C') {
		strcpy(prec->egu,"degC");
	    } else if(pabio->parm[0]=='O') {
		config |= IR_UNITS_OHMS;
		strcpy(prec->egu,"ohms");
	    } else {
		config |= IR_UNITS_DEGF;
		strcpy(prec->egu,"degF");
	    }
	    config |= IR_SIGNED;
	    pdevPvt->write_msg = config;
	    drvStatus = (*pabDrv->startScan)(drvPvt,IR_SCAN_RATE,
		&pdevPvt->write_msg,IR_INITMSG_LENGTH,
		pdevPvt->read_msg,IR_READMSG_LENGTH);
	    if(drvStatus != abSuccess) {
		status = S_db_badField;
		recGblRecordError(status,(void *)prec,
			"devAiAb1771Ir (init_record) startScan");
	    }
	    break;
	default:
	    status = S_db_badField;
	    recGblRecordError(status,(void *)prec,
		"devAiAb1771Ir (init_record) registerCard");
	    break;
	}
	return(status);
}

LOCAL long init_1771IrPlatinum(struct aiRecord	*prec)
{
	return(init_1771Ir(prec,"IRPLATINUM",0));
}

LOCAL long init_1771IrCopper(struct aiRecord *prec)
{
	return(init_1771Ir(prec,"IRCOPPER",IR_COPPER));
}
