/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    devAiCan.c

Description:
    CANBUS Analogue Input device support

Author:
    Andrew Johnson
Created:
    8 August 1995

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
#include <aiRecord.h>
#include <canBus.h>


#define CONVERT 0
#define DO_NOT_CONVERT 2


typedef struct {
    CALLBACK callback;		/* This *must* be first member */
    WDOG_ID wdId;
    IOSCANPVT ioscanpvt;
    struct aiRecord *prec;
    canIo_t inp;
    ulong_t mask;
    ulong_t sign;
    long data;
    int status;
} aiCanPrivate_t;

LOCAL long init_ai(struct aiRecord *prec);
LOCAL long get_ioint_info(int cmd, struct aiRecord *prec, IOSCANPVT *ppvt);
LOCAL long read_ai(struct aiRecord *prec);
LOCAL long special_linconv(struct aiRecord *prec, int after);
LOCAL void aiProcess(aiCanPrivate_t *pcanAi);
LOCAL void aiMessage(aiCanPrivate_t *pcanAi, canMessage_t *pmessage);
LOCAL void aiSignal(aiCanPrivate_t *pcanAi, int status);

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_ai;
    DEVSUPFUN special_linconv;
} devAiCan = {
    6,
    NULL,
    NULL,
    init_ai,
    get_ioint_info,
    read_ai,
    special_linconv
};


LOCAL long init_ai (
    struct aiRecord *prec
) {
    aiCanPrivate_t *pcanAi;
    int status;
    ulong_t fsd;

    if (prec->inp.type != INST_IO) {
	recGblRecordError(S_db_badField, (void *) prec,
			  "devAiCan (init_record) Illegal INP field");
	return S_db_badField;
    }

    pcanAi = (aiCanPrivate_t *) malloc(sizeof(aiCanPrivate_t));
    if (pcanAi == NULL) {
	return S_dev_noMemory;
    }
    prec->dpvt = pcanAi;
    pcanAi->prec = prec;
    pcanAi->ioscanpvt = NULL;
    pcanAi->status = NO_ALARM;

    /* Convert the address string into members of the canIo structure */
    status = canIoParse(prec->inp.value.instio.string, &pcanAi->inp);
    if (status) {
	recGblRecordError(S_can_badAddress, (void *) prec,
			  "devAiCan (init_record) bad CAN address");
	return S_can_badAddress;
    }

    #ifdef DEBUG
	printf("aiCan %s: Init bus=%s, id=%#x, off=%d, parm=%d\n",
		    prec->name, pcanAi->inp.busName, pcanAi->inp.identifier,
		    pcanAi->inp.offset, pcanAi->inp.parameter);
    #endif

    /* For ai records, the final parameter specifies the raw input size.
	eg 0xff or 0x100 specify an 8-bit unsigned value. -ve numbers 
	specify a signed value, eg -4095 means a 12-bit signed value.
	The range does not have to be a power of two, eg 99 is legal. */

    fsd = abs(pcanAi->inp.parameter);
    if (fsd > 0) {
	if ((fsd & (fsd-1)) == 0) {
	    fsd--;
	}

	/* Make a mask to contain only the valid input bits based on fsd */
	pcanAi->mask = 1;
	while (pcanAi->mask < fsd) {
	    pcanAi->mask <<= 1;
	}
	pcanAi->mask--;

	if (pcanAi->inp.parameter < 0) {
	    /* signed: rval = sign-extend(data & mask) */
	    pcanAi->sign = (pcanAi->mask >> 1) + 1;
	} else {
	    /* unsigned: rval = data & mask */
	    pcanAi->sign = 0;
	}

	if (prec->linr == 1) {
	    prec->roff = pcanAi->sign;
	    prec->eslo = (prec->eguf - prec->egul) / fsd;
	} else {
	    prec->roff = 0;
	}
    } else {
	pcanAi->mask = pcanAi->sign = 0;
    }

    #ifdef DEBUG
	printf("  fsd=%d, eslo=%g, roff = %d, mask=%#x, sign=%d\n", 
		fsd, prec->eslo, prec->roff, pcanAi->mask, pcanAi->sign);
    #endif

    /* Create a callback for asynchronous processing */
    callbackSetCallback(aiProcess, &pcanAi->callback);
    callbackSetPriority(prec->prio, &pcanAi->callback);

    /* and a watchdog for CANbus RTR timeouts */
    pcanAi->wdId = wdCreate();
    if (pcanAi->wdId == NULL) {
	return S_dev_noMemory;
    }

    /* Register the message and signal handlers with the Canbus driver */
    canMessage(pcanAi->inp.canBusID, pcanAi->inp.identifier, 
	       (canMsgCallback_t *) aiMessage, pcanAi);
    canSignal(pcanAi->inp.canBusID, (canSigCallback_t *) aiSignal, pcanAi);

    return OK;
}

LOCAL long get_ioint_info (
    int cmd,
    struct aiRecord *prec, 
    IOSCANPVT *ppvt
) {
    aiCanPrivate_t *pcanAi = (aiCanPrivate_t *) prec->dpvt;

    if (pcanAi->ioscanpvt == NULL) {
	scanIoInit(&pcanAi->ioscanpvt);
    }

    #ifdef DEBUG
	printf("canAi %s: get_ioint_info %d\n", prec->name, cmd);
    #endif

    *ppvt = pcanAi->ioscanpvt;
    return OK;
}

