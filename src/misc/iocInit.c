/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocInit.c */
/* $Id$ */
/*
 *      Original Author: Marty Kraimer
 *      Date:            06-01-91
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "dbDefs.h"
#include "epicsThread.h"
#include "epicsPrint.h"
#include "ellLib.h"
#include "dbDefs.h"
#include "dbBase.h"
#include "caeventmask.h"
#include "dbAddr.h"
#include "dbBkpt.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbLock.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "dbNotify.h"
#include "dbCa.h"
#include "dbScan.h"
#include "taskwd.h"
#include "callback.h"
#include "dbCommon.h"
#include "dbLock.h"
#include "devSup.h"
#include "drvSup.h"
#include "registryRecordType.h"
#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"
#include "errMdef.h"
#include "recSup.h"
#include "envDefs.h"
#include "rsrv.h"
epicsShareFunc int epicsShareAPI asInit (void);
#include "dbStaticLib.h"
#include "db_access_routines.h"
#include "initHooks.h"
#include "epicsExit.h"
#include "epicsSignal.h"

#define epicsExportSharedSymbols
#include "epicsRelease.h"
#include "iocInit.h"

static enum {
    iocVirgin, iocBuilding, iocBuilt, iocRunning, iocPaused, iocStopped
} iocState = iocVirgin;

/* define forward references*/
static void initDrvSup(void);
static void initRecSup(void);
static void initDevSup(void);
static void finishDevSup(void);
static void initDatabase(void);
static void initialProcess(void);
static void exitDatabase(void *dummy);


/*
 *  Initialize EPICS on the IOC.
 */
int iocInit(void)
{
    return iocBuild() || iocRun();
}

int iocBuild(void)
{
    if (iocState != iocVirgin) {
        errlogPrintf("iocBuild: IOC can only be initialized once\n");
        return -1;
    }

    if (!epicsThreadIsOkToBlock()) {
        epicsThreadSetOkToBlock(1);
    }

    errlogPrintf("Starting iocInit\n");
    if (!pdbbase) {
        errlogPrintf("iocBuild: Aborting, no database loaded!\n");
        return -1;
    }
    epicsSignalInstallSigHupIgnore();
    initHooks(initHookAtBeginning);

    coreRelease();
    /* After this point, further calls to iocInit() are disallowed.  */
    iocState = iocBuilding;

    taskwdInit();
    callbackInit();
    initHooks(initHookAfterCallbackInit);

    dbCaLinkInit();
    initHooks(initHookAfterCaLinkInit);

    initDrvSup();
    initHooks(initHookAfterInitDrvSup);

    initRecSup();
    initHooks(initHookAfterInitRecSup);

    initDevSup();
    initHooks(initHookAfterInitDevSup);

    initDatabase();
    dbLockInitRecords(pdbbase);
    dbBkptInit();
    initHooks(initHookAfterInitDatabase);

    finishDevSup();
    initHooks(initHookAfterFinishDevSup);

    scanInit();
    if (asInit()) {
        errlogPrintf("iocBuild: asInit Failed.\n");
        return -1;
    }
    dbPutNotifyInit();
    epicsThreadSleep(.5);
    initHooks(initHookAfterScanInit);

    initialProcess();
    initHooks(initHookAfterInitialProcess);

    /* Start CA server threads */
    rsrv_init();

    iocState = iocBuilt;
    return 0;
}

int iocRun(void)
{
    if (iocState != iocPaused && iocState != iocBuilt) {
        errlogPrintf("iocRun: IOC not paused\n");
        return -1;
    }

   /* Enable scan tasks and some driver support functions.  */
    scanRun();
    dbCaRun();
    if (iocState == iocBuilt)
        initHooks(initHookAfterInterruptAccept);

    rsrv_run();
    if (iocState == iocBuilt)
        initHooks(initHookAtEnd);

    errlogPrintf("iocRun: %s\n", iocState == iocBuilt ?
        "All initialization complete" :
        "IOC restarted");
    iocState = iocRunning;
    return 0;
}

int iocPause(void)
{
    if (iocState != iocRunning) {
        errlogPrintf("iocPause: IOC not running\n");
        return -1;
    }

    rsrv_pause();
    dbCaPause();
    scanPause();
    iocState = iocPaused;

    errlogPrintf("iocPause: IOC suspended\n");
    return 0;
}


