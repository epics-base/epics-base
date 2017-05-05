/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 Helmholtz-Zentrum Berlin
*     für Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
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

#include "dbBase.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "envDefs.h"
#include "epicsExit.h"
#include "epicsGeneralTime.h"
#include "epicsPrint.h"
#include "epicsSignal.h"
#include "epicsThread.h"
#include "errMdef.h"
#include "iocsh.h"
#include "taskwd.h"

#include "caeventmask.h"

#include "epicsExport.h" /* defines epicsExportSharedSymbols */
#include "alarm.h"
#include "asDbLib.h"
#include "callback.h"
#include "dbAccess.h"
#include "db_access_routines.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbBkpt.h"
#include "dbCa.h"
#include "dbChannel.h"
#include "dbCommon.h"
#include "dbFldTypes.h"
#include "dbLock.h"
#include "dbNotify.h"
#include "dbScan.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "devSup.h"
#include "drvSup.h"
#include "epicsRelease.h"
#include "initHooks.h"
#include "iocInit.h"
#include "link.h"
#include "menuConvert.h"
#include "menuPini.h"
#include "recGbl.h"
#include "recSup.h"
#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"
#include "registryJLinks.h"
#include "registryRecordType.h"
#include "rsrv.h"

static enum {
    iocVirgin, iocBuilding, iocBuilt, iocRunning, iocPaused, iocStopped
} iocState = iocVirgin;
static enum {
    buildRSRV, buildIsolated
} iocBuildMode;

/* define forward references*/
static int checkDatabase(dbBase *pdbbase);
static void checkGeneralTime(void);
static void initDrvSup(void);
static void initRecSup(void);
static void initDevSup(void);
static void finishDevSup(void);
static void initDatabase(void);
static void initialProcess(void);
static void exitDatabase(void *dummy);

/*
 * Iterate through all record instances (but not aliases),
 * calling a function for each one.
 */
typedef void (*recIterFunc)(dbRecordType *rtyp, dbCommon *prec, void *user);

static void iterateRecords(recIterFunc func, void *user);

int dbThreadRealtimeLock = 1;
epicsExportAddress(int, dbThreadRealtimeLock);

/*
 *  Initialize EPICS on the IOC.
 */
int iocInit(void)
{
    return iocBuild() || iocRun();
}

static int iocBuild_1(void)
{
    if (iocState != iocVirgin && iocState != iocStopped) {
        errlogPrintf("iocBuild: IOC can only be initialized from uninitialized or stopped state\n");
        return -1;
    }
    errlogInit(0);
    initHookAnnounce(initHookAtIocBuild);

    if (!epicsThreadIsOkToBlock()) {
        epicsThreadSetOkToBlock(1);
    }

    errlogPrintf("Starting iocInit\n");
    if (checkDatabase(pdbbase)) {
        errlogPrintf("iocBuild: Aborting, bad database definition (DBD)!\n");
        return -1;
    }
    epicsSignalInstallSigHupIgnore();
    initHookAnnounce(initHookAtBeginning);

    coreRelease();
    iocState = iocBuilding;

    checkGeneralTime();
    taskwdInit();
    callbackInit();
    initHookAnnounce(initHookAfterCallbackInit);

    return 0;
}

static void prepareLinks(dbRecordType *rtyp, dbCommon *prec, void *junk)
{
    dbInitRecordLinks(rtyp, prec);
}

static int iocBuild_2(void)
{
    initHookAnnounce(initHookAfterCaLinkInit);

    initDrvSup();
    initHookAnnounce(initHookAfterInitDrvSup);

    initRecSup();
    initHookAnnounce(initHookAfterInitRecSup);

    initDevSup();
    initHookAnnounce(initHookAfterInitDevSup); /* used by autosave pass 0 */

    iterateRecords(prepareLinks, NULL);

    dbLockInitRecords(pdbbase);
    initDatabase();
    dbBkptInit();
    initHookAnnounce(initHookAfterInitDatabase); /* used by autosave pass 1 */

    finishDevSup();
    initHookAnnounce(initHookAfterFinishDevSup);

    scanInit();
    if (asInit()) {
        errlogPrintf("iocBuild: asInit Failed.\n");
        return -1;
    }
    dbProcessNotifyInit();
    epicsThreadSleep(.5);
    initHookAnnounce(initHookAfterScanInit);

    initialProcess();
    initHookAnnounce(initHookAfterInitialProcess);
    return 0;
}

static int iocBuild_3(void)
{
    initHookAnnounce(initHookAfterCaServerInit);

    iocState = iocBuilt;
    initHookAnnounce(initHookAfterIocBuilt);
    return 0;
}

