/*******************************************************************************

Project:
    Gemini/UKIRT CAN Bus Driver for EPICS

File:
    devAoCan.c

Description:
    CANBUS Analogue Output device support

Author:
    Andrew Johnson
Created:
    9 August 1995

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
#include <aoRecord.h>
#include <canBus.h>


typedef struct {
    CALLBACK callback;		/* This *must* be first member */
    IOSCANPVT ioscanpvt;
    struct aoRecord *prec;
    canIo_t out;
    ulong_t mask;
    ulong_t sign;
    long data;
    int status;
} aoCanPrivate_t;

LOCAL long init_ao(struct aoRecord *prec);
LOCAL long get_ioint_info(int cmd, struct aoRecord *prec, IOSCANPVT *ppvt);
LOCAL long write_ao(struct aoRecord *prec);
LOCAL long special_linconv(struct aoRecord *prec, int after);
LOCAL void aoProcess(aoCanPrivate_t *pcanAo);
LOCAL void aoMessage(aoCanPrivate_t *pcanAo, canMessage_t *pmessage);
LOCAL void aoSignal(aoCanPrivate_t *pcanAo, int status);

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write_ao;
    DEVSUPFUN special_linconv;
} devAoCan = {
    6,
    NULL,
    NULL,
    init_ao,
    get_ioint_info,
    write_ao,
    special_linconv
};


LOCAL long init_ao (
    struct aoRecord *prec
) {
    aoCanPrivate_t *pcanAo;
    int status, fsd;

    if (prec->out.type != INST_IO) {
	recGblRecordError(S_db_badField, (void *) prec,
			  "devAoCan (init_record) Illegal INP field");
	return S_db_badField;
    }

    pcanAo = (aoCanPrivate_t *) malloc(sizeof(aoCanPrivate_t));
    if (pcanAo == NULL) {
	return S_dev_noMemory;
    }
    prec->dpvt = pcanAo;
    pcanAo->prec = prec;
    pcanAo->ioscanpvt = NULL;
    pcanAo->status = NO_ALARM;

    /* Convert the parameter string into members of the canIo structure */
    status = canIoParse(prec->out.value.instio.string, &pcanAo->out);
    if (status) {
	recGblRecordError(S_can_badAddress, (void *) prec,
			  "devAoCan (init_record) bad CAN address");
	return S_can_badAddress;
    }

    #ifdef DEBUG
	printf("canAo %s: Init bus=%s, id=%#x, off=%d, parm=%d\n",
		    prec->name, pcanAo->out.busName, pcanAo->out.identifier,
		    pcanAo->out.offset, pcanAo->out.parameter);
    #endif

    /* For ao records, the final parameter specifies the raw output size. 
       eg 0xfff or 0x1000 specify a 12-bit unsigned value.  -ve numbers
       specify a signed value, eg -256 means an 8-bit signed value. */
    fsd = abs(pcanAo->out.parameter);
    if (fsd > 0) {
	if ((fsd & (fsd-1)) == 0) {
	    fsd--;
	}

	/* Make a mask to contain only the valid out bits based on fsd */
	pcanAo->mask = 1;
	while (pcanAo->mask < fsd) {
	    pcanAo->mask <<= 1;
	}
	pcanAo->mask--;

	if (pcanAo->out.parameter < 0) {
	    /* signed data */
	    pcanAo->sign = (pcanAo->mask >> 1) + 1;
	} else {
	    pcanAo->sign = 0;
	}
	if (prec->linr == 1) {
	    prec->roff = pcanAo->sign;
	    prec->eslo = (prec->eguf - prec->egul) / fsd;
	} else {
	    prec->roff = 0;
	}
    } else {
	pcanAo->mask = pcanAo->sign = 0;
    }

    #ifdef DEBUG
	printf("  fsd=%d, eslo=%g, roff=%d, mask=%#x, sign=%d\n", 
		fsd, prec->eslo, prec->roff, pcanAo->mask, pcanAo->sign);
    #endif

    /* Create a callback for error processing */
    callbackSetCallback(aoProcess, &pcanAo->callback);
    callbackSetPriority(prec->prio, &pcanAo->callback);

    /* Register the message and signal handlers with the Canbus driver */
    canMessage(pcanAo->out.canBusID, pcanAo->out.identifier, 
	       (canMsgCallback_t *) aoMessage, pcanAo);
    canSignal(pcanAo->out.canBusID, (canSigCallback_t *) aoSignal, pcanAo);

    return OK;
}

LOCAL long get_ioint_info (
    int cmd,
    struct aoRecord *prec, 
    IOSCANPVT *ppvt
) {
    aoCanPrivate_t *pcanAo = (aoCanPrivate_t *) prec->dpvt;

    if (pcanAo->ioscanpvt == NULL) {
	scanIoInit(&pcanAo->ioscanpvt);
    }

    #ifdef DEBUG
	printf("aoCan %s: get_ioint_info %d\n", prec->name, cmd);
    #endif

    *ppvt = pcanAo->ioscanpvt;
    return OK;
}