static void initDrvSup(void) /* Locate all driver support entry tables */
{
    drvSup *pdrvSup;

    for (pdrvSup = (drvSup *)ellFirst(&pdbbase->drvList); pdrvSup;
         pdrvSup = (drvSup *)ellNext(&pdrvSup->node)) {
        struct drvet *pdrvet = registryDriverSupportFind(pdrvSup->name);

        if (!pdrvet) {
            errlogPrintf("iocInit: driver %s not found\n", pdrvSup->name);
            continue;
        }
        pdrvSup->pdrvet = pdrvet;

        if (pdrvet->init)
            pdrvet->init();
    }
}

static void initRecSup(void)
{
    dbRecordType *pdbRecordType;

    for (pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        recordTypeLocation *precordTypeLocation =
            registryRecordTypeFind(pdbRecordType->name);
        struct rset *prset;

        if (!precordTypeLocation) {
            errlogPrintf("iocInit record support for %s not found\n",
                pdbRecordType->name);
            continue;
        }
        prset = precordTypeLocation->prset;
        pdbRecordType->prset = prset;
        if (prset->init) {
            prset->init();
        }
    }
}

static long do_nothing(struct dbCommon *precord) { return 0; }

/* Dummy DSXT used for soft device supports */
struct dsxt devSoft_DSXT = {
    do_nothing,
    do_nothing
};

static devSup *pthisDevSup = NULL;

static void initDevSup(void)
{
    dbRecordType *pdbRecordType;
    
    for (pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        for (pthisDevSup = (devSup *)ellFirst(&pdbRecordType->devList);
             pthisDevSup;
             pthisDevSup = (devSup *)ellNext(&pthisDevSup->node)) {
            struct dset *pdset = registryDeviceSupportFind(pthisDevSup->name);

            if (!pdset) {
                errlogPrintf("device support %s not found\n",pthisDevSup->name);
                continue;
            }
            if (pthisDevSup->link_type == CONSTANT)
                pthisDevSup->pdsxt = &devSoft_DSXT;
            pthisDevSup->pdset = pdset;
            if (pdset->init) {
                pdset->init(0);
            }
        }
    }
}

epicsShareFunc void devExtend(dsxt *pdsxt)
{
    if (!pthisDevSup)
        errlogPrintf("devExtend() called outside of initDevSup()\n");
    else {
        pthisDevSup->pdsxt = pdsxt;
    }
}

static void finishDevSup(void) 
{
    dbRecordType *pdbRecordType;

    for (pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        for (pthisDevSup = (devSup *)ellFirst(&pdbRecordType->devList);
             pthisDevSup;
             pthisDevSup = (devSup *)ellNext(&pthisDevSup->node)) {
            struct dset *pdset = pthisDevSup->pdset;

            if (pdset && pdset->init)
                pdset->init(1);
        }
    }
}

/*
 * Iterate through all record instances (but not aliases),
 * calling a function for each one.
 */
typedef void (*recIterFunc)(dbRecordType *pdbRecordType, dbCommon *precord);

static void iterateRecords(recIterFunc func)
{
    dbRecordType *pdbRecordType;

    for (pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        dbRecordNode *pdbRecordNode;

        for (pdbRecordNode = (dbRecordNode *)ellFirst(&pdbRecordType->recList);
             pdbRecordNode;
             pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
            dbCommon *precord = pdbRecordNode->precord;

            if (!precord->name[0] ||
                pdbRecordNode->flags & DBRN_FLAGS_ISALIAS)
                continue;

            func(pdbRecordType, precord);
        }
    }
    return;
}

static void doInitRecord0(dbRecordType *pdbRecordType, dbCommon *precord)
{
    struct rset *prset = pdbRecordType->prset;
    devSup *pdevSup;

    if (!prset) return;         /* unlikely */

    precord->rset = prset;
    precord->rdes = pdbRecordType;
    precord->mlok = epicsMutexMustCreate();
    ellInit(&precord->mlis);

    /* Reset the process active field */
    precord->pact = FALSE;

    /* Init DSET NOTE that result may be NULL */
    pdevSup = dbDTYPtoDevSup(pdbRecordType, precord->dtyp);
    precord->dset = pdevSup ? pdevSup->pdset : NULL;

    if (prset->init_record)
        prset->init_record(precord, 0);
}

