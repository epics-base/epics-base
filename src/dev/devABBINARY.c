/* devABBINARY.c */
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
 * .01  12-01-94	lrd	combine the device support that was resident in the driver and
 *				device support to significantly reduce the amount of code
 * 	...
 */


#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	<dbDefs.h>
#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<drvAb.h>
#include        <dbCommon.h>
#include        <biRecord.h>
#include        <boRecord.h>
#include        <mbbiRecord.h>
#include        <mbboRecord.h>
#include        <mbbiDirectRecord.h>
#include        <mbboDirectRecord.h>

typedef struct {
	void		*drvPvt;
	IOSCANPVT	ioscanpvt;
}devPvt;


/* Create the dsets*/
LOCAL long ioinfo(int cmd, struct dbCommon *prec, IOSCANPVT *ppvt);
LOCAL long read_bi(struct biRecord *prec);
LOCAL long init_bi08(struct biRecord *prec);
LOCAL long init_bi16(struct biRecord *prec);
LOCAL long init_bi32(struct biRecord *prec);
LOCAL long init_bi(struct biRecord *prec,abNumBits nBits);
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;} ABBIDSET;

ABBIDSET devBiAb=    { 5, 0, 0, init_bi08,   ioinfo,     read_bi};
ABBIDSET devBiAb16=  { 5, 0, 0, init_bi16,   ioinfo,     read_bi};
ABBIDSET devBiAb32=  { 5, 0, 0, init_bi32,   ioinfo,     read_bi};

LOCAL long read_mbbi(struct mbbiRecord *prec);
LOCAL long init_mbbi08(struct mbbiRecord *prec);
LOCAL long init_mbbi16(struct mbbiRecord *prec);
LOCAL long init_mbbi32(struct mbbiRecord *prec);
LOCAL long init_mbbi(struct mbbiRecord *prec,abNumBits nBits);
ABBIDSET devMbbiAb=  { 5, 0, 0, init_mbbi08, ioinfo,   read_mbbi};
ABBIDSET devMbbiAb16={ 5, 0, 0, init_mbbi16, ioinfo,   read_mbbi};
ABBIDSET devMbbiAb32={ 5, 0, 0, init_mbbi32, ioinfo,   read_mbbi};

LOCAL long read_mbbiDirect(struct mbbiDirectRecord *prec);
LOCAL long init_mbbiDirect08(struct mbbiDirectRecord *prec);
LOCAL long init_mbbiDirect16(struct mbbiDirectRecord *prec);
LOCAL long init_mbbiDirect32(struct mbbiDirectRecord *prec);
LOCAL long init_mbbiDirect(struct mbbiDirectRecord *prec,abNumBits nBits);
ABBIDSET devMbbiDirectAb=  { 5,0,0, init_mbbiDirect08,ioinfo, read_mbbiDirect};
ABBIDSET devMbbiDirectAb16={ 5,0,0, init_mbbiDirect16,ioinfo, read_mbbiDirect};
ABBIDSET devMbbiDirectAb32={ 5,0,0, init_mbbiDirect32,ioinfo, read_mbbiDirect};

LOCAL long write_bo(struct boRecord *prec);
LOCAL long init_bo08(struct boRecord *prec);
LOCAL long init_bo16(struct boRecord *prec);
LOCAL long init_bo32(struct boRecord *prec);
LOCAL long init_bo(struct boRecord *prec,abNumBits nBits);
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
} ABBODSET;

ABBODSET devBoAb=    { 5, 0, 0, init_bo08,   0, write_bo};
ABBODSET devBoAb16=  { 5, 0, 0, init_bo16,   0, write_bo};
ABBODSET devBoAb32=  { 5, 0, 0, init_bo32,   0, write_bo};

