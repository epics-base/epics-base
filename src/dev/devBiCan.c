/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    devBiCan.c

Description:
    CANBUS Binary Input device support

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
#include <biRecord.h>
#include <canBus.h>


#define CONVERT 0
#define DO_NOT_CONVERT 2


typedef struct {
    CALLBACK callback;		/* This *must* be first member */
    WDOG_ID wdId;
    IOSCANPVT ioscanpvt;
    struct biRecord *prec;
    canIo_t inp;
    long data;
    int status;
} biCanPrivate_t;

LOCAL long init_bi(struct biRecord *prec);
LOCAL long get_ioint_info(int cmd, struct biRecord *prec, IOSCANPVT *ppvt);
LOCAL long read_bi(struct biRecord *prec);
LOCAL void biProcess(biCanPrivate_t *pcanBi);
LOCAL void biMessage(biCanPrivate_t *pcanBi, canMessage_t *pmessage);
LOCAL void biSignal(biCanPrivate_t *pcanBi, int status);

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_bi;
} devBiCan = {
    5,
    NULL,
    NULL,
    init_bi,
    get_ioint_info,
    read_bi
};


LOCAL long init_bi (
    struct biRecord *prec
) {
    biCanPrivate_t *pcanBi;
    int status;

    if (prec->inp.type != INST_IO) {
	recGblRecordError(S_db_badField, (void *) prec,
			  "devBiCan (init_record) Illegal INP field");
	return S_db_badField;
    }

    pcanBi = (biCanPrivate_t *) malloc(sizeof(biCanPrivate_t));
    if (pcanBi == NULL) {
	return S_dev_noMemory;
    }
    prec->dpvt = pcanBi;
    pcanBi->prec = prec;
    pcanBi->ioscanpvt = NULL;
    pcanBi->status = NO_ALARM;

    /* Convert the address string into members of the canIo structure */
    status = canIoParse(prec->inp.value.instio.string, &pcanBi->inp);
    if (status ||
	pcanBi->inp.parameter < 0 ||
	pcanBi->inp.parameter > 7) {
	recGblRecordError(S_can_badAddress, (void *) prec,
			  "devBiCan (init_record) bad CAN address");
	return S_can_badAddress;
    }

    #ifdef DEBUG
	printf("biCan %s: Init bus=%s, id=%#x, off=%d, parm=%d\n",
		    prec->name, pcanBi->inp.busName, pcanBi->inp.identifier,
		    pcanBi->inp.offset, pcanBi->inp.parameter);
    #endif

    /* For bi records, the final parameter specifies the input bit number,
       with offset specifying the message byte number. */
    prec->mask = 1 << pcanBi->inp.parameter;

    #ifdef DEBUG
	printf("  bit=%d, mask=%#x\n", 
		pcanBi->inp.parameter, prec->mask);
    #endif

    /* Create a callback for asynchronous processing */
    callbackSetCallback(biProcess, &pcanBi->callback);
    callbackSetPriority(prec->prio, &pcanBi->callback);

    /* and a watchdog for CANbus RTR timeouts */
    pcanBi->wdId = wdCreate();
    if (pcanBi->wdId == NULL) {
	return S_dev_noMemory;
    }

    /* Register the message and signal handlers with the Canbus driver */
    canMessage(pcanBi->inp.canBusID, pcanBi->inp.identifier, 
	       (canMsgCallback_t *) biMessage, pcanBi);
    canSignal(pcanBi->inp.canBusID, (canSigCallback_t *) biSignal, pcanBi);

    return OK;
}

LOCAL long get_ioint_info (
    int cmd,
    struct biRecord *prec, 
    IOSCANPVT *ppvt
) {
    biCanPrivate_t *pcanBi = (biCanPrivate_t *) prec->dpvt;

    if (pcanBi->ioscanpvt == NULL) {
	scanIoInit(&pcanBi->ioscanpvt);
    }

    #ifdef DEBUG
	printf("canBi %s: get_ioint_info %d\n", prec->name, cmd);
    #endif

    *ppvt = pcanBi->ioscanpvt;
    return OK;
}

LOCAL long read_bi (
    struct biRecord *prec
) {
    biCanPrivate_t *pcanBi = (biCanPrivate_t *) prec->dpvt;

    if (pcanBi->inp.canBusID == NULL) {
	return DO_NOT_CONVERT;
    }

    #ifdef DEBUG
	printf("canBi %s: read_bi status=%#x\n", prec->name, pcanBi->status);
    #endif

    switch (pcanBi->status) {
	case TIMEOUT_ALARM:
	case COMM_ALARM:
	    recGblSetSevr(prec, pcanBi->status, MAJOR_ALARM);
	    pcanBi->status = NO_ALARM;
	    return DO_NOT_CONVERT;
	case READ_ALARM:
	    recGblSetSevr(prec, COMM_ALARM, MINOR_ALARM);
	    pcanBi->status = NO_ALARM;
	    return DO_NOT_CONVERT;

	case NO_ALARM:
	    if (prec->pact || prec->scan == SCAN_IO_EVENT) {
		#ifdef DEBUG
		    printf("canBi %s: message id=%#x, data=%#x\n", 
			    prec->name, pcanBi->inp.identifier, pcanBi->data);
		#endif

		prec->rval = pcanBi->data & prec->mask;
		return CONVERT;
	    } else {
		canMessage_t message;

		message.identifier = pcanBi->inp.identifier;
		message.rtr = RTR;
		message.length = 0;

		#ifdef DEBUG
		    printf("canBi %s: RTR, id=%#x\n", 
			    prec->name, pcanBi->inp.identifier);
		#endif

		prec->pact = TRUE;
		pcanBi->status = TIMEOUT_ALARM;

		callbackSetPriority(prec->prio, &pcanBi->callback);
		wdStart(pcanBi->wdId, pcanBi->inp.timeout, 
			(FUNCPTR) callbackRequest, (int) pcanBi);
		canWrite(pcanBi->inp.canBusID, &message, pcanBi->inp.timeout);
		return 0;
	    }
	default:
	    recGblSetSevr(prec, UDF_ALARM, INVALID_ALARM);
	    pcanBi->status = NO_ALARM;
	    return DO_NOT_CONVERT;
    }
}

LOCAL void biProcess (
    biCanPrivate_t *pcanBi
) {
    dbScanLock((struct dbCommon *) pcanBi->prec);
    (*((struct rset *) pcanBi->prec->rset)->process)(pcanBi->prec);
    dbScanUnlock((struct dbCommon *) pcanBi->prec);
}

LOCAL void biMessage (
    biCanPrivate_t *pcanBi,
    canMessage_t *pmessage
) {
    if (pmessage->rtr == RTR) {
	return;		/* Ignore RTRs */
    }

    pcanBi->data = pmessage->data[pcanBi->inp.offset];

    if (pcanBi->prec->scan == SCAN_IO_EVENT) {
	pcanBi->status = NO_ALARM;
	scanIoRequest(pcanBi->ioscanpvt);
    } else if (pcanBi->status == TIMEOUT_ALARM) {
	pcanBi->status = NO_ALARM;
	wdCancel(pcanBi->wdId);
	callbackRequest(&pcanBi->callback);
    }
}

LOCAL void biSignal (
    biCanPrivate_t *pcanBi,
    int status
) {
    switch(status) {
	case CAN_BUS_OK:
	    return;
	case CAN_BUS_ERROR:
	    pcanBi->status = READ_ALARM;
	    break;
	case CAN_BUS_OFF:
	    pcanBi->status = COMM_ALARM;
	    break;
    }
    callbackRequest(&pcanBi->callback);
}