int iocBuild(void)
{
    int status;

    status = iocBuild_1();
    if (status) return status;

    dbCaLinkInit();

    status = iocBuild_2();
    if (status) return status;

    /* Start CA server threads */
    rsrv_init();

    status = iocBuild_3();

    if (dbThreadRealtimeLock)
        epicsThreadRealtimeLock();

    if (!status) iocBuildMode = buildRSRV;
    return status;
}

int iocBuildIsolated(void)
{
    int status;

    status = iocBuild_1();
    if (status) return status;

    dbCaLinkInitIsolated();

    status = iocBuild_2();
    if (status) return status;

    status = iocBuild_3();
    if (!status) iocBuildMode = buildIsolated;
    return status;
}

int iocRun(void)
{
    if (iocState != iocPaused && iocState != iocBuilt) {
        errlogPrintf("iocRun: IOC not paused\n");
        return -1;
    }
    initHookAnnounce(initHookAtIocRun);

   /* Enable scan tasks and some driver support functions.  */
    scanRun();
    dbCaRun();
    initHookAnnounce(initHookAfterDatabaseRunning);
    if (iocState == iocBuilt)
        initHookAnnounce(initHookAfterInterruptAccept);

    rsrv_run();
    initHookAnnounce(initHookAfterCaServerRunning);
    if (iocState == iocBuilt)
        initHookAnnounce(initHookAtEnd);

    errlogPrintf("iocRun: %s\n", iocState == iocBuilt ?
        "All initialization complete" :
        "IOC restarted");
    iocState = iocRunning;
    initHookAnnounce(initHookAfterIocRunning);
    return 0;
}

int iocPause(void)
{
    if (iocState != iocRunning) {
        errlogPrintf("iocPause: IOC not running\n");
        return -1;
    }
    initHookAnnounce(initHookAtIocPause);

    rsrv_pause();
    initHookAnnounce(initHookAfterCaServerPaused);

    dbCaPause();
    scanPause();
    initHookAnnounce(initHookAfterDatabasePaused);

    iocState = iocPaused;
    errlogPrintf("iocPause: IOC suspended\n");
    initHookAnnounce(initHookAfterIocPaused);
    return 0;
}

/*
 * Database sanity checks
 *
 * This is not an attempt to sanity-check the whole .dbd file, only
 * two menus normally get modified by users: menuConvert and menuScan.
 *
 * The menuConvert checks were added to flag problems with IOCs
 * converted from 3.13.x, where the SLOPE choice didn't exist.
 *
 * The menuScan checks make sure the user didn't fiddle too much
 * when creating new periodic scan choices.
 */

static int checkDatabase(dbBase *pdbbase)
{
    const dbMenu *pMenu;

    if (!pdbbase) {
        errlogPrintf("checkDatabase: No database definitions loaded.\n");
        return -1;
    }

    pMenu = dbFindMenu(pdbbase, "menuConvert");
    if (!pMenu) {
        errlogPrintf("checkDatabase: menuConvert not defined.\n");
        return -1;
    }
    if (pMenu->nChoice <= menuConvertLINEAR) {
        errlogPrintf("checkDatabase: menuConvert has too few choices.\n");
        return -1;
    }
    if (strcmp(pMenu->papChoiceName[menuConvertNO_CONVERSION],
               "menuConvertNO_CONVERSION")) {
        errlogPrintf("checkDatabase: menuConvertNO_CONVERSION doesn't match.\n");
        return -1;
    }
    if (strcmp(pMenu->papChoiceName[menuConvertSLOPE], "menuConvertSLOPE")) {
        errlogPrintf("checkDatabase: menuConvertSLOPE doesn't match.\n");
        return -1;
    }
    if (strcmp(pMenu->papChoiceName[menuConvertLINEAR], "menuConvertLINEAR")) {
        errlogPrintf("checkDatabase: menuConvertLINEAR doesn't match.\n");
        return -1;
    }

    pMenu = dbFindMenu(pdbbase, "menuScan");
    if (!pMenu) {
        errlogPrintf("checkDatabase: menuScan not defined.\n");
        return -1;
    }
    if (pMenu->nChoice <= menuScanI_O_Intr) {
        errlogPrintf("checkDatabase: menuScan has too few choices.\n");
        return -1;
    }
    if (strcmp(pMenu->papChoiceName[menuScanPassive],
               "menuScanPassive")) {
        errlogPrintf("checkDatabase: menuScanPassive doesn't match.\n");
        return -1;
    }
    if (strcmp(pMenu->papChoiceName[menuScanEvent],
               "menuScanEvent")) {
        errlogPrintf("checkDatabase: menuScanEvent doesn't match.\n");
        return -1;
    }
    if (strcmp(pMenu->papChoiceName[menuScanI_O_Intr],
               "menuScanI_O_Intr")) {
        errlogPrintf("checkDatabase: menuScanI_O_Intr doesn't match.\n");
        return -1;
    }
    if (pMenu->nChoice <= SCAN_1ST_PERIODIC) {
        errlogPrintf("checkDatabase: menuScan has no periodic choices.\n");
        return -1;
    }

    return 0;
}