LOCAL long write_mbbo(struct mbboRecord *prec);
LOCAL long init_mbbo08(struct mbboRecord *prec);
LOCAL long init_mbbo16(struct mbboRecord *prec);
LOCAL long init_mbbo32(struct mbboRecord *prec);
LOCAL long init_mbbo(struct mbboRecord *prec,abNumBits nBits);
ABBODSET devMbboAb=  { 5, 0, 0, init_mbbo08, 0, write_mbbo};
ABBODSET devMbboAb16={ 5, 0, 0, init_mbbo16, 0, write_mbbo};
ABBODSET devMbboAb32={ 5, 0, 0, init_mbbo32, 0, write_mbbo};

LOCAL long write_mbboDirect(struct mbboDirectRecord *prec);
LOCAL long init_mbboDirect08(struct mbboDirectRecord *prec);
LOCAL long init_mbboDirect16(struct mbboDirectRecord *prec);
LOCAL long init_mbboDirect32(struct mbboDirectRecord *prec);
LOCAL long init_mbboDirect(struct mbboDirectRecord *prec,abNumBits nBits);
ABBODSET devMbboDirectAb=  { 5, 0, 0, init_mbboDirect08, 0, write_mbboDirect};
ABBODSET devMbboDirectAb16={ 5, 0, 0, init_mbboDirect16, 0, write_mbboDirect};
ABBODSET devMbboDirectAb32={ 5, 0, 0, init_mbboDirect32, 0, write_mbboDirect};

LOCAL void devCallback(void * drvPvt)
{
    devPvt	*pdevPvt;

    pdevPvt = (devPvt *)(*pabDrv->getUserPvt)(drvPvt);
    if(!pdevPvt) return;
    if(!pdevPvt->ioscanpvt) return;
    scanIoRequest(pdevPvt->ioscanpvt);
}

LOCAL long ioinfo(int cmd, struct dbCommon *prec, IOSCANPVT *ppvt)
{
    devPvt	*pdevPvt;

    pdevPvt = prec->dpvt;
    if(!pdevPvt) return(0);
    *ppvt = pdevPvt->ioscanpvt;
    return(0);
}
LOCAL long init_bi08(struct biRecord *prec)
{
	return(init_bi(prec, abBit8));
}
LOCAL long init_bi16(struct biRecord *prec)
{
	return(init_bi(prec, abBit16));
}
LOCAL long init_bi32(struct biRecord *prec)
{
	return(init_bi(prec, abBit32));
}

LOCAL long init_mbbi08(struct mbbiRecord *prec)
{
	return(init_mbbi(prec, abBit8));
}
LOCAL long init_mbbi16(struct mbbiRecord *prec)
{
	return(init_mbbi(prec, abBit16));
}
LOCAL long init_mbbi32(struct mbbiRecord *prec)
{
	return(init_mbbi(prec, abBit32));
}

LOCAL long init_mbbiDirect08(struct mbbiDirectRecord *prec)
{
	return(init_mbbiDirect(prec, abBit8));
}
LOCAL long init_mbbiDirect16(struct mbbiDirectRecord *prec)
{
	return(init_mbbiDirect(prec, abBit16));
}
LOCAL long init_mbbiDirect32(struct mbbiDirectRecord *prec)
{
	return(init_mbbiDirect(prec, abBit32));
}

LOCAL long init_bo08(struct boRecord *prec)
{
	return(init_bo(prec, abBit8));
}
LOCAL long init_bo16(struct boRecord *prec)
{
	return(init_bo(prec, abBit16));
}
LOCAL long init_bo32(struct boRecord *prec)
{
	return(init_bo(prec, abBit32));
}

LOCAL long init_mbbo08(struct mbboRecord *prec)
{
	return(init_mbbo(prec, abBit8));
}
LOCAL long init_mbbo16(struct mbboRecord *prec)
{
	return(init_mbbo(prec, abBit16));
}
LOCAL long init_mbbo32(struct mbboRecord *prec)
{
	return(init_mbbo(prec, abBit32));
}

