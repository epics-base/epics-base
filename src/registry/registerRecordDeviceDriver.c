#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "recSup.h"
#include "devSup.h"
#include "drvSup.h"
#include "cantProceed.h"
#include "registry.h"
#include "registryRecordType.h"
#include "registryDeviceSupport.h"
#include "registryDriverSupport.h"

int registerRecordDeviceDriver(DBBASE *pdbbase)
{
    dbRecordType *pdbRecordType;
    recordTypeLocation *precordTypeLocation;
    struct rset *prset;
    computeSizeOffset sizeOffset;
    char name[100];
    drvSup *pdrvSup;

    if(!pdbbase) {
        printf("pdbbase not specified\n");
        return(0);
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        if(registryRecordTypeFind(pdbRecordType->name)) continue;
        strcpy(name,pdbRecordType->name);
        strcat(name,"RSET");
        prset = 0;
        prset = registryFind(0,name);
        if(!prset) continue;
        strcpy(name,pdbRecordType->name);
        strcat(name,"RecordSizeOffset");
        sizeOffset = 0;
        sizeOffset = (computeSizeOffset) registryFind(0,name);
        if(!sizeOffset) continue;
        precordTypeLocation = callocMustSucceed(1,sizeof(recordTypeLocation),
            "registerRecordDeviceDriver");
        precordTypeLocation->prset = prset;
        precordTypeLocation->sizeOffset = sizeOffset;
        if(!registryRecordTypeAdd(pdbRecordType->name,precordTypeLocation)) {
            errlogPrintf("registryRecordTypeAdd failed for %s\n",
                pdbRecordType->name);
            continue;
        }
        sizeOffset(pdbRecordType);
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        devSup *pdevSup;
        struct dset *pdset;
        for(pdevSup = (devSup *)ellFirst(&pdbRecordType->devList);
        pdevSup;
        pdevSup = (devSup *)ellNext(&pdevSup->node)) {
            if(registryDeviceSupportFind(pdevSup->name)) continue;
            pdset = registryFind(0,pdevSup->name);
            if(!pdset) continue;
            if(!registryDeviceSupportAdd(pdevSup->name,pdset)) {
                errlogPrintf("registryRecordTypeAdd failed for %s\n",
                    pdevSup->name);
            }
        }
    }
    for(pdrvSup = (drvSup *)ellFirst(&pdbbase->drvList);
    pdrvSup;
    pdrvSup = (drvSup *)ellNext(&pdrvSup->node)) {
        struct drvet *pdrvet = 0;
        if(registryDriverSupportFind(pdrvSup->name)) continue;
        pdrvet = registryFind(0,pdrvSup->name);
        if(!pdrvet) continue;
        if(!registryDriverSupportAdd(pdrvSup->name,pdrvet)) {
            errlogPrintf("registryRecordTypeAdd failed for %s\n",
                pdrvSup->name);
        }
    }
    return(0);
}
