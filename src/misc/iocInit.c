/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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

#include "dbDefs.h"
#include "epicsThread.h"
#include "epicsPrint.h"
#include "ellLib.h"
#include "epicsGeneralTime.h"
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
#include "menuConvert.h"
#include "menuPini.h"
#include "registryRecordType.h"
#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"
#include "errMdef.h"
#include "recSup.h"
#include "envDefs.h"
#include "rsrv.h"
#include "asDbLib.h"
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
    /* After this point, further calls to iocInit() are disallowed.  */
    iocState = iocBuilding;

    checkGeneralTime();
    taskwdInit();
    callbackInit();
    initHookAnnounce(initHookAfterCallbackInit);

    dbCaLinkInit();
    initHookAnnounce(initHookAfterCaLinkInit);

    initDrvSup();
    initHookAnnounce(initHookAfterInitDrvSup);

    initRecSup();
    initHookAnnounce(initHookAfterInitRecSup);

    initDevSup();
    initHookAnnounce(initHookAfterInitDevSup);

    initDatabase();
    dbLockInitRecords(pdbbase);
    dbBkptInit();
    initHookAnnounce(initHookAfterInitDatabase);

    finishDevSup();
    initHookAnnounce(initHookAfterFinishDevSup);

    scanInit();
    if (asInit()) {
        errlogPrintf("iocBuild: asInit Failed.\n");
        return -1;
    }
    dbPutNotifyInit();
    epicsThreadSleep(.5);
    initHookAnnounce(initHookAfterScanInit);

    initialProcess();
    initHookAnnounce(initHookAfterInitialProcess);

    /* Start CA server threads */
    rsrv_init();
    initHookAnnounce(initHookAfterCaServerInit);

    iocState = iocBuilt;
    initHookAnnounce(initHookAfterIocBuilt);
    return 0;
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

/*
 * Iterate through all record instances (but not aliases),
 * calling a function for each one.
 */
typedef void (*recIterFunc)(dbRecordType *rtyp, dbCommon *prec, void *user);

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

static void doResolveLinks(dbRecordType *pdbRecordType, dbCommon *precord,
    void *user)
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
                        errlogPrintf("Forward-link uses Channel Access "
                            "without pointing to PROC field\n"
                            "    %s.%s = %s\n",
                            precord->name, pdbFldDes->name,
                            plink->value.pv_link.pvname);
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

static void doInitRecord1(dbRecordType *pdbRecordType, dbCommon *precord,
    void *user)
{
    struct rset *prset = pdbRecordType->prset;

    if (!prset) return;         /* unlikely */

    if (prset->init_record)
        prset->init_record(precord, 1);
}

static void initDatabase(void)
{
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
 * Shutdown processing.
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

        if (plink->type == CA_LINK) {
            if (!locked) {
                dbScanLock(precord);
                locked = 1;
            }
            dbCaRemoveLink(plink);
            plink->type = CONSTANT;
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
        pdsxt->del_record(precord);
    }
    if (locked) {
        precord->pact = TRUE;
        dbScanUnlock(precord);
    }
}

static void exitDatabase(void *dummy)
{
    iterateRecords(doCloseLinks, NULL);
    iocState = iocStopped;
}