LOCAL long init_mbboDirect08(struct mbboDirectRecord *prec)
{
	return(init_mbboDirect(prec, abBit8));
}
LOCAL long init_mbboDirect16(struct mbboDirectRecord *prec)
{
	return(init_mbboDirect(prec, abBit16));
}
LOCAL long init_mbboDirect32(struct mbboDirectRecord *prec)
{
	return(init_mbboDirect(prec, abBit32));
}

LOCAL long read_bi(struct biRecord *prec)
{
	devPvt		*pdevPvt = (devPvt *)prec->dpvt;
	void		*drvPvt;
	abStatus	drvStatus;
	unsigned long	value;

	if(!pdevPvt) {
	    recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
	    return(2);
	}
	drvPvt = pdevPvt->drvPvt;
	drvStatus = (*pabDrv->readBi)(drvPvt,&value,prec->mask);
        if(drvStatus!=abSuccess) {
 		recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
		return(2);
	}
	prec->rval = value;
	return(0);
}
LOCAL long init_bi(struct biRecord *prec,abNumBits nBits)
{
    struct abio	*pabio;
    devPvt	*pdevPvt;
    abStatus	drvStatus;
    long	status=0;
    void	*drvPvt;

    if (prec->inp.type != AB_IO){
    	recGblRecordError(S_db_badField,(void *)prec,
		"init_record: bad INP field");
    	return(S_db_badField);
    }
    pabio = (struct abio *)&(prec->inp.value);
    prec->mask=1;
    prec->mask <<= pabio->signal;
    drvStatus = (*pabDrv->registerCard)(pabio->link,pabio->adapter,
	pabio->card,typeBi,"BINARY",devCallback,&drvPvt);
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
	drvStatus = (*pabDrv->setNbits)(drvPvt,nBits);
	if(drvStatus!=abSuccess) {
	    status = S_db_badField;
    	    recGblRecordError(status,(void *)prec,"init_record setNbits");
	}
	break;
    default:
	status = S_db_badField;
    	recGblRecordError(status,(void *)prec,"init_record registerCard");
	break;
    }
    return(status);
}

LOCAL long read_mbbi(struct mbbiRecord *prec)
{
	devPvt		*pdevPvt = (devPvt *)prec->dpvt;
	void		*drvPvt;
	abStatus	drvStatus;
	unsigned long	value;

	if(!pdevPvt) {
	    recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
	    return(2);
	}
	drvPvt = pdevPvt->drvPvt;
	drvStatus = (*pabDrv->readBi)(drvPvt,&value,prec->mask);
        if(drvStatus!=abSuccess) {
 		recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
		return(2);
	}
	prec->rval = value;
	return(0);
}
LOCAL long init_mbbi(struct mbbiRecord *prec,abNumBits nBits)
{
    struct abio	*pabio;
    devPvt	*pdevPvt;
    abStatus	drvStatus;
    long	status=0;
    void	*drvPvt;

    if (prec->inp.type != AB_IO){
    	recGblRecordError(S_db_badField,(void *)prec, "Illegal INP field");
    	return(S_db_badField);
    }
    pabio = (struct abio *)&(prec->inp.value);
    prec->shft = prec->inp.value.abio.signal;
    prec->mask <<= prec->shft;
    drvStatus = (*pabDrv->registerCard)(pabio->link,pabio->adapter,
	pabio->card,typeBi,"BINARY",devCallback,&drvPvt);
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
	drvStatus = (*pabDrv->setNbits)(drvPvt,nBits);
	if(drvStatus!=abSuccess) {
	    status = S_db_badField;
    	    recGblRecordError(status,(void *)prec,"init_record setNbits");
	}
	break;
    default:
	status = S_db_badField;
    	recGblRecordError(status,(void *)prec,"init_record registerCard");
	break;
    }
    return(status);
}

