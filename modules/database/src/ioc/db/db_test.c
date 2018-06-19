/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*      database access subroutines */
/*
 *      Author: Bob Dalesio
 *              Andrew Johnson <anj@aps.anl.gov>
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "cadef.h"
#include "dbDefs.h"
#include "epicsStdio.h"
#include "epicsStdlib.h"
#include "epicsString.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbChannel.h"
#include "dbCommon.h"
#include "dbNotify.h"
#include "db_test.h"

#define		MAX_ELEMS	10

int gft(const char *pname)
{
    char tgf_buffer[MAX_ELEMS*MAX_STRING_SIZE + sizeof(struct dbr_ctrl_double)];
    struct dbChannel *chan;
    struct dbCommon *precord;
    long elements;
    short type;
    int i;

    chan = dbChannel_create(pname);
    if (!chan) {
        printf("Channel couldn't be created\n");
        return 1;
    }

    precord = dbChannelRecord(chan);
    elements = dbChannelElements(chan);
    type = dbChannelExportCAType(chan);

    printf("   Record Name: %s\n", precord->name);
    printf("Record Address: 0x%p\n", precord);
    printf("   Export Type: %d\n", type);
    printf(" Field Address: 0x%p\n", dbChannelField(chan));
    printf("    Field Size: %d\n", dbChannelFieldSize(chan));
    printf("   No Elements: %ld\n", elements);

    if (elements > MAX_ELEMS)
        elements = MAX_ELEMS;

    for (i = 0; i <= LAST_BUFFER_TYPE; i++) {
        if (type == 0) {
            if ((i != DBR_STRING) && (i != DBR_STS_STRING) &&
                (i != DBR_TIME_STRING) && (i != DBR_GR_STRING) &&
                (i != DBR_CTRL_STRING))
                continue;
        }
        if (dbChannel_get(chan, i, tgf_buffer, elements, NULL) < 0)
            printf("\t%s Failed\n", dbr_text[i]);
        else
            ca_dump_dbr(i, elements, tgf_buffer);
    }

    dbChannelDelete(chan);
    return 0;
}

/*
 * TPF
 * Test put field
 */
int pft(const char *pname, const char *pvalue)
{
    struct dbChannel *chan;
    struct dbCommon *precord;
    long elements;
    short type;
    char buffer[500];
    short shortvalue;
    long longvalue;
    float floatvalue;
    unsigned char charvalue;
    double doublevalue;

    chan = dbChannel_create(pname);
    if (!chan) {
        printf("Channel couldn't be created\n");
        return 1;
    }

    precord = dbChannelRecord(chan);
    elements = dbChannelElements(chan);
    type = dbChannelExportCAType(chan);

    printf("   Record Name: %s\n", precord->name);
    printf("Record Address: 0x%p\n", precord);
    printf("   Export Type: %d\n", type);
    printf(" Field Address: 0x%p\n", dbChannelField(chan));
    printf("    Field Size: %d\n", dbChannelFieldSize(chan));
    printf("   No Elements: %ld\n", elements);

    if (dbChannel_put(chan, DBR_STRING,pvalue, 1) < 0)
        printf("\n\t failed ");
    if (dbChannel_get(chan, DBR_STRING,buffer, 1, NULL) < 0)
        printf("\n\tfailed");
    else
        ca_dump_dbr(DBR_STRING,1, buffer);

    if (type <= DBF_STRING || type == DBF_ENUM)
        return 0;

    if (sscanf(pvalue, "%hd", &shortvalue) == 1) {
        if (dbChannel_put(chan, DBR_SHORT,&shortvalue, 1) < 0)
            printf("\n\t SHORT failed ");
        if (dbChannel_get(chan, DBR_SHORT,buffer, 1, NULL) < 0)
            printf("\n\t SHORT GET failed");
        else
            ca_dump_dbr(DBR_SHORT,1, buffer);
    }
    if (sscanf(pvalue, "%ld", &longvalue) == 1) {
        if (dbChannel_put(chan, DBR_LONG,&longvalue, 1) < 0)
            printf("\n\t LONG failed ");
        if (dbChannel_get(chan, DBR_LONG,buffer, 1, NULL) < 0)
            printf("\n\t LONG GET failed");
        else
            ca_dump_dbr(DBR_LONG,1, buffer);
    }
    if (epicsScanFloat(pvalue, &floatvalue) == 1) {
        if (dbChannel_put(chan, DBR_FLOAT,&floatvalue, 1) < 0)
            printf("\n\t FLOAT failed ");
        if (dbChannel_get(chan, DBR_FLOAT,buffer, 1, NULL) < 0)
            printf("\n\t FLOAT GET failed");
        else
            ca_dump_dbr(DBR_FLOAT,1, buffer);
    }
    if (epicsScanFloat(pvalue, &floatvalue) == 1) {
        doublevalue = floatvalue;
        if (dbChannel_put(chan, DBR_DOUBLE,&doublevalue, 1) < 0)
            printf("\n\t DOUBLE failed ");
        if (dbChannel_get(chan, DBR_DOUBLE,buffer, 1, NULL) < 0)
            printf("\n\t DOUBLE GET failed");
        else
            ca_dump_dbr(DBR_DOUBLE,1, buffer);
    }
    if (sscanf(pvalue, "%hd", &shortvalue) == 1) {
        charvalue = (unsigned char) shortvalue;
        if (dbChannel_put(chan, DBR_CHAR,&charvalue, 1) < 0)
            printf("\n\t CHAR failed ");
        if (dbChannel_get(chan, DBR_CHAR,buffer, 1, NULL) < 0)
            printf("\n\t CHAR GET failed");
        else
            ca_dump_dbr(DBR_CHAR,1, buffer);
    }
    if (sscanf(pvalue, "%hd", &shortvalue) == 1) {
        if (dbChannel_put(chan, DBR_ENUM,&shortvalue, 1) < 0)
            printf("\n\t ENUM failed ");
        if (dbChannel_get(chan, DBR_ENUM,buffer, 1, NULL) < 0)
            printf("\n\t ENUM GET failed");
        else
            ca_dump_dbr(DBR_ENUM,1, buffer);
    }
    printf("\n");
    dbChannelDelete(chan);
    return (0);
}


