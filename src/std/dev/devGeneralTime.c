/*************************************************************************\
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *   Original Author:	Sheng Peng, ORNL / SNS Project
 *   Date:		07/2004
 *
 *   EPICS device support for general timestamp support
 *
 *   Integrated into base by Peter Denison, Diamond Light Source
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "devSup.h"
#include "epicsString.h"
#include "epicsGeneralTime.h"

#include "aiRecord.h"
#include "boRecord.h"
#include "longinRecord.h"
#include "stringinRecord.h"
#include "epicsExport.h"


/********* ai record **********/
static int getCurrentTime(double * pseconds)
{
    epicsTimeStamp ts;

    if (epicsTimeOK == epicsTimeGetCurrent(&ts)) {
        *pseconds = ts.secPastEpoch + ((double)(ts.nsec)) * 1e-9;
        return 0;
    }
    return -1;
}

static struct ai_channel {
    char *name;
    int (*get)(double *);
} ai_channels[] = {
    {"TIME", getCurrentTime},
};

static long init_ai(aiRecord *prec)
{
    int i;

    if (prec->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, (void *)prec,
                          "devAiGeneralTime::init_ai: Illegal INP field");
        prec->pact = TRUE;
        return S_db_badField;
    }

    for (i = 0; i < NELEMENTS(ai_channels); i++) {
        struct ai_channel *pchan = &ai_channels[i];
        if (!epicsStrCaseCmp(prec->inp.value.instio.string, pchan->name)) {
            prec->dpvt = pchan;
            return 0;
        }
    }

    recGblRecordError(S_db_badField, (void *)prec,
                      "devAiGeneralTime::init_ai: Bad parm");
    prec->pact = TRUE;
    prec->dpvt = NULL;
    return S_db_badField;
}

static long read_ai(aiRecord *prec)
{
    struct ai_channel *pchan = (struct ai_channel *)prec->dpvt;

    if (!pchan) return -1;

    if (pchan->get(&prec->val) == 0) {
        prec->udf = FALSE;
        return 2;
    }
    prec->udf = TRUE;
    recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
    return -1;
}

struct {
    dset common;
    DEVSUPFUN read_write;
    DEVSUPFUN special_linconv;
} devAiGeneralTime = {
    {6, NULL, NULL, init_ai, NULL}, read_ai,  NULL
};
epicsExportAddress(dset, devAiGeneralTime);


/********* bo record **********/
static void resetErrors(void)
{
    generalTimeResetErrorCounts();
}

static struct bo_channel {
    char *name;
    void (*put)(void);
} bo_channels[] = {
    {"RSTERRCNT", resetErrors},
};

static long init_bo(boRecord *prec)
{
    int i;

    if (prec->out.type != INST_IO) {
        recGblRecordError(S_db_badField, (void *)prec,
                          "devAiGeneralTime::init_ai: Illegal INP field");
        prec->pact = TRUE;
        return S_db_badField;
    }

    for (i = 0; i < NELEMENTS(bo_channels); i++) {
        struct bo_channel *pchan = &bo_channels[i];
        if (!epicsStrCaseCmp(prec->out.value.instio.string, pchan->name)) {
            prec->dpvt = pchan;
            prec->mask = 0;
            return 2;
        }
    }

    recGblRecordError(S_db_badField, (void *)prec,
                      "devBoGeneralTime::init_bo: Bad parm");
    prec->pact = TRUE;
    prec->dpvt = NULL;
    return S_db_badField;
}

static long write_bo(boRecord *prec)
{
    struct bo_channel *pchan = (struct bo_channel *)prec->dpvt;

    if (!pchan) return -1;

    pchan->put();
    return 0;
}

struct {
    dset common;
    DEVSUPFUN read_write;
} devBoGeneralTime = {
    {5, NULL, NULL, init_bo, NULL}, write_bo
};
epicsExportAddress(dset, devBoGeneralTime);


/******* longin record *************/
static int errorCount(void)
{
    return generalTimeGetErrorCounts();
}

static struct li_channel {
    char *name;
    int (*get)(void);
} li_channels[] = {
    {"GETERRCNT", errorCount},
};

static long init_li(longinRecord *prec)
{
    int i;

    if (prec->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, (void *)prec,
                          "devLiGeneralTime::init_li: Illegal INP field");
        prec->pact = TRUE;
        return S_db_badField;
    }

    for (i = 0; i < NELEMENTS(li_channels); i++) {
        struct li_channel *pchan = &li_channels[i];
        if (!epicsStrCaseCmp(prec->inp.value.instio.string, pchan->name)) {
            prec->dpvt = pchan;
            return 0;
        }
    }

    recGblRecordError(S_db_badField, (void *)prec,
                      "devLiGeneralTime::init_li: Bad parm");
    prec->pact = TRUE;
    prec->dpvt = NULL;
    return S_db_badField;
}

static long read_li(longinRecord *prec)
{
    struct li_channel *pchan = (struct li_channel *)prec->dpvt;

    if (!pchan) return -1;

    prec->val = pchan->get();
    return 0;
}

struct {
    dset common;
    DEVSUPFUN read_write;
} devLiGeneralTime = {
    {5, NULL, NULL, init_li, NULL}, read_li
};
epicsExportAddress(dset, devLiGeneralTime);


/********** stringin record **********/
static const char * timeProvider(void)
{
    return generalTimeCurrentProviderName();
}

static const char * highestProvider(void)
{
    return generalTimeHighestCurrentName();
}

static const char * eventProvider(void)
{
    return generalTimeEventProviderName();
}

static struct si_channel {
    char *name;
    const char * (*get)(void);
} si_channels[] = {
    {"BESTTCP", timeProvider},
    {"TOPTCP", highestProvider},
    {"BESTTEP", eventProvider},
};

static long init_si(stringinRecord *prec)
{
    int i;

    if (prec->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, (void *)prec,
                          "devSiGeneralTime::init_si: Illegal INP field");
        prec->pact = TRUE;
        return S_db_badField;
    }

    for (i = 0; i < NELEMENTS(si_channels); i++) {
        struct si_channel *pchan = &si_channels[i];
        if (!epicsStrCaseCmp(prec->inp.value.instio.string, pchan->name)) {
            prec->dpvt = pchan;
            return 0;
        }
    }

    recGblRecordError(S_db_badField, (void *)prec,
                      "devSiGeneralTime::init_si: Bad parm");
    prec->pact = TRUE;
    prec->dpvt = NULL;
    return S_db_badField;
}

static long read_si(stringinRecord *prec)
{
    struct si_channel *pchan = (struct si_channel *)prec->dpvt;
    const char *name;

    if (!pchan) return -1;

    name = pchan->get();
    if (name) {
        strncpy(prec->val, name, sizeof(prec->val));
        prec->val[sizeof(prec->val) - 1] = '\0';
    } else {
        strcpy(prec->val, "No working providers");
        recGblSetSevr(prec, READ_ALARM, MAJOR_ALARM);
    }
    prec->udf = FALSE;
    return 0;
}

struct {
    dset common;
    DEVSUPFUN read_write;
} devSiGeneralTime = {
    {5, NULL, NULL, init_si, NULL}, read_si
};
epicsExportAddress(dset, devSiGeneralTime);