LOCAL long read_mbbiDirect(struct mbbiDirectRecord *prec)
{
	devPvt		*pdevPvt = (devPvt *)prec->dpvt;
	void		*drvPvt;
	abStatus	drvStatus;
	unsigned long	value;

	if(!pdevPvt) {
	    recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
	    return(2);
	}
	drvPvt = pdevPvt->drvPvt;
	drvStatus = (*pabDrv->readBi)(drvPvt,&value,prec->mask);
        if(drvStatus!=abSuccess) {
 		recGblSetSevr(prec,READ_ALARM,INVALID_ALARM);
		return(2);
	}
	prec->rval = value;
	return(0);
}
LOCAL long init_mbbiDirect(struct mbbiDirectRecord *prec,abNumBits nBits)
{
    struct abio	*pabio;
    devPvt	*pdevPvt;
    abStatus	drvStatus;
    long	status=0;
    void	*drvPvt;

    if (prec->inp.type != AB_IO){
    	recGblRecordError(S_db_badField,(void *)prec, "Illegal INP field");
    	return(S_db_badField);
    }
    pabio = (struct abio *)&(prec->inp.value);
    prec->shft = prec->inp.value.abio.signal;
    prec->mask <<= prec->shft;
    drvStatus = (*pabDrv->registerCard)(pabio->link,pabio->adapter,
	pabio->card,typeBi,"BINARY",devCallback,&drvPvt);
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
	drvStatus = (*pabDrv->setNbits)(drvPvt,nBits);
	if(drvStatus!=abSuccess) {
	    status = S_db_badField;
    	    recGblRecordError(status,(void *)prec,"init_record setNbits");
	}
	break;
    default:
	status = S_db_badField;
    	recGblRecordError(status,(void *)prec,"init_record registerCard");
	break;
    }
    return(status);
}

LOCAL long write_bo(struct boRecord *prec)
{
	devPvt		*pdevPvt = (devPvt *)prec->dpvt;
	void		*drvPvt;
	abStatus	drvStatus;

	if(!pdevPvt) {
	    recGblSetSevr(prec,WRITE_ALARM,INVALID_ALARM);
	    return(0);
	}
	drvPvt = pdevPvt->drvPvt;
	drvStatus = (*pabDrv->updateBo)(drvPvt,prec->rval,prec->mask);
        if(drvStatus!=abSuccess) {
 		recGblSetSevr(prec,WRITE_ALARM,INVALID_ALARM);
	}
	return(0);
}
LOCAL long init_bo(struct boRecord *prec,abNumBits nBits)
{
    struct abio		*pabio;
    devPvt		*pdevPvt;
    abStatus		drvStatus;
    long		status=0;
    void		*drvPvt;
    unsigned long	value;

    if (prec->out.type != AB_IO){
    	recGblRecordError(S_db_badField,(void *)prec, "Illegal INP field");
    	return(S_db_badField);
    }
    pabio = (struct abio *)&(prec->out.value);
    prec->mask = 1;
    prec->mask <<= pabio->signal;
    drvStatus = (*pabDrv->registerCard)(pabio->link,pabio->adapter,
	pabio->card,typeBo,"BINARY",NULL,&drvPvt);
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
	drvStatus = (*pabDrv->setNbits)(drvPvt,nBits);
	if(drvStatus!=abSuccess) {
    	    recGblRecordError(S_db_badField,(void *)prec,
		"init_record setNbits");
	}
	break;
    default:
	status = S_db_badField;
    	recGblRecordError(status,(void *)prec,"init_record registerCard");
	return(status);
    }
    drvStatus = (*pabDrv->readBo)(drvPvt,&value,prec->mask);
    if(drvStatus==abSuccess) {
	prec->rval = value;
	status = 0;
    } else {
	status = 2;
    }
    return(status);
}

