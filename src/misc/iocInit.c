/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocInit.c	ioc initialization */ 
/* $Id$ */
/*      Author:		Marty Kraimer   Date:	06-01-91 */


#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

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
#include "epicsTime.h"
#include "epicsSignal.h"

#define epicsExportSharedSymbols
#include "epicsRelease.h"
#include "iocInit.h"

LOCAL int initialized=FALSE;

/* define forward references*/
LOCAL void initDrvSup(void);
LOCAL void initRecSup(void);
LOCAL void initDevSup(void);
LOCAL void finishDevSup(void);
LOCAL void initDatabase(void);
LOCAL void initialProcess(void);


/*
 *  Initialize EPICS on the IOC.
 */
int epicsShareAPI iocInit()
{
    epicsTimeStamp timeStamp;
    if (initialized) {
	errlogPrintf("iocInit can only be called once\n");
	return(-1);
    }
    if(!epicsThreadIsOkToBlock()) epicsThreadSetOkToBlock(1);
    errlogPrintf("Starting iocInit\n");
    if (!pdbbase) {
	errlogPrintf("iocInit aborting because No database\n");
	return(-1);
    }
    epicsSignalInstallSigHupIgnore();
    initHooks(initHookAtBeginning);
    coreRelease();
    /* After this point, further calls to iocInit() are disallowed.  */
    initialized = TRUE;

    taskwdInit();
    callbackInit();
    /* The following forces iocClockInit to be called on vxWorks.
       This is a kludge so that TSinit can be used before iocInit.
    */
    epicsTimeGetCurrent(&timeStamp);
    /* let threads start */
    epicsThreadSleep(.1);
    initHooks(initHookAfterCallbackInit);
    dbCaLinkInit(); initHooks(initHookAfterCaLinkInit);
    initDrvSup(); initHooks(initHookAfterInitDrvSup);
    initRecSup(); initHooks(initHookAfterInitRecSup);
    initDevSup(); initHooks(initHookAfterInitDevSup);

    initDatabase();
    dbLockInitRecords(pdbbase);
    initHooks(initHookAfterInitDatabase);

    finishDevSup(); initHooks(initHookAfterFinishDevSup);

    scanInit();
    if(asInit()) {
	errlogPrintf("iocInit: asInit Failed during initialization\n");
	return(-1);
    }
    dbPutNotifyInit();
    epicsThreadSleep(.5);
    initHooks(initHookAfterScanInit);

    initialProcess(); initHooks(initHookAfterInitialProcess);

   /* Enable scan tasks and some driver support functions.  */
    interruptAccept=TRUE; initHooks(initHookAfterInterruptAccept);
    epicsThreadSleep(1.0);

    dbBkptInit();

   /*  Start up CA server */
    rsrv_init();

    errlogPrintf("iocInit: All initialization complete\n");
    initHooks(initHookAtEnd);
    return(0);
}

LOCAL void initDrvSup(void) /* Locate all driver support entry tables */
{
    drvSup	*pdrvSup;
    struct drvet *pdrvet;

    for(pdrvSup = (drvSup *)ellFirst(&pdbbase->drvList); pdrvSup;
    pdrvSup = (drvSup *)ellNext(&pdrvSup->node)) {
	pdrvet = registryDriverSupportFind(pdrvSup->name);
	if(pdrvet==0) {
            errlogPrintf("iocInit: driver %s not found\n",pdrvSup->name);
	    continue;
	}
        pdrvSup->pdrvet = pdrvet;
       /*
        *   If an initialization routine is defined (not NULL),
        *      for the driver support call it.
        */
	if(pdrvet->init) (*(pdrvet->init))();
    }
    return;
}

LOCAL void initRecSup(void)
{
    dbRecordType *pdbRecordType;
    recordTypeLocation *precordTypeLocation;
    struct rset *prset;
    
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        precordTypeLocation = registryRecordTypeFind(pdbRecordType->name);
	if (precordTypeLocation==0) {
            errlogPrintf("iocInit record support for %s not found\n",
                pdbRecordType->name);
	    continue;
	}
	prset = precordTypeLocation->prset;
        pdbRecordType->prset = prset;
	if(prset->init) (*prset->init)();
    }
    return;
}

static long do_nothing(struct dbCommon *precord) { return 0; }

/* Dummy DSXT used for soft device supports */
struct dsxt devSoft_DSXT = {
    do_nothing,
    do_nothing
};

LOCAL devSup *pthisDevSup = NULL;

LOCAL void initDevSup(void)
{
    dbRecordType	*pdbRecordType;
    struct dset *pdset;
    
    for (pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	for (pthisDevSup = (devSup *)ellFirst(&pdbRecordType->devList);
	     pthisDevSup;
	     pthisDevSup = (devSup *)ellNext(&pthisDevSup->node)) {
	    pdset = registryDeviceSupportFind(pthisDevSup->name);
	    if (pdset==0) {
		errlogPrintf("device support %s not found\n",pthisDevSup->name);
		continue;
	    }
	    if (pthisDevSup->link_type == CONSTANT)
		pthisDevSup->pdsxt = &devSoft_DSXT;
	    pthisDevSup->pdset = pdset;
	    if (pdset->init) (*pdset->init)(0);
	}
    }
    return;
}