LOCAL long write_ao (
    struct aoRecord *prec
) {
    aoCanPrivate_t *pcanAo = (aoCanPrivate_t *) prec->dpvt;

    if (pcanAo->out.canBusID == NULL) {
	return ERROR;
    }

    #ifdef DEBUG
	printf("aoCan %s: write_ao status=%#x\n", prec->name, pcanAo->status);
    #endif

    switch (pcanAo->status) {
	case COMM_ALARM:
	    recGblSetSevr(prec, pcanAo->status, MAJOR_ALARM);
	    pcanAo->status = NO_ALARM;
	    return ERROR;
	case READ_ALARM:
	    recGblSetSevr(prec, COMM_ALARM, MINOR_ALARM);
	    pcanAo->status = NO_ALARM;
	    return ERROR;

	case NO_ALARM:
	    {
		canMessage_t message;
		int status;

		message.identifier = pcanAo->out.identifier;
		message.rtr = SEND;

		pcanAo->data = prec->rval & pcanAo->mask;

		if (pcanAo->mask == 0) {
		    message.length  = 0;
		} else if (pcanAo->mask <= 0xff) {
		    message.data[0] = (pcanAo->data      ) & 0xff;
		    message.length  = 1;
		} else if (pcanAo->mask <= 0xffff) {
		    message.data[0] = (pcanAo->data >>  8) & 0xff;
		    message.data[1] = (pcanAo->data      ) & 0xff;
		    message.length  = 2;
		} else if (pcanAo->mask <= 0xffffff) {
		    message.data[0] = (pcanAo->data >> 16) & 0xff;
		    message.data[1] = (pcanAo->data >>  8) & 0xff;
		    message.data[2] = (pcanAo->data      ) & 0xff;
		    message.length  = 3;
		} else {
		    message.data[0] = (pcanAo->data >> 24) & 0xff;
		    message.data[1] = (pcanAo->data >> 16) & 0xff;
		    message.data[2] = (pcanAo->data >>  8) & 0xff;
		    message.data[3] = (pcanAo->data      ) & 0xff;
		    message.length  = 4;
		}

		#ifdef DEBUG
		    printf("canAo %s: SEND id=%#x, length=%d, data=%#x\n", 
			    prec->name, message.identifier, message.length, 
			    pcanAo->data);
		#endif

		status = canWrite(pcanAo->out.canBusID, &message, 
				  pcanAo->out.timeout);
		if (status) {
		    #ifdef DEBUG
			printf("canAo %s: canWrite status=%#x\n", status);
		    #endif

		    recGblSetSevr(prec, TIMEOUT_ALARM, MINOR_ALARM);
		    return ERROR;
		}
		return 0;
	    }
	default:
	    recGblSetSevr(prec, UDF_ALARM, INVALID_ALARM);
	    pcanAo->status = NO_ALARM;
	    return ERROR;
    }
}

LOCAL long special_linconv (
    struct aoRecord *prec,
    int after
) {
    if (after) {
	if (prec->linr == 1) {
	    ulong_t fsd;
	    aoCanPrivate_t *pcanAo = (aoCanPrivate_t *) prec->dpvt;
	    fsd = abs(pcanAo->out.parameter);
	    if (fsd > 0) {
		if ((fsd & (fsd-1)) == 0) {
		    fsd--;
		}

		prec->roff = pcanAo->sign;
		prec->eslo = (prec->eguf - prec->egul) / fsd;
	    }
	} else {
	    prec->roff = 0;
	}
    }
    return 0;
}

LOCAL void aoProcess (
    aoCanPrivate_t *pcanAo
) {
    dbScanLock((struct dbCommon *) pcanAo->prec);
    (*((struct rset *) pcanAo->prec->rset)->process)(pcanAo->prec);
    dbScanUnlock((struct dbCommon *) pcanAo->prec);
}

LOCAL void aoMessage (
    aoCanPrivate_t *pcanAo,
    canMessage_t *pmessage
) {
    if (pcanAo->prec->scan == SCAN_IO_EVENT &&
	pmessage->rtr == RTR) {
	pcanAo->status = NO_ALARM;
	scanIoRequest(pcanAo->ioscanpvt);
    }
}

LOCAL void aoSignal (
    aoCanPrivate_t *pcanAo,
    int status
) {
    switch(status) {
	case CAN_BUS_OK:
	    return;
	case CAN_BUS_ERROR:
	    pcanAo->status = READ_ALARM;
	    break;
	case CAN_BUS_OFF:
	    pcanAo->status = COMM_ALARM;
	    break;
    }
    callbackRequest(&pcanAo->callback);
}