static void checkGeneralTime(void)
{
    epicsTimeStamp ts;

    epicsTimeGetCurrent(&ts);
    if (ts.secPastEpoch < 2*24*60*60) {
        static const char * const tsfmt = "%Y-%m-%d %H:%M:%S.%09f";
        char buff[40];

        epicsTimeToStrftime(buff, sizeof(buff), tsfmt, &ts);
        errlogPrintf("iocInit: Time provider has not yet synchronized.\n");
    }

    epicsTimeGetEvent(&ts, 1);  /* Prime gtPvt.lastEventProvider for ISRs */
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
        rset *prset;

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

static void initDevSup(void)
{
    dbRecordType *pdbRecordType;
    
    for (pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        devSup *pdevSup;

        for (pdevSup = (devSup *)ellFirst(&pdbRecordType->devList);
             pdevSup;
             pdevSup = (devSup *)ellNext(&pdevSup->node)) {
            struct dset *pdset = registryDeviceSupportFind(pdevSup->name);

            if (!pdset) {
                errlogPrintf("device support %s not found\n",pdevSup->name);
                continue;
            }
            dbInitDevSup(pdevSup, pdset);   /* Calls pdset->init(0) */
        }
    }
}

static void finishDevSup(void)
{
    dbRecordType *pdbRecordType;

    for (pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        devSup *pdevSup;

        for (pdevSup = (devSup *)ellFirst(&pdbRecordType->devList);
             pdevSup;
             pdevSup = (devSup *)ellNext(&pdevSup->node)) {
            struct dset *pdset = pdevSup->pdset;

            if (pdset && pdset->init)
                pdset->init(1);
        }
    }
}

static void iterateRecords(recIterFunc func, void *user)
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

            func(pdbRecordType, precord, user);
        }
    }
    return;
}

static void doInitRecord0(dbRecordType *pdbRecordType, dbCommon *precord,
    void *user)
{
    rset *prset = pdbRecordType->prset;
    devSup *pdevSup;

    if (!prset) return;         /* unlikely */

    precord->rset = prset;
    precord->mlok = epicsMutexMustCreate();
    ellInit(&precord->mlis);

    /* Reset the process active field */
    precord->pact = FALSE;

    /* Initial UDF severity */
    if (precord->udf && precord->stat == UDF_ALARM)
    	precord->sevr = precord->udfs;

    /* Init DSET NOTE that result may be NULL */
    pdevSup = dbDTYPtoDevSup(pdbRecordType, precord->dtyp);
    precord->dset = pdevSup ? pdevSup->pdset : NULL;

    if (prset->init_record)
        prset->init_record(precord, 0);
}

static void doResolveLinks(dbRecordType *pdbRecordType, dbCommon *precord,
    void *user)
{
    dbFldDes **papFldDes = pdbRecordType->papFldDes;
    short *link_ind = pdbRecordType->link_ind;
    int j;

    /* For all the links in the record type... */
    for (j = 0; j < pdbRecordType->no_links; j++) {
        dbFldDes *pdbFldDes = papFldDes[link_ind[j]];
        DBLINK *plink = (DBLINK*)((char*)precord + pdbFldDes->offset);

        if (ellCount(&precord->rdes->devList) > 0 && pdbFldDes->isDevLink) {
            devSup *pdevSup = dbDTYPtoDevSup(pdbRecordType, precord->dtyp);

            if (pdevSup) {
                struct dsxt *pdsxt = pdevSup->pdsxt;
                if (pdsxt && pdsxt->add_record) {
                    pdsxt->add_record(precord);
                }
            }
        }

        dbInitLink(plink, pdbFldDes->field_type);
    }
}

static void doInitRecord1(dbRecordType *pdbRecordType, dbCommon *precord,
    void *user)
{
    rset *prset = pdbRecordType->prset;

    if (!prset) return;         /* unlikely */

    if (prset->init_record)
        prset->init_record(precord, 1);
}

static void initDatabase(void)
{
    dbChannelInit();
    iterateRecords(doInitRecord0, NULL);
    iterateRecords(doResolveLinks, NULL);
    iterateRecords(doInitRecord1, NULL);

    epicsAtExit(exitDatabase, NULL);
    return;
}

/*
 *  Process database records at initialization ordered by phase
 *     if their pini (process at init) field is set.
 */
