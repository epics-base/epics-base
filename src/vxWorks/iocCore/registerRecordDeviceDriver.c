/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "dbStaticLib.h"
#include "recSup.h"
#include "devSup.h"
#include "drvSup.h"
#include "cantProceed.h"
#include "epicsFindSymbol.h"
#define epicsExportSharedSymbols
#include "registry.h"
#include "registryRecordType.h"
#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"

static void *locateAddrName(char *name)
{
    char pname[100];
    void *addr;

    addr = epicsFindSymbol(name);
    if(addr) return(addr);
    strcpy(pname,"p");
    strcat(pname,name);
    addr = epicsFindSymbol(pname);
    if(addr) return((*(void **)addr));
    return(0);
}

int registerRecordDeviceDriver(struct dbBase *pdbbase)
{
    dbRecordType *pdbRecordType;
    recordTypeLocation rtl;
    recordTypeLocation *precordTypeLocation;
    char name[100];
    drvSup *pdrvSup;

    if(!pdbbase) {
        printf("pdbbase not specified\n");
        return(0);
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        computeSizeOffset sizeOffset;
        if(registryRecordTypeFind(pdbRecordType->name)) continue;
        strcpy(name,pdbRecordType->name);
        strcat(name,"RSET");
        rtl.prset = (rset *)locateAddrName(name);
        if(!rtl.prset) {
            printf("RSET %s not found\n",name);
            continue;
        }
        strcpy(name,pdbRecordType->name);
        strcat(name,"RecordSizeOffset");
        rtl.sizeOffset = (computeSizeOffset)locateAddrName(name);
        if(!rtl.sizeOffset) {
            printf("SizeOfset %s not found\n",name);
            continue;
        }
        precordTypeLocation = callocMustSucceed(1,sizeof(recordTypeLocation),
            "registerRecordDeviceDriver");
        precordTypeLocation->prset = rtl.prset;
        precordTypeLocation->sizeOffset = rtl.sizeOffset;
        if(!registryRecordTypeAdd(pdbRecordType->name,precordTypeLocation)) {
            errlogPrintf("registryRecordTypeAdd failed for %s\n",
                pdbRecordType->name);
            continue;
        }
        sizeOffset = precordTypeLocation->sizeOffset;
        sizeOffset(pdbRecordType);
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        devSup *pdevSup;
        for(pdevSup = (devSup *)ellFirst(&pdbRecordType->devList);
        pdevSup;
        pdevSup = (devSup *)ellNext(&pdevSup->node)) {
            dset *pdset = (dset *)locateAddrName(pdevSup->name);
            if(!pdset) {
                printf("DSET %s not found\n",pdevSup->name);
                continue;
            }
            if(!registryDeviceSupportAdd(pdevSup->name,pdset)) {
                errlogPrintf("registryRecordTypeAdd failed for %s\n",
                    pdevSup->name);
            }
        }
    }
    for(pdrvSup = (drvSup *)ellFirst(&pdbbase->drvList);
    pdrvSup;
    pdrvSup = (drvSup *)ellNext(&pdrvSup->node)) {
        drvet *pdrvet = (drvet *)locateAddrName(pdrvSup->name);
        if(!pdrvet) {
            printf("drvet %s not found\n",pdrvSup->name);
            continue;
        }
        if(!registryDriverSupportAdd(pdrvSup->name,pdrvet)) {
            errlogPrintf("registryRecordTypeAdd failed for %s\n",
                pdrvSup->name);
        }
    }
    return(0);
}