void devExtend(dsxt *pdsxt)
{
    if (!pthisDevSup)
	errlogPrintf("devExtend() called outside of initDevSup()\n");
    else
	pthisDevSup->pdsxt = pdsxt;
}

LOCAL void finishDevSup(void) 
{
    dbRecordType	*pdbRecordType;
    struct dset *pdset;

    for (pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	for (pthisDevSup = (devSup *)ellFirst(&pdbRecordType->devList);
	     pthisDevSup;
	     pthisDevSup = (devSup *)ellNext(&pthisDevSup->node)) {
	    pdset = pthisDevSup->pdset;
	    if (pdset && pdset->init) (*pdset->init)(1);
	}
    
    }
    return;
}

LOCAL void initDatabase(void)
{
    dbRecordType	*pdbRecordType;
    dbFldDes		*pdbFldDes;
    dbRecordNode 	*pdbRecordNode;
    devSup		*pdevSup;
    struct rset		*prset;
    struct dset		*pdset;
    dbCommon		*precord;
    DBADDR		dbaddr;
    DBLINK		*plink;
    int			j;
   
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	prset = pdbRecordType->prset;
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    if(!prset) break;
           /* Find pointer to record instance */
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
	    precord->rset = prset;
	    precord->rdes = pdbRecordType;
            precord->mlok = epicsMutexMustCreate();
	    ellInit(&(precord->mlis));

           /* Reset the process active field */
	    precord->pact=FALSE;

	    /* Init DSET NOTE that result may be NULL */
	    pdevSup = dbDTYPtoDevSup(pdbRecordType,precord->dtyp);
	    pdset = (pdevSup ? pdevSup->pdset : 0);
	    precord->dset = pdset;
	    if(prset->init_record) (*prset->init_record)(precord,0);
	}
    }

/* initDatabse cont. */
   /* Second pass to resolve links */
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	prset = pdbRecordType->prset;
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
            /* Convert all PV_LINKs to DB_LINKs or CA_LINKs */
            /* For all the links in the record type... */
	    for(j=0; j<pdbRecordType->no_links; j++) {
		pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->link_ind[j]];
		plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
		if (plink->type == PV_LINK) {
                    if(plink==&precord->tsel) recGblTSELwasModified(plink);
		    if(!(plink->value.pv_link.pvlMask&(pvlOptCA|pvlOptCP|pvlOptCPP))
		    && (dbNameToAddr(plink->value.pv_link.pvname,&dbaddr)==0)) {
			DBADDR	*pdbAddr;

			plink->type = DB_LINK;
			pdbAddr = dbCalloc(1,sizeof(struct dbAddr));
			*pdbAddr = dbaddr; /*structure copy*/;
			plink->value.pv_link.pvt = pdbAddr;
		    } else {/*It is a CA link*/
			char	*pperiod;

			if(pdbFldDes->field_type==DBF_INLINK) {
			    plink->value.pv_link.pvlMask |= pvlOptInpNative;
			}
			dbCaAddLink(plink);
			if(pdbFldDes->field_type==DBF_FWDLINK) {
			    pperiod = strrchr(plink->value.pv_link.pvname,'.');
			    if(pperiod && strstr(pperiod,"PROC")) {
				plink->value.pv_link.pvlMask |= pvlOptFWD;
                            } else {
                                errlogPrintf("%s.FLNK is a Channel Access Link "
                                    " but does not access field PROC\n",
                                     precord->name);
                            }
			}
		    }
		}
	    }
            pdevSup = dbDTYPtoDevSup(pdbRecordType,precord->dtyp);
            if (pdevSup) {
                struct dsxt *pdsxt = pdevSup->pdsxt;
                if (pdsxt && pdsxt->add_record)
                    (*pdsxt->add_record)(precord);
            }
	}
    }

    /* Call record support init_record routine - Second pass */
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	prset = pdbRecordType->prset;
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    if(!prset) break;
           /* Find pointer to record instance */
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
	    precord->rset = prset;
	    if(prset->init_record) (*prset->init_record)(precord,1);
	}
    }
    return;
}

/*
 *  Process database records at initialization if
 *     their pini (process at init) field is set.
 */
LOCAL void initialProcess(void)
{
    dbRecordType		*pdbRecordType;
    dbRecordNode 	*pdbRecordNode;
    dbCommon		*precord;
    
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
	for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
	pdbRecordNode;
	pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
	    precord = pdbRecordNode->precord;
	    if(!(precord->name[0])) continue;
	    if(!precord->pini) continue;
	    (void)dbProcess(precord);
	}
    }
    return;
}