LOCAL long write_mbbo(struct mbboRecord *prec)
{
	devPvt		*pdevPvt = (devPvt *)prec->dpvt;
	void		*drvPvt;
	abStatus	drvStatus;

	if(!pdevPvt) {
	    recGblSetSevr(prec,WRITE_ALARM,INVALID_ALARM);
	    return(0);
	}
	drvPvt = pdevPvt->drvPvt;
	drvStatus = (*pabDrv->updateBo)(drvPvt,prec->rval,prec->mask);
        if(drvStatus!=abSuccess) {
 		recGblSetSevr(prec,WRITE_ALARM,INVALID_ALARM);
	}
	return(0);
}
LOCAL long init_mbbo(struct mbboRecord *prec,abNumBits nBits)
{
    struct abio		*pabio;
    devPvt		*pdevPvt;
    abStatus		drvStatus;
    long		status=0;
    void		*drvPvt;
    unsigned long	value;

    if (prec->out.type != AB_IO){
    	recGblRecordError(S_db_badField,(void *)prec, "Illegal INP field");
    	return(S_db_badField);
    }
    pabio = (struct abio *)&(prec->out.value);
    prec->shft = prec->out.value.abio.signal;
    prec->mask <<= prec->shft;
    drvStatus = (*pabDrv->registerCard)(pabio->link,pabio->adapter,
	pabio->card,typeBo,"BINARY",NULL,&drvPvt);
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
	drvStatus = (*pabDrv->setNbits)(drvPvt,nBits);
	if(drvStatus!=abSuccess) {
    	    recGblRecordError(S_db_badField,(void *)prec,
		"init_record setNbits");
	}
	break;
    default:
	status = S_db_badField;
    	recGblRecordError(status,(void *)prec,"init_record registerCard");
	return(status);
    }
    drvStatus = (*pabDrv->readBo)(drvPvt,&value,prec->mask);
    if(drvStatus==abSuccess) {
	prec->rval = value;
	status = 0;
    } else {
	status = 2;
    }
    return(status);
}

LOCAL long write_mbboDirect(struct mbboDirectRecord *prec)
{
	devPvt		*pdevPvt = (devPvt *)prec->dpvt;
	void		*drvPvt;
	abStatus	drvStatus;
	unsigned long	value = prec->rval;

	if(!pdevPvt) {
	    recGblSetSevr(prec,WRITE_ALARM,INVALID_ALARM);
	    return(0);
	}
	drvPvt = pdevPvt->drvPvt;
	drvStatus = (*pabDrv->updateBo)(drvPvt,value,prec->mask);
        if(drvStatus!=abSuccess) {
 		recGblSetSevr(prec,WRITE_ALARM,INVALID_ALARM);
	}
	return(0);
}
LOCAL long init_mbboDirect(struct mbboDirectRecord *prec,abNumBits nBits)
{
    struct abio		*pabio;
    devPvt		*pdevPvt;
    abStatus		drvStatus;
    long		status=0;
    void		*drvPvt;
    unsigned long	value;

    if (prec->out.type != AB_IO){
    	recGblRecordError(S_db_badField,(void *)prec, "Illegal INP field");
    	return(S_db_badField);
    }
    pabio = (struct abio *)&(prec->out.value);
    prec->shft = prec->out.value.abio.signal;
    prec->mask <<= prec->shft;
    drvStatus = (*pabDrv->registerCard)(pabio->link,pabio->adapter,
	pabio->card,typeBo,"BINARY",NULL,&drvPvt);
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
	drvStatus = (*pabDrv->setNbits)(drvPvt,nBits);
	if(drvStatus!=abSuccess) {
    	    recGblRecordError(S_db_badField,(void *)prec,
		"init_record setNbits");
	}
	break;
    default:
	status = S_db_badField;
    	recGblRecordError(status,(void *)prec,"init_record registerCard");
	return(status);
    }
    drvStatus = (*pabDrv->readBo)(drvPvt,&value,prec->mask);
    if(drvStatus==abSuccess) {
	prec->rval = value;
	status = 0;
    } else {
	status = 2;
    }
    return(status);
}
