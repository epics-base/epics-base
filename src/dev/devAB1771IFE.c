/* devAB1771IFE.c */
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
LOCAL long init_1771IfeDiff(struct aiRecord *prec);
LOCAL long init_1771Ife0to5V(struct aiRecord *prec);
LOCAL long init_1771IfeMa(struct aiRecord *prec);
LOCAL long init_1771IfeSe(struct aiRecord *prec);
LOCAL long linconv_1771Ife(struct aiRecord *prec, int after);
LOCAL long read_1771Ife(struct aiRecord *prec);

typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;} ABAIDSET;

ABAIDSET devAiAb1771Ife={6, 0, 0, init_1771IfeDiff,
	ioinfo_ai, read_1771Ife, linconv_1771Ife};
ABAIDSET devAiAb1771Ife0to5V={6, 0, 0, init_1771Ife0to5V,
	ioinfo_ai, read_1771Ife, linconv_1771Ife};
ABAIDSET devAiAb1771IfeMa={6, 0, 0, init_1771IfeMa,
	ioinfo_ai, read_1771Ife, linconv_1771Ife};
ABAIDSET devAiAb1771IfeSe={6, 0, 0, init_1771IfeSe,
	ioinfo_ai, read_1771Ife, linconv_1771Ife};

#define IFE_SCAN_RATE 		1
#define	IFE_INITMSG_LENGTH	37
#define	IFE_DIFF_READMSG_LENGTH	12
#define	IFE_SE_READMSG_LENGTH	20

struct	ab1771Ife_read {
	unsigned short	diag;	/* diagnostic word */
	unsigned short	urange;	/* low byte - under range channels */
	unsigned short	orange;	/* low byte - over  range channels */
	unsigned short	sign;	/* low byte - polarity 1 = negative */
	short		data[16];	/* 16 data values (can be signed) */
};

/* IFE Differential Initialization Message - from the IFE Manual */
LOCAL short	ab1771IfeDiff_initmsg[IFE_INITMSG_LENGTH] = {
	0xffff, 0xffff,			/* -10 to 10 volts (signals 0 - 15) */
	0x0700,				/* signed magnitude - differential */
	0xffff, 0x0000,			/* scaled 0 - 4095 (12 bits)	*/
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095
};
/* 1771 - IFE 0 to 5 Volt */
LOCAL short	ab1771Ife0to5v_initmsg[IFE_INITMSG_LENGTH] = {
	0x5555, 0x5555,			/* 0 to 5 volts (signals 0 - 15) */
	0x0700,				/* signed magnitude - differential */
	0x0000, 0x0000, 		/* scaled 0 - 4095 (12 bits)	*/
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095
};
/* 1771 - IFE MilliAmp */
LOCAL short	ab1771IfeMa_initmsg[IFE_INITMSG_LENGTH] = {
	0x0000, 0x0000,			/* 4 - 20 Ma (signals 0 - 15) */
	0x0700,				/* signed magnitude - differential */
	0x0000, 0x0000, 		/* scaled 0 - 4095 (12 bits)	*/
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095
};
/* 1771 - IFE Single Ended (-10 to 10 Volts) */
LOCAL short	ab1771IfeSe_initmsg[IFE_INITMSG_LENGTH] = {
	0xffff, 0xffff,			/* 0 to 5 volts (signals 0 - 15) */
	0x0600,				/* signed magnitude - differential */
	0xffff, 0x0000, 		/* scaled 0 - 4095 (12 bits)	*/
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095,
	0, 0x4095, 0, 0x4095, 0, 0x4095, 0, 0x4095
};

typedef struct {
	void		*drvPvt;
	IOSCANPVT	ioscanpvt;
	unsigned short	read_msg_len;
	unsigned short	*pread_msg;
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

LOCAL long read_1771Ife(struct aiRecord *prec)
{
	struct abio 		*pabio;
	devPvt			*pdevPvt= (devPvt *)prec->dpvt;
	abStatus		drvStatus;
        struct ab1771Ife_read	*pdata;

	if(!pdevPvt) {
                recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
		return(2); /*dont convert*/
	}
	pabio = (struct abio *)&(prec->inp.value);
	drvStatus = (*pabDrv->getStatus)(pdevPvt->drvPvt);
	pdata = (struct ab1771Ife_read *)pdevPvt->pread_msg;
	if(drvStatus != abSuccess) {
                recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
		return(2); /*dont convert*/
	}
	prec->rval = pdata->data[pabio->signal];	
	if( (pdata->orange & (1 << pabio->signal))
	||  (pdata->urange & (1 << pabio->signal)) )
          	recGblSetSevr(prec,HW_LIMIT_ALARM,INVALID_ALARM);
	return(0);
}

LOCAL long linconv_1771Ife(struct aiRecord *prec, int after)
{

    if(!after) return(0);
    /* set linear conversion slope*/
    prec->eslo = (prec->eguf -prec->egul)/4095.0;
    return(0);
}

LOCAL long init_1771Ife(struct aiRecord *prec, const char *card_type,
	unsigned short *pinit_msg, unsigned short  read_size)
{
	struct abio 	*pabio;
	devPvt		*pdevPvt;
	abStatus 	drvStatus;
	long		status=0;
	void		*drvPvt;

	/* ai.inp must be an AB_IO */
	if (prec->inp.type != AB_IO){
		recGblRecordError(S_db_badField,(void *)prec,
			"devAiAb1771Ife (init_record) Illegal INP field");
		return(S_db_badField);
	}

	/* set linear conversion slope*/
	prec->eslo = (prec->eguf -prec->egul)/4095.0;
	/* pointer to the data addess structure */
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
	    pdevPvt->read_msg_len = read_size;
	    pdevPvt->pread_msg = calloc(read_size,sizeof(unsigned short));
	    drvStatus = (*pabDrv->startScan)(drvPvt,IFE_SCAN_RATE,
		pinit_msg,IFE_INITMSG_LENGTH,pdevPvt->pread_msg ,read_size);
	    if(drvStatus!=abSuccess) {
		status = S_db_badField;
		recGblRecordError(S_db_badField,(void *)prec,
			"devAiAb1771Ife (init_record) startScan");
	    }
	    break;
	default:
	    status = S_db_badField;
	    recGblRecordError(status,(void *)prec,
		"devAiAb1771Ife (init_record) registerCard");
	    break;
	}
	return(status);
}

LOCAL long init_1771IfeDiff(struct aiRecord	*prec)
{
	return(init_1771Ife(prec,"IFEDIFF",&ab1771IfeDiff_initmsg[0],
		IFE_DIFF_READMSG_LENGTH));
}

LOCAL long init_1771Ife0to5V(struct aiRecord *prec)
{
	return(init_1771Ife(prec,"IFE0to5V",&ab1771Ife0to5v_initmsg[0],
		IFE_DIFF_READMSG_LENGTH));
}

LOCAL long init_1771IfeMa(struct aiRecord *prec)
{
	return(init_1771Ife(prec,"IFEMA",&ab1771IfeMa_initmsg[0],
		IFE_DIFF_READMSG_LENGTH));
}

LOCAL long init_1771IfeSe(struct aiRecord *prec)
{
	return(init_1771Ife(prec,"IFESE",&ab1771IfeSe_initmsg[0],
		IFE_SE_READMSG_LENGTH));
}
