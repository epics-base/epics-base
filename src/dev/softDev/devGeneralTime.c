/*************************************************************************\
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Id$ */
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
#include "epicsExport.h"

#include "aiRecord.h"
#include "boRecord.h"
#include "longinRecord.h"
#include "stringinRecord.h"


/********* ai record **********/
static struct ai_channel {
    char *name;
    int (*get)(double *);
} ai_channels[] = {
    {"TIME", generalTimeGetCurrentDouble},
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
static struct bo_channel {
    char *name;
    void (*put)();
} bo_channels[] = {
    {"RSTERRCNT", generalTimeResetErrorCounts},
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

    pchan->put(prec->val);
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
static struct li_channel {
    char *name;
    int (*get)(void);
} li_channels[] = {
    {"GETERRCNT", generalTimeGetErrorCounts},
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
static struct si_channel {
    char *name;
    void (*get)(char *buf);
} si_channels[] = {
    {"BESTTCP", generalTimeGetBestTcp},
    {"BESTTEP", generalTimeGetBestTep},
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

    if (!pchan) return -1;

    pchan->get(prec->val);
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