LOCAL long read_ai (
    struct aiRecord *prec
) {
    aiCanPrivate_t *pcanAi = (aiCanPrivate_t *) prec->dpvt;

    if (pcanAi->inp.canBusID == NULL) {
	return DO_NOT_CONVERT;
    }

    #ifdef DEBUG
	printf("canAi %s: read_ai status=%#x\n", prec->name, pcanAi->status);
    #endif

    switch (pcanAi->status) {
	case TIMEOUT_ALARM:
	case COMM_ALARM:
	    recGblSetSevr(prec, pcanAi->status, MAJOR_ALARM);
	    pcanAi->status = NO_ALARM;
	    return DO_NOT_CONVERT;
	case READ_ALARM:
	    recGblSetSevr(prec, COMM_ALARM, MINOR_ALARM);
	    pcanAi->status = NO_ALARM;
	    return DO_NOT_CONVERT;

	case NO_ALARM:
	    if (prec->pact || prec->scan == SCAN_IO_EVENT) {
		#ifdef DEBUG
		    printf("canAi %s: message id=%#x, data=%#x\n", 
			    prec->name, pcanAi->inp.identifier, pcanAi->data);
		#endif

		prec->rval = pcanAi->data & pcanAi->mask;
		if (pcanAi->sign & prec->rval) {
		    prec->rval |= ~pcanAi->mask;
		}
		return CONVERT;
	    } else {
		canMessage_t message;

		message.identifier = pcanAi->inp.identifier;
		message.rtr = RTR;
		message.length = 0;

		#ifdef DEBUG
		    printf("canAi %s: RTR, id=%#x\n", 
			    prec->name, pcanAi->inp.identifier);
		#endif

		prec->pact = TRUE;
		pcanAi->status = TIMEOUT_ALARM;

		callbackSetPriority(prec->prio, &pcanAi->callback);
		wdStart(pcanAi->wdId, pcanAi->inp.timeout, 
			(FUNCPTR) callbackRequest, (int) pcanAi);
		canWrite(pcanAi->inp.canBusID, &message, pcanAi->inp.timeout);
		return 0;
	    }
	default:
	    recGblSetSevr(prec, UDF_ALARM, INVALID_ALARM);
	    pcanAi->status = NO_ALARM;
	    return DO_NOT_CONVERT;
    }
}

LOCAL long special_linconv (
    struct aiRecord *prec,
    int after
) {
    if (after) {
        if (prec->linr == 1) {
	    ulong_t fsd;
	    aiCanPrivate_t *pcanAi = (aiCanPrivate_t *) prec->dpvt;

	    fsd = abs(pcanAi->inp.parameter);
	    if (fsd > 0) {
		if ((fsd & (fsd-1)) == 0) {
		    fsd--;
		}
		prec->roff = pcanAi->sign;
		prec->eslo = (prec->eguf - prec->egul) / fsd;
	    }
	} else {
	    prec->roff = 0;
	}
    }
    return 0;
}

LOCAL void aiProcess (
    aiCanPrivate_t *pcanAi
) {
    dbScanLock((struct dbCommon *) pcanAi->prec);
    (*((struct rset *) pcanAi->prec->rset)->process)(pcanAi->prec);
    dbScanUnlock((struct dbCommon *) pcanAi->prec);
}

LOCAL void aiMessage (
    aiCanPrivate_t *pcanAi,
    canMessage_t *pmessage
) {
    if (pmessage->rtr == RTR) {
	return;		/* Ignore RTRs */
    }

    if (pcanAi->mask == 0) {
	pcanAi->data = 0;
    } else if (pcanAi->mask <= 0xff) {
	pcanAi->data = pmessage->data[pcanAi->inp.offset+0];
    } else if (pcanAi->mask <= 0xffff) {
	pcanAi->data = pmessage->data[pcanAi->inp.offset+0] <<  8 |
		       pmessage->data[pcanAi->inp.offset+1];
    } else if (pcanAi->mask <= 0xffffff) {
	pcanAi->data = pmessage->data[pcanAi->inp.offset+0] << 16 |
		       pmessage->data[pcanAi->inp.offset+1] <<  8 |
		       pmessage->data[pcanAi->inp.offset+2];
    } else {
	pcanAi->data = pmessage->data[pcanAi->inp.offset+0] << 24 |
		       pmessage->data[pcanAi->inp.offset+1] << 16 |
		       pmessage->data[pcanAi->inp.offset+2] <<  8 |
		       pmessage->data[pcanAi->inp.offset+3];
    }

    if (pcanAi->prec->scan == SCAN_IO_EVENT) {
	pcanAi->status = NO_ALARM;
	scanIoRequest(pcanAi->ioscanpvt);
    } else if (pcanAi->status == TIMEOUT_ALARM) {
	pcanAi->status = NO_ALARM;
	wdCancel(pcanAi->wdId);
	callbackRequest(&pcanAi->callback);
    }
}

LOCAL void aiSignal (
    aiCanPrivate_t *pcanAi,
    int status
) {
    switch(status) {
	case CAN_BUS_OK:
	    return;
	case CAN_BUS_ERROR:
	    pcanAi->status = READ_ALARM;
	    break;
	case CAN_BUS_OFF:
	    pcanAi->status = COMM_ALARM;
	    break;
    }
    callbackRequest(&pcanAi->callback);
}
