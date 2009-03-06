/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbCaTest.c */

/****************************************************************
*
*       Author:         Marty Kraimer
*       Date:           10APR96
*
****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "dbStaticLib.h"
#include "epicsStdioRedirect.h"
#include "link.h"
/*definitions needed because of old vs new database access*/
#undef DBR_SHORT
#undef DBR_PUT_ACKT
#undef DBR_PUT_ACKS
#undef VALID_DB_REQ
#undef INVALID_DB_REQ
/*end of conflicting definitions*/
#include "cadef.h"
#include "db_access.h"
#include "dbDefs.h"
#include "epicsPrint.h"
#include "dbCommon.h"
#include "epicsEvent.h"

/*define DB_CONVERT_GBLSOURCE because db_access.c does not include db_access.h*/
#define DB_CONVERT_GBLSOURCE

#define epicsExportSharedSymbols
#include "dbLock.h"
#include "db_access_routines.h"
#include "db_convert.h"
#include "dbCa.h"
#include "dbCaPvt.h"
#include "dbCaTest.h"


long dbcar(char *precordname, int level)
{
    DBENTRY             dbentry;
    DBENTRY             *pdbentry=&dbentry;
    long                status;
    dbCommon            *precord;
    dbRecordType        *pdbRecordType;
    dbFldDes            *pdbFldDes;
    DBLINK              *plink;
    int                 ncalinks=0;
    int                 nconnected=0;
    int                 noReadAccess=0;
    int                 noWriteAccess=0;
    unsigned long       nDisconnect=0;
    unsigned long       nNoWrite=0;
    caLink              *pca;
    int                 j;

    if (!precordname || precordname[0] == '\0' || !strcmp(precordname, "*")) {
        precordname = NULL;
        printf("CA links in all records\n\n");
    } else {
        printf("CA links in record named '%s'\n\n", precordname);
    }
    dbInitEntry(pdbbase,pdbentry);
    status = dbFirstRecordType(pdbentry);
    while (!status) {
        status = dbFirstRecord(pdbentry);
        while (!status) {
            if (precordname ?
                !strcmp(precordname, dbGetRecordName(pdbentry)) :
                !dbIsAlias(pdbentry)) {
                pdbRecordType = pdbentry->precordType;
                precord = (dbCommon *)pdbentry->precnode->precord;
                for (j=0; j<pdbRecordType->no_links; j++) {
                    pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->link_ind[j]];
                    plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
                    dbLockSetGblLock();
                    if (plink->type == CA_LINK) {
                        ncalinks++;
                        pca = (caLink *)plink->value.pv_link.pvt;
                        if (pca
                        && pca->chid
                        && (ca_field_type(pca->chid) != TYPENOTCONN)) {
                            nconnected++;
                            nDisconnect += pca->nDisconnect;
                            nNoWrite += pca->nNoWrite;
                            if (!ca_read_access(pca->chid)) noReadAccess++;
                            if (!ca_write_access(pca->chid)) noWriteAccess++;
                            if (level>1) {
                                int rw = ca_read_access(pca->chid) |
                                         ca_write_access(pca->chid) << 1;
                                static const char *rights[4] = {
                                    "No Access", "Read Only",
                                    "Write Only", "Read/Write"
                                };
                                int mask = plink->value.pv_link.pvlMask;
                                printf("%28s.%-4s ==> %-28s  (%lu, %lu)\n",
                                    precord->name,
                                    pdbFldDes->name,
                                    plink->value.pv_link.pvname,
                                    pca->nDisconnect,
                                    pca->nNoWrite);
                                printf("%21s [%s%s%s%s] host %s, %s\n", "",
                                    mask & pvlOptInpNative ? "IN" : "  ",
                                    mask & pvlOptInpString ? "IS" : "  ",
                                    mask & pvlOptOutNative ? "ON" : "  ",
                                    mask & pvlOptOutString ? "OS" : "  ",
                                    ca_host_name(pca->chid),
                                    rights[rw]);
                            }
                        } else {
                            if (level>0) {
                                printf("%28s.%-4s --> %-28s  (%lu, %lu)\n",
                                    precord->name,
                                    pdbFldDes->name,
                                    plink->value.pv_link.pvname,
                                    pca->nDisconnect,
                                    pca->nNoWrite);
                            }
                        }
                    }
                    dbLockSetGblUnlock();
                }
                if (precordname) goto done;
            }
            status = dbNextRecord(pdbentry);
        }
        status = dbNextRecordType(pdbentry);
    }
done:
    if ((level > 1 && nconnected > 0) ||
        (level > 0 && ncalinks != nconnected)) printf("\n");
    printf("Total %d CA link%s; ",
           ncalinks, (ncalinks != 1) ? "s" : "");
    printf("%d connected, %d not connected.\n",
           nconnected, (ncalinks - nconnected));
    printf("    %d can't read, %d can't write.",
           noReadAccess, noWriteAccess);
    printf("  (%lu disconnects, %lu writes prohibited)\n\n",
           nDisconnect, nNoWrite);
    dbFinishEntry(pdbentry);
    
    if ( level > 2  && dbCaClientContext != 0 ) {
        ca_context_status ( dbCaClientContext, level - 2 );
    }

    return(0);
}

void dbcaStats(int *pchans, int *pdiscon)
{
    DBENTRY     dbentry;
    DBENTRY     *pdbentry = &dbentry;
    long        status;
    DBLINK      *plink;
    long        ncalinks = 0;
    long        nconnected = 0;

    dbInitEntry(pdbbase,pdbentry);
    status = dbFirstRecordType(pdbentry);
    while (!status) {
        dbRecordType *pdbRecordType = pdbentry->precordType;

        status = dbFirstRecord(pdbentry);
        while (!status) {
            dbCommon *precord = (dbCommon *)pdbentry->precnode->precord;
            int j;

            if (!dbIsAlias(pdbentry)) {
                for (j=0; j<pdbRecordType->no_links; j++) {
                    int i = pdbRecordType->link_ind[j];

                    dbFldDes *pdbFldDes = pdbRecordType->papFldDes[i];
                    plink = (DBLINK *)((char *)precord + pdbFldDes->offset);
                    if (plink->type == CA_LINK) {
                        caLink *pca = (caLink *)plink->value.pv_link.pvt;

                        ncalinks++;
                        if (pca && ca_state(pca->chid) == cs_conn) {
                            nconnected++;
                        }
                    }
                }
            }
            status = dbNextRecord(pdbentry);
        }
        status = dbNextRecordType(pdbentry);
    }
    dbFinishEntry(pdbentry);
    if (pchans)  *pchans  = ncalinks;
    if (pdiscon) *pdiscon = ncalinks - nconnected;
}