typedef struct tpnInfo {
    epicsEventId callbackDone;
    processNotify *ppn;
    char buffer[80];
} tpnInfo;


static int putCallback(processNotify *ppn,notifyPutType type) {
    tpnInfo *ptpnInfo = (tpnInfo *)ppn->usrPvt;

    return db_put_process(ppn, type, DBR_STRING, ptpnInfo->buffer, 1);
}

static void doneCallback(processNotify *ppn)
{
    tpnInfo *ptpnInfo = (tpnInfo *) ppn->usrPvt;
    notifyStatus status = ppn->status;
    const char *pname = dbChannelRecord(ppn->chan)->name;

    if (status == 0)
        printf("tpnCallback '%s': Success\n", pname);
    else
        printf("tpnCallback '%s': Notify status %d\n", pname, (int)status);
    epicsEventSignal(ptpnInfo->callbackDone);
}

static void tpnThread(void *pvt)
{
    tpnInfo *ptpnInfo = (tpnInfo *) pvt;
    processNotify *ppn = (processNotify *) ptpnInfo->ppn;

    dbProcessNotify(ppn);
    epicsEventWait(ptpnInfo->callbackDone);
    dbNotifyCancel(ppn);
    epicsEventDestroy(ptpnInfo->callbackDone);
    dbChannelDelete(ppn->chan);
    free(ppn);
    free(ptpnInfo);
}

int tpn(const char *pname, const char *pvalue)
{
    struct dbChannel *chan;
    tpnInfo *ptpnInfo;
    processNotify *ppn = NULL;

    chan = dbChannel_create(pname);
    if (!chan) {
        printf("Channel couldn't be created\n");
        return 1;
    }

    ppn = calloc(1, sizeof(processNotify));
    if (!ppn) {
        printf("calloc failed\n");
        dbChannelDelete(chan);
        return -1;
    }
    ppn->requestType = putProcessRequest;
    ppn->chan = chan;
    ppn->putCallback = putCallback;
    ppn->doneCallback = doneCallback;

    ptpnInfo = calloc(1, sizeof(tpnInfo));
    if (!ptpnInfo) {
        printf("calloc failed\n");
        free(ppn);
        dbChannelDelete(chan);
        return -1;
    }
    ptpnInfo->ppn = ppn;
    ptpnInfo->callbackDone = epicsEventCreate(epicsEventEmpty);
    strncpy(ptpnInfo->buffer, pvalue, 80);
    ptpnInfo->buffer[79] = 0;

    ppn->usrPvt = ptpnInfo;
    epicsThreadCreate("tpn", epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackMedium), tpnThread, ptpnInfo);
    return 0;
}

