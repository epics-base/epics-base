/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    devMbbiDirectCan.c

Description:
    CANBUS Multi-Bit Binary Input Direct device support

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
#include <mbbiDirectRecord.h>
#include <canBus.h>


#define CONVERT 0
#define DO_NOT_CONVERT 2


typedef struct {
    CALLBACK callback;		/* This *must* be first member */
    WDOG_ID wdId;
    IOSCANPVT ioscanpvt;
    struct mbbiDirectRecord *prec;
    canIo_t inp;
    long data;
    int status;
} mbbiDirectCanPrivate_t;

LOCAL long init_mbbiDirect(struct mbbiDirectRecord *prec);
LOCAL long get_ioint_info(int cmd, struct mbbiDirectRecord *prec, IOSCANPVT *ppvt);
LOCAL long read_mbbiDirect(struct mbbiDirectRecord *prec);
LOCAL void mbbiDirectProcess(mbbiDirectCanPrivate_t *pcanMbbiDirect);
LOCAL void mbbiDirectMessage(mbbiDirectCanPrivate_t *pcanMbbiDirect, canMessage_t *pmessage);
LOCAL void mbbiDirectSignal(mbbiDirectCanPrivate_t *pcanMbbiDirect, int status);

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_mbbiDirect;
} devMbbiDirectCan = {
    5,
    NULL,
    NULL,
    init_mbbiDirect,
    get_ioint_info,
    read_mbbiDirect
};


LOCAL long init_mbbiDirect (
    struct mbbiDirectRecord *prec
) {
    mbbiDirectCanPrivate_t *pcanMbbiDirect;
    int status;

    if (prec->inp.type != INST_IO) {
	recGblRecordError(S_db_badField, (void *) prec,
			  "devMbbiDirectCan (init_record) Illegal INP field");
	return S_db_badField;
    }

    pcanMbbiDirect = (mbbiDirectCanPrivate_t *) malloc(sizeof(mbbiDirectCanPrivate_t));
    if (pcanMbbiDirect == NULL) {
	return S_dev_noMemory;
    }
    prec->dpvt = pcanMbbiDirect;
    pcanMbbiDirect->prec = prec;
    pcanMbbiDirect->ioscanpvt = NULL;
    pcanMbbiDirect->status = NO_ALARM;

    /* Convert the address string into members of the canIo structure */
    status = canIoParse(prec->inp.value.instio.string, &pcanMbbiDirect->inp);
    if (status ||
	pcanMbbiDirect->inp.parameter < 0 ||
	pcanMbbiDirect->inp.parameter > 7) {
	recGblRecordError(S_can_badAddress, (void *) prec,
			  "devMbbiDirectCan (init_record) bad CAN address");
	return S_can_badAddress;
    }

    #ifdef DEBUG
	printf("mbbiDirectCan %s: Init bus=%s, id=%#x, off=%d, parm=%d\n",
		    prec->name, pcanMbbiDirect->inp.busName, pcanMbbiDirect->inp.identifier,
		    pcanMbbiDirect->inp.offset, pcanMbbiDirect->inp.parameter);
    #endif

    /* For mbbiDirect records, the final parameter specifies the input bit shift,
       with offset specifying the message byte number. */
    prec->shft = pcanMbbiDirect->inp.parameter;
    prec->mask <<= pcanMbbiDirect->inp.parameter;

    #ifdef DEBUG
	printf("  shft=%d, mask=%#x\n", 
		pcanMbbiDirect->inp.parameter, prec->mask);
    #endif

    /* Create a callback for asynchronous processing */
    callbackSetCallback(mbbiDirectProcess, &pcanMbbiDirect->callback);
    callbackSetPriority(prec->prio, &pcanMbbiDirect->callback);

    /* and a watchdog for CANbus RTR timeouts */
    pcanMbbiDirect->wdId = wdCreate();
    if (pcanMbbiDirect->wdId == NULL) {
	return S_dev_noMemory;
    }

    /* Register the message and signal handlers with the Canbus driver */
    canMessage(pcanMbbiDirect->inp.canBusID, pcanMbbiDirect->inp.identifier, 
	       (canMsgCallback_t *) mbbiDirectMessage, pcanMbbiDirect);
    canSignal(pcanMbbiDirect->inp.canBusID, 
	      (canSigCallback_t *) mbbiDirectSignal, pcanMbbiDirect);

    return OK;
}

