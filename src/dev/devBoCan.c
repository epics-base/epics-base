/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    devBoCan.c

Description:
    CANBUS Binary Output device support

Author:
    Andrew Johnson
Created:
    14 August 1995

(c) 1995 Royal Greenwich Observatory

*******************************************************************************/


#include <vxWorks.h>
#include <stdlib.h>
#include <wdLib.h>

#include <errMdef.h>
#include <devLib.h>
#include <dbAccess.h>
#include <dbScan.h>
#include <callback.h>
#include <cvtTable.h>
#include <link.h>
#include <recSup.h>
#include <devSup.h>
#include <dbCommon.h>
#include <boRecord.h>
#include <canBus.h>


typedef struct {
    CALLBACK callback;		/* This *must* be first member */
    IOSCANPVT ioscanpvt;
    struct boRecord *prec;
    canIo_t out;
    long data;
    int status;
} boCanPrivate_t;

LOCAL long init_bo(struct boRecord *prec);
LOCAL long get_ioint_info(int cmd, struct boRecord *prec, IOSCANPVT *ppvt);
LOCAL long write_bo(struct boRecord *prec);
LOCAL void boProcess(boCanPrivate_t *pcanBo);
LOCAL void boMessage(boCanPrivate_t *pcanBo, canMessage_t *pmessage);
LOCAL void boSignal(boCanPrivate_t *pcanBo, int status);

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write_bo;
} devBoCan = {
    5,
    NULL,
    NULL,
    init_bo,
    get_ioint_info,
    write_bo
};


LOCAL long init_bo (
    struct boRecord *prec
) {
    boCanPrivate_t *pcanBo;
    int status;

    if (prec->out.type != INST_IO) {
	recGblRecordError(S_db_badField, (void *) prec,
			  "devBoCan (init_record) Illegal INP field");
	return S_db_badField;
    }

    pcanBo = (boCanPrivate_t *) malloc(sizeof(boCanPrivate_t));
    if (pcanBo == NULL) {
	return S_dev_noMemory;
    }
    prec->dpvt = pcanBo;
    pcanBo->prec = prec;
    pcanBo->ioscanpvt = NULL;
    pcanBo->status = NO_ALARM;

    /* Convert the parameter string into members of the canIo structure */
    status = canIoParse(prec->out.value.instio.string, &pcanBo->out);
    if (status ||
	pcanBo->out.parameter < 0 ||
	pcanBo->out.parameter > 7) {
	recGblRecordError(S_can_badAddress, (void *) prec,
			  "devBoCan (init_record) bad CAN address");
	return S_can_badAddress;
    }

    #ifdef DEBUG
	printf("canBo %s: Init bus=%s, id=%#x, off=%d, parm=%d\n",
		    prec->name, pcanBo->out.busName, pcanBo->out.identifier,
		    pcanBo->out.offset, pcanBo->out.parameter);
    #endif

    /* For bo records, the final parameter specifies the output bit number,
       with the offset specifying the message byte number. */
    prec->mask = 1 << pcanBo->out.parameter;

    #ifdef DEBUG
	printf("  bit=%d, mask=%#x\n", out.parameter, prec->mask);
    #endif

    /* Create a callback for error processing */
    callbackSetCallback(boProcess, &pcanBo->callback);
    callbackSetPriority(prec->prio, &pcanBo->callback);

    /* Register the message and signal handlers with the Canbus driver */
    canMessage(pcanBo->out.canBusID, pcanBo->out.identifier, 
	       (canMsgCallback_t *) boMessage, pcanBo);
    canSignal(pcanBo->out.canBusID, (canSigCallback_t *) boSignal, pcanBo);

    return OK;
}

LOCAL long get_ioint_info (
    int cmd,
    struct boRecord *prec, 
    IOSCANPVT *ppvt
) {
    boCanPrivate_t *pcanBo = (boCanPrivate_t *) prec->dpvt;

    if (pcanBo->ioscanpvt == NULL) {
	scanIoInit(&pcanBo->ioscanpvt);
    }

    #ifdef DEBUG
	printf("boCan %s: get_ioint_info %d\n", prec->name, cmd);
    #endif

    *ppvt = pcanBo->ioscanpvt;
    return OK;
}

LOCAL long write_bo (
    struct boRecord *prec
) {
    boCanPrivate_t *pcanBo = (boCanPrivate_t *) prec->dpvt;

    if (pcanBo->out.canBusID == NULL) {
	return ERROR;
    }

    #ifdef DEBUG
	printf("boCan %s: write_bo status=%#x\n", prec->name, pcanBo->status);
    #endif

    switch (pcanBo->status) {
	case COMM_ALARM:
	    recGblSetSevr(prec, pcanBo->status, MAJOR_ALARM);
	    pcanBo->status = NO_ALARM;
	    return ERROR;
	case READ_ALARM:
	    recGblSetSevr(prec, COMM_ALARM, MINOR_ALARM);
	    pcanBo->status = NO_ALARM;
	    return ERROR;

	case NO_ALARM:
	    {
		canMessage_t message;
		int status;

		message.identifier = pcanBo->out.identifier;
		message.rtr = SEND;

		pcanBo->data = prec->rval & prec->mask;

		message.data[pcanBo->out.offset] = pcanBo->data;
		message.length  = pcanBo->out.offset + 1;

		#ifdef DEBUG
		    printf("canBo %s: SEND id=%#x, length=%d, data=%#x\n", 
			    prec->name, message.identifier, message.length, 
			    pcanBo->data);
		#endif

		status = canWrite(pcanBo->out.canBusID, &message, 
				  pcanBo->out.timeout);
		if (status) {
		    #ifdef DEBUG
			printf("canBo %s: canWrite status=%#x\n", status);
		    #endif

		    recGblSetSevr(prec, TIMEOUT_ALARM, MINOR_ALARM);
		    return ERROR;
		}
		return 0;
	    }
	default:
	    recGblSetSevr(prec, UDF_ALARM, INVALID_ALARM);
	    pcanBo->status = NO_ALARM;
	    return ERROR;
    }
}

LOCAL void boProcess (
    boCanPrivate_t *pcanBo
) {
    dbScanLock((struct dbCommon *) pcanBo->prec);
    (*((struct rset *) pcanBo->prec->rset)->process)(pcanBo->prec);
    dbScanUnlock((struct dbCommon *) pcanBo->prec);
}

LOCAL void boMessage (
    boCanPrivate_t *pcanBo,
    canMessage_t *pmessage
) {
    if (pcanBo->prec->scan == SCAN_IO_EVENT &&
	pmessage->rtr == RTR) {
	pcanBo->status = NO_ALARM;
	scanIoRequest(pcanBo->ioscanpvt);
    }
}

LOCAL void boSignal (
    boCanPrivate_t *pcanBo,
    int status
) {
    switch(status) {
	case CAN_BUS_OK:
	    return;
	case CAN_BUS_ERROR:
	    pcanBo->status = READ_ALARM;
	    break;
	case CAN_BUS_OFF:
	    pcanBo->status = COMM_ALARM;
	    break;
    }
    callbackRequest(&pcanBo->callback);
}