typedef struct {
    int this;
    int next;
    epicsEnum16 pini;
} phaseData_t;

static void doRecordPini(dbRecordType *rtype, dbCommon *precord, void *user)
{
    phaseData_t *pphase = (phaseData_t *)user;
    int phas;

    if (precord->pini != pphase->pini) return;

    phas = precord->phas;
    if (phas == pphase->this) {
        dbScanLock(precord);
        dbProcess(precord);
        dbScanUnlock(precord);
    } else if (phas > pphase->this && phas < pphase->next)
        pphase->next = phas;
}

static void piniProcess(int pini)
{
    phaseData_t phase;
    phase.next = MIN_PHASE;
    phase.pini = pini;

    /* This scans through the whole database as many times as needed.
     * During the first pass it is unlikely to find any records with
     * PHAS = MIN_PHASE, but during each iteration it looks for the
     * phase value of the next pass to run.  Note that PHAS fields can
     * be changed at runtime, so we have to look for the lowest value
     * of PHAS each time.
     */
    do {
        phase.this = phase.next;
        phase.next = MAX_PHASE + 1;
        iterateRecords(doRecordPini, &phase);
    } while (phase.next != MAX_PHASE + 1);
}

static void piniProcessHook(initHookState state)
{
    switch (state) {
    case initHookAtIocRun:
        piniProcess(menuPiniRUN);
        break;

    case initHookAfterIocRunning:
        piniProcess(menuPiniRUNNING);
        break;

    case initHookAtIocPause:
        piniProcess(menuPiniPAUSE);
        break;

    case initHookAfterIocPaused:
        piniProcess(menuPiniPAUSED);
        break;

    default:
        break;
    }
}

static void initialProcess(void)
{
    initHookRegister(piniProcessHook);
    piniProcess(menuPiniYES);
}


/*
 * set DB_LINK and CA_LINK to PV_LINK
 * Delete record scans
 */
static void doCloseLinks(dbRecordType *pdbRecordType, dbCommon *precord,
    void *user)
{
    devSup *pdevSup;
    struct dsxt *pdsxt;
    int j;
    int locked = 0;

    for (j = 0; j < pdbRecordType->no_links; j++) {
        dbFldDes *pdbFldDes =
            pdbRecordType->papFldDes[pdbRecordType->link_ind[j]];
        DBLINK *plink = (DBLINK *)((char *)precord + pdbFldDes->offset);

        if (plink->type == CA_LINK ||
            plink->type == JSON_LINK ||
            (plink->type == DB_LINK && iocBuildMode == buildIsolated)) {
            if (!locked) {
                dbScanLock(precord);
                locked = 1;
            }
            dbRemoveLink(NULL, plink);
        }
    }

    if (precord->dset &&
        (pdevSup = dbDSETtoDevSup(pdbRecordType, precord->dset)) &&
        (pdsxt = pdevSup->pdsxt) &&
        pdsxt->del_record) {
        if (!locked) {
            dbScanLock(precord);
            locked = 1;
        }
        scanDelete(precord);    /* Being consistent... */
        pdsxt->del_record(precord);
    }
    if (locked) {
        precord->pact = TRUE;
        dbScanUnlock(precord);
    }
}

static void doFreeRecord(dbRecordType *pdbRecordType, dbCommon *precord,
    void *user)
{
    int j;

    for (j = 0; j < pdbRecordType->no_links; j++) {
        dbFldDes *pdbFldDes =
            pdbRecordType->papFldDes[pdbRecordType->link_ind[j]];
        DBLINK *plink = (DBLINK *)((char *)precord + pdbFldDes->offset);

        dbFreeLinkContents(plink);
    }

    epicsMutexDestroy(precord->mlok);
    free(precord->ppnr); /* may be allocated in dbNotify.c */
}

int iocShutdown(void)
{
    if (iocState == iocVirgin || iocState == iocStopped) return 0;
    iterateRecords(doCloseLinks, NULL);
    if (iocBuildMode==buildIsolated) {
        /* stop and "join" threads */
        scanStop();
        callbackStop();
    }
    dbCaShutdown(); /* must be before dbFreeRecord and dbChannelExit */
    if (iocBuildMode==buildIsolated) {
        /* free resources */
        scanCleanup();
        callbackCleanup();
        iterateRecords(doFreeRecord, NULL);
        dbLockCleanupRecords(pdbbase);
        asShutdown();
        dbChannelExit();
        dbProcessNotifyExit();
        iocshFree();
    }
    iocState = iocStopped;
    iocBuildMode = buildRSRV;
    return 0;
}

static void exitDatabase(void *dummy)
{
    iocShutdown();
}