LOCAL long get_ioint_info (
    int cmd,
    struct mbbiDirectRecord *prec, 
    IOSCANPVT *ppvt
) {
    mbbiDirectCanPrivate_t *pcanMbbiDirect = (mbbiDirectCanPrivate_t *) prec->dpvt;

    if (pcanMbbiDirect->ioscanpvt == NULL) {
	scanIoInit(&pcanMbbiDirect->ioscanpvt);
    }

    #ifdef DEBUG
	printf("canMbbiDirect %s: get_ioint_info %d\n", prec->name, cmd);
    #endif

    *ppvt = pcanMbbiDirect->ioscanpvt;
    return OK;
}

LOCAL long read_mbbiDirect (
    struct mbbiDirectRecord *prec
) {
    mbbiDirectCanPrivate_t *pcanMbbiDirect = (mbbiDirectCanPrivate_t *) prec->dpvt;

    if (pcanMbbiDirect->inp.canBusID == NULL) {
	return DO_NOT_CONVERT;
    }

    #ifdef DEBUG
	printf("canMbbiDirect %s: read_mbbiDirect status=%#x\n", prec->name, pcanMbbiDirect->status);
    #endif

    switch (pcanMbbiDirect->status) {
	case TIMEOUT_ALARM:
	case COMM_ALARM:
	    recGblSetSevr(prec, pcanMbbiDirect->status, MAJOR_ALARM);
	    pcanMbbiDirect->status = NO_ALARM;
	    return DO_NOT_CONVERT;
	case READ_ALARM:
	    recGblSetSevr(prec, COMM_ALARM, MINOR_ALARM);
	    pcanMbbiDirect->status = NO_ALARM;
	    return DO_NOT_CONVERT;

	case NO_ALARM:
	    if (prec->pact || prec->scan == SCAN_IO_EVENT) {
		#ifdef DEBUG
		    printf("canMbbiDirect %s: message id=%#x, data=%#x\n", 
			    prec->name, pcanMbbiDirect->inp.identifier, pcanMbbiDirect->data);
		#endif

		prec->rval = pcanMbbiDirect->data & prec->mask;
		return CONVERT;
	    } else {
		canMessage_t message;

		message.identifier = pcanMbbiDirect->inp.identifier;
		message.rtr = RTR;
		message.length = 0;

		#ifdef DEBUG
		    printf("canMbbiDirect %s: RTR, id=%#x\n", 
			    prec->name, pcanMbbiDirect->inp.identifier);
		#endif

		prec->pact = TRUE;
		pcanMbbiDirect->status = TIMEOUT_ALARM;

		callbackSetPriority(prec->prio, &pcanMbbiDirect->callback);
		wdStart(pcanMbbiDirect->wdId, pcanMbbiDirect->inp.timeout, 
			(FUNCPTR) callbackRequest, (int) pcanMbbiDirect);
		canWrite(pcanMbbiDirect->inp.canBusID, &message, pcanMbbiDirect->inp.timeout);
		return 0;
	    }
	default:
	    recGblSetSevr(prec, UDF_ALARM, INVALID_ALARM);
	    pcanMbbiDirect->status = NO_ALARM;
	    return DO_NOT_CONVERT;
    }
}

LOCAL void mbbiDirectProcess (
    mbbiDirectCanPrivate_t *pcanMbbiDirect
) {
    dbScanLock((struct dbCommon *) pcanMbbiDirect->prec);
    (*((struct rset *) pcanMbbiDirect->prec->rset)->process)(pcanMbbiDirect->prec);
    dbScanUnlock((struct dbCommon *) pcanMbbiDirect->prec);
}

LOCAL void mbbiDirectMessage (
    mbbiDirectCanPrivate_t *pcanMbbiDirect,
    canMessage_t *pmessage
) {
    if (pmessage->rtr == RTR) {
	return;		/* Ignore RTRs */
    }

    pcanMbbiDirect->data = pmessage->data[pcanMbbiDirect->inp.offset];

    if (pcanMbbiDirect->prec->scan == SCAN_IO_EVENT) {
	pcanMbbiDirect->status = NO_ALARM;
	scanIoRequest(pcanMbbiDirect->ioscanpvt);
    } else if (pcanMbbiDirect->status == TIMEOUT_ALARM) {
	pcanMbbiDirect->status = NO_ALARM;
	wdCancel(pcanMbbiDirect->wdId);
	callbackRequest(&pcanMbbiDirect->callback);
    }
}

LOCAL void mbbiDirectSignal (
    mbbiDirectCanPrivate_t *pcanMbbiDirect,
    int status
) {
    switch(status) {
	case CAN_BUS_OK:
	    return;
	case CAN_BUS_ERROR:
	    pcanMbbiDirect->status = READ_ALARM;
	    break;
	case CAN_BUS_OFF:
	    pcanMbbiDirect->status = COMM_ALARM;
	    break;
    }
    callbackRequest(&pcanMbbiDirect->callback);
}