static void doResolveLinks(dbRecordType *pdbRecordType, dbCommon *precord)
{
    devSup *pdevSup;
    int j;

    /* Convert all PV_LINKs to DB_LINKs or CA_LINKs */
    /* For all the links in the record type... */
    for (j = 0; j < pdbRecordType->no_links; j++) {
        dbFldDes *pdbFldDes =
            pdbRecordType->papFldDes[pdbRecordType->link_ind[j]];
        DBLINK *plink = (DBLINK *)((char *)precord + pdbFldDes->offset);

        if (plink->type == PV_LINK) {
            DBADDR dbaddr;

            if (plink == &precord->tsel) recGblTSELwasModified(plink);
            if (!(plink->value.pv_link.pvlMask&(pvlOptCA|pvlOptCP|pvlOptCPP))
                && (dbNameToAddr(plink->value.pv_link.pvname,&dbaddr)==0)) {
                DBADDR  *pdbAddr;

                plink->type = DB_LINK;
                pdbAddr = dbCalloc(1,sizeof(struct dbAddr));
                *pdbAddr = dbaddr; /*structure copy*/;
                plink->value.pv_link.pvt = pdbAddr;
            } else {/*It is a CA link*/

                if (pdbFldDes->field_type == DBF_INLINK) {
                    plink->value.pv_link.pvlMask |= pvlOptInpNative;
                }
                dbCaAddLink(plink);
                if (pdbFldDes->field_type == DBF_FWDLINK) {
                    char *pperiod =
                        strrchr(plink->value.pv_link.pvname,'.');

                    if (pperiod && strstr(pperiod,"PROC")) {
                        plink->value.pv_link.pvlMask |= pvlOptFWD;
                    } else {
                        errlogPrintf("%s.FLNK is a Channel Access Link "
                            " but does not link to a PROC field\n",
                                precord->name);
                    }
                }
            }
        }
    }
    pdevSup = dbDTYPtoDevSup(pdbRecordType, precord->dtyp);
    if (pdevSup) {
        struct dsxt *pdsxt = pdevSup->pdsxt;
        if (pdsxt && pdsxt->add_record) {
            pdsxt->add_record(precord);
        }
    }
}

/* Find range of PHAS for records where PINI is true */
static short minPhase, maxPhase;

static void doInitRecord1(dbRecordType *pdbRecordType, dbCommon *precord)
{
    struct rset *prset = pdbRecordType->prset;
    short phase = precord->phas;

    if (!prset) return;         /* unlikely */

    if (prset->init_record)
        prset->init_record(precord, 1);

    if (precord->pini) {
        if (phase > maxPhase) maxPhase = phase;
        if (phase < minPhase) minPhase = phase;
    }
}

static void initDatabase(void)
{
    minPhase = MAX_PHASE;
    maxPhase = MIN_PHASE;

    iterateRecords(doInitRecord0);
    iterateRecords(doResolveLinks);
    iterateRecords(doInitRecord1);

    epicsAtExit(exitDatabase, NULL);
    return;
}

/*
 *  Process database records at initialization ordered by phase
 *     if their pini (process at init) field is set.
 */
static short piniPhase, nextPhase;

static void doRecordPini(dbRecordType *pdbRecordType, dbCommon *precord)
{
    int phase;

    if (!precord->pini) return;

    phase = precord->phas;
    if (phase == piniPhase)
        dbProcess(precord);
    else if (phase > piniPhase && phase < nextPhase)
        nextPhase = phase;
}

static void initialProcess(void)
{
    nextPhase = minPhase;
    do {
        piniPhase = nextPhase;
        nextPhase = maxPhase;
        iterateRecords(doRecordPini);
    } while (piniPhase != maxPhase);
}


/*
 * Shutdown processing.
 */
static void doCloseLinks(dbRecordType *pdbRecordType, dbCommon *precord)
{
    devSup *pdevSup;
    struct dsxt *pdsxt;
    int j;

    for (j = 0; j < pdbRecordType->no_links; j++) {
        dbFldDes *pdbFldDes =
            pdbRecordType->papFldDes[pdbRecordType->link_ind[j]];
        DBLINK *plink = (DBLINK *)((char *)precord + pdbFldDes->offset);

        if (plink->type == CA_LINK) {
            dbCaRemoveLink(plink);
        }
    }

    if (precord->dset &&
        (pdevSup = dbDSETtoDevSup(pdbRecordType, precord->dset)) &&
        (pdsxt = pdevSup->pdsxt) &&
        pdsxt->del_record) {
        pdsxt->del_record(precord);
    }
}

static void exitDatabase(void *dummy)
{
    iterateRecords(doCloseLinks);
    iocState = iocStopped;
}
