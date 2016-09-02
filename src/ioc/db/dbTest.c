/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* database access test subroutines */

#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "dbDefs.h"
#include "ellLib.h"
#include "epicsMutex.h"
#include "epicsStdio.h"
#include "epicsStdlib.h"
#include "epicsString.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "callback.h"
#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbCommon.h"
#include "dbEvent.h"
#include "db_field_log.h"
#include "dbFldTypes.h"
#include "dbLock.h"
#include "dbStaticLib.h"
#include "dbTest.h"
#include "devSup.h"
#include "drvSup.h"
#include "link.h"
#include "recGbl.h"
#include "recSup.h"
#include "special.h"

#define MAXLINE 80
struct msgBuff {    /* line output structure */
    char            out_buff[MAXLINE + 1];
    char           *pNext;
    char           *pLast;
    char           *pNexTab;
    char            message[128];
};
typedef struct msgBuff TAB_BUFFER;

#ifndef MIN
#   define MIN(x,y)  (((x) < (y)) ? (x) : (y))
#endif
#ifndef MAX
#   define MAX(x,y)  (((x) < (y)) ? (x) : (y))
#endif

/* Local Routines */
static long nameToAddr(const char *pname, DBADDR *paddr);
static void printDbAddr(DBADDR *paddr);
static void printBuffer(long status, short dbr_type, void *pbuffer,
    long reqOptions, long retOptions, long no_elements,
    TAB_BUFFER *pMsgBuff, int tab_size);
static int dbpr_report(const char *pname, DBADDR *paddr, int interest_level,
    TAB_BUFFER *pMsgBuff, int tab_size);
static void dbpr_msgOut(TAB_BUFFER *pMsgBuff,int tab_size);
static void dbpr_init_msg(TAB_BUFFER *pMsgBuff,int tab_size);
static void dbpr_insert_msg(TAB_BUFFER *pMsgBuff,size_t len,int tab_size);
static void dbpr_msg_flush(TAB_BUFFER *pMsgBuff,int tab_size);

static char *dbf[DBF_NTYPES] = {
    "STRING","CHAR","UCHAR","SHORT","USHORT","LONG","ULONG",
    "FLOAT","DOUBLE","ENUM","MENU","DEVICE",
    "INLINK","OUTLINK","FWDLINK","NOACCESS"
};

static char *dbr[DBR_ENUM+2] = {
    "STRING","CHAR","UCHAR","SHORT","USHORT","LONG","ULONG",
    "FLOAT","DOUBLE","ENUM","NOACCESS"
};

long dba(const char*pname)
{
    DBADDR addr;

    if (!pname || !*pname) {
        printf("Usage: dba \"pv name\"\n");
        return 1;
    }

    if (nameToAddr(pname, &addr))
        return -1;

    printDbAddr(&addr);
    return 0;
}

long dbl(const char *precordTypename, const char *fields)
{
    DBENTRY dbentry;
    DBENTRY *pdbentry=&dbentry;
    long status;
    int nfields = 0;
    int ifield;
    char *fieldnames = 0;
    char **papfields = 0;

    if (!pdbbase) {
        printf("No database loaded\n");
        return 0;
    }

    if (precordTypename &&
        ((*precordTypename == '\0') || !strcmp(precordTypename,"*")))
        precordTypename = NULL;
    if (fields && (*fields == '\0'))
        fields = NULL;
    if (fields) {
        char *pnext;

        fieldnames = epicsStrDup(fields);
        nfields = 1;
        pnext = fieldnames;
        while (*pnext && (pnext = strchr(pnext,' '))) {
            nfields++;
            while (*pnext == ' ') pnext++;
        }
        papfields = dbCalloc(nfields,sizeof(char *));
        pnext = fieldnames;
        for (ifield = 0; ifield < nfields; ifield++) {
            papfields[ifield] = pnext;
            if (ifield < nfields - 1) {
                pnext = strchr(pnext, ' ');
                *pnext++ = 0;
                while (*pnext == ' ') pnext++;
            }
        }
    }
    dbInitEntry(pdbbase, pdbentry);
    if (!precordTypename)
        status = dbFirstRecordType(pdbentry);
    else
        status = dbFindRecordType(pdbentry,precordTypename);
    if (status) {
        printf("No record type\n");
    }

    while (!status) {
        status = dbFirstRecord(pdbentry);
        while (!status) {
            printf("%s", dbGetRecordName(pdbentry));
            for (ifield = 0; ifield < nfields; ifield++) {
                char *pvalue;
                status = dbFindField(pdbentry, papfields[ifield]);
                if (status) {
                    if (!strcmp(papfields[ifield], "recordType")) {
                        pvalue = dbGetRecordTypeName(pdbentry);
                    }
                    else {
                        printf(", ");
                        continue;
                    }
                }
                else {
                    pvalue = dbGetString(pdbentry);
                }
                printf(", \"%s\"", pvalue ? pvalue : "");
            }
            printf("\n");
            status = dbNextRecord(pdbentry);
        }
        if (precordTypename)
            break;

        status = dbNextRecordType(pdbentry);
    }
    if (nfields > 0) {
        free((void *)papfields);
        free((void *)fieldnames);
    }
    dbFinishEntry(pdbentry);
    return 0;
}

long dbnr(int verbose)
{
    DBENTRY dbentry;
    DBENTRY *pdbentry=&dbentry;
    long status;
    int nrecords;
    int naliases;
    int trecords = 0;
    int taliases = 0;

    if (!pdbbase) {
        printf("No database loaded\n");
        return 0;
    }

    dbInitEntry(pdbbase, pdbentry);
    status = dbFirstRecordType(pdbentry);
    if (status) {
        printf("No record types loaded\n");
        return 0;
    }

    printf("Records  Aliases  Record Type\n");
    while (!status) {
        naliases = dbGetNAliases(pdbentry);
        taliases += naliases;
        nrecords = dbGetNRecords(pdbentry) - naliases;
        trecords += nrecords;
        if (verbose || nrecords)
            printf(" %5d    %5d    %s\n",
                nrecords, naliases, dbGetRecordTypeName(pdbentry));
        status = dbNextRecordType(pdbentry);
    }

    dbFinishEntry(pdbentry);
    printf("Total %d records, %d aliases\n", trecords, taliases);
    return 0;
}

long dbla(const char *pmask)
{
    DBENTRY dbentry;
    DBENTRY *pdbentry = &dbentry;
    long status;

    if (!pdbbase) {
        printf("No database loaded\n");
        return 0;
    }

    dbInitEntry(pdbbase, pdbentry);
    status = dbFirstRecordType(pdbentry);
    while (!status) {
        for (status = dbFirstRecord(pdbentry); !status;
             status = dbNextRecord(pdbentry)) {
            char *palias;

            if (!dbIsAlias(pdbentry))
                continue;

            palias = dbGetRecordName(pdbentry);
            if (pmask && *pmask && !epicsStrGlobMatch(palias, pmask))
                continue;
            dbFindField(pdbentry, "NAME");
            printf("%s -> %s\n", palias, dbGetString(pdbentry));
        }
        status = dbNextRecordType(pdbentry);
    }

    dbFinishEntry(pdbentry);
    return 0;
}

long dbgrep(const char *pmask)
{
    DBENTRY dbentry;
    DBENTRY *pdbentry = &dbentry;
    long status;

    if (!pmask || !*pmask) {
        printf("Usage: dbgrep \"pattern\"\n");
        return 1;
    }

    if (!pdbbase) {
        printf("No database loaded\n");
        return 0;
    }

    dbInitEntry(pdbbase, pdbentry);
    status = dbFirstRecordType(pdbentry);
    while (!status) {
        status = dbFirstRecord(pdbentry);
        while (!status) {
            char *pname = dbGetRecordName(pdbentry);
            if (epicsStrGlobMatch(pname, pmask))
                puts(pname);
            status = dbNextRecord(pdbentry);
        }
        status = dbNextRecordType(pdbentry);
    }

    dbFinishEntry(pdbentry);
    return 0;
}

long dbgf(const char *pname)
{
    /* declare buffer long just to ensure correct alignment */
    long buffer[100];
    long *pbuffer=&buffer[0];
    DBADDR addr;
    long options = 0;
    long no_elements;
    static TAB_BUFFER msg_Buff;

    if (!pname || !*pname) {
        printf("Usage: dbgf \"pv name\"\n");
        return 1;
    }

    if (nameToAddr(pname, &addr))
        return -1;

    no_elements = MIN(addr.no_elements, sizeof(buffer)/addr.field_size);
    if (addr.dbr_field_type == DBR_ENUM) {
        long status = dbGetField(&addr, DBR_STRING, pbuffer,
            &options, &no_elements, NULL);

        printBuffer(status, DBR_STRING, pbuffer, 0L, 0L,
            no_elements, &msg_Buff, 10);
    }
    else {
        long status = dbGetField(&addr, addr.dbr_field_type, pbuffer,
            &options, &no_elements, NULL);

        printBuffer(status, addr.dbr_field_type, pbuffer, 0L, 0L,
            no_elements, &msg_Buff, 10);
    }

    msg_Buff.message[0] = '\0';
    dbpr_msgOut(&msg_Buff, 10);
    return 0;
}

long dbpf(const char *pname,const char *pvalue)
{
    DBADDR addr;
    long status;
    short dbrType;
    size_t n = 1;

    if (!pname || !*pname || !pvalue) {
        printf("Usage: dbpf \"pv name\", \"value\"\n");
        return 1;
    }

    if (nameToAddr(pname, &addr))
        return -1;

    if (addr.no_elements > 1 &&
        (addr.dbr_field_type == DBR_CHAR || addr.dbr_field_type == DBR_UCHAR)) {
        dbrType = addr.dbr_field_type;
        n = strlen(pvalue) + 1;
    }
    else {
        dbrType = DBR_STRING;
    }

    status = dbPutField(&addr, dbrType, pvalue, (long) n);
    dbgf(pname);
    return status;
}

long dbpr(const char *pname,int interest_level)
{
    static TAB_BUFFER msg_Buff;
    TAB_BUFFER *pMsgBuff = &msg_Buff;
    DBADDR addr;
    char *pmsg;
    int tab_size = 20;

    if (!pname || !*pname) {
        printf("Usage: dbpr \"pv name\", level\n");
        return 1;
    }

    if (nameToAddr(pname, &addr))
        return -1;

    pmsg = pMsgBuff->message;

    if (dbpr_report(pname, &addr, interest_level, pMsgBuff, tab_size))
        return 1;

    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return 0;
}

long dbtr(const char *pname)
{
    DBADDR addr;
    long status;
    struct dbCommon *precord;

    if (!pname || !*pname) {
        printf("Usage: dbtr \"pv name\"\n");
        return 1;
    }

    if (nameToAddr(pname, &addr))
        return -1;

    precord = (struct dbCommon*)addr.precord;
    if (precord->pact) {
        printf("record active\n");
        return 1;
    }

    dbScanLock(precord);
    status = dbProcess(precord);
    dbScanUnlock(precord);

    if (status)
        recGblRecordError(status, precord, "dbtr(dbProcess)");

    dbpr(pname, 3);
    return 0;
}

long dbtgf(const char *pname)
{
    /* declare buffer long just to ensure correct alignment */
    long       buffer[400];
    long       *pbuffer = &buffer[0];
    DBADDR     addr;
    long       status;
    long       req_options, ret_options, no_elements;
    short      dbr_type;
    static TAB_BUFFER msg_Buff;
    TAB_BUFFER *pMsgBuff = &msg_Buff;
    char       *pmsg = pMsgBuff->message;
    int        tab_size = 10;

    if (pname==0 || *pname==0) {
        printf("Usage: dbtgf \"pv name\"\n");
        return 1;
    }

    if (nameToAddr(pname, &addr))
        return -1;

    /* try all options first */
    req_options = 0xffffffff;
    ret_options = req_options;
    no_elements = 0;
    status = dbGetField(&addr, addr.dbr_field_type, pbuffer,
        &ret_options, &no_elements, NULL);
    printBuffer(status, addr.dbr_field_type, pbuffer,
        req_options, ret_options, no_elements, pMsgBuff, tab_size);

    /* Now try all request types */
    ret_options=0;

    dbr_type = DBR_STRING;
    no_elements = MIN(addr.no_elements,((sizeof(buffer))/MAX_STRING_SIZE));
    status = dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);

    dbr_type = DBR_CHAR;
    no_elements = MIN(addr.no_elements,((sizeof(buffer))/sizeof(epicsInt8)));
    status = dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);

    dbr_type = DBR_UCHAR;
    no_elements = MIN(addr.no_elements,((sizeof(buffer))/sizeof(epicsUInt8)));
    status = dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);

    dbr_type = DBR_SHORT;
    no_elements = MIN(addr.no_elements,((sizeof(buffer))/sizeof(epicsInt16)));
    status = dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);

    dbr_type = DBR_USHORT;
    no_elements = MIN(addr.no_elements,((sizeof(buffer))/sizeof(epicsUInt16)));
    status = dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);

    dbr_type = DBR_LONG;
    no_elements = MIN(addr.no_elements,((sizeof(buffer))/sizeof(epicsInt32)));
    status = dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);

    dbr_type = DBR_ULONG;
    no_elements = MIN(addr.no_elements,((sizeof(buffer))/sizeof(epicsUInt32)));
    status = dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);

    dbr_type = DBR_FLOAT;
    no_elements = MIN(addr.no_elements,((sizeof(buffer))/sizeof(epicsFloat32)));
    status = dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);

    dbr_type = DBR_DOUBLE;
    no_elements = MIN(addr.no_elements,((sizeof(buffer))/sizeof(epicsFloat64)));
    status = dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);

    dbr_type = DBR_ENUM;
    no_elements = MIN(addr.no_elements,((sizeof(buffer))/sizeof(epicsEnum16)));
    status = dbGetField(&addr,dbr_type,pbuffer,&ret_options,&no_elements,NULL);
    printBuffer(status,dbr_type,pbuffer,0L,0L,no_elements,pMsgBuff,tab_size);

    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return(0);
}

long dbtpf(const char *pname, const char *pvalue)
{
    /* declare buffer long just to ensure correct alignment */
    long buffer[100];
    long *pbuffer = buffer;
    DBADDR addr;
    long status = 0;
    long options, no_elements;
    char *pend;
    long val_long;
    int validLong;
    unsigned long val_ulong;
    int validULong;
    int valid = 1;
    int put_type;
    epicsFloat32 fvalue;
    epicsFloat64 dvalue;
    static TAB_BUFFER msg_Buff;
    TAB_BUFFER *pMsgBuff = &msg_Buff;
    char *pmsg = pMsgBuff->message;
    int tab_size = 10;

    if (!pname || !*pname || !pvalue) {
        printf("Usage: dbtpf \"pv name\", \"value\"\n");
        return 1;
    }

    if (nameToAddr(pname, &addr))
        return -1;

    val_long = strtol(pvalue, &pend, 10);
    validLong = (*pend == 0);

    val_ulong = strtoul(pvalue, &pend, 10);
    validULong = (*pend == 0);

    for (put_type = DBR_STRING; put_type <= DBF_ENUM; put_type++) {
        switch (put_type) {
        case DBR_STRING:
            status = dbPutField(&addr, put_type, pvalue, 1L);
            break;
        case DBR_CHAR:
            if ((valid = validLong)) {
                epicsInt8 val_i8 = (epicsInt8)val_long;
                status = dbPutField(&addr, put_type, &val_i8, 1L);
            }
            break;
        case DBR_UCHAR:
            if ((valid = validULong)) {
                epicsUInt8 val_u8 = (epicsUInt8)val_ulong;
                status = dbPutField(&addr, put_type, &val_u8, 1L);
            }
            break;
        case DBR_SHORT:
            if ((valid = validLong)) {
                epicsInt16 val_i16 = (epicsInt16) val_long;
                status = dbPutField(&addr, put_type, &val_i16,1L);
            }
            break;
        case DBR_USHORT:
            if ((valid = validULong)) {
                epicsUInt16 val_u16 = (epicsUInt16) val_ulong;
                status = dbPutField(&addr, put_type, &val_u16, 1L);
            }
            break;
        case DBR_LONG:
            if ((valid = validLong)) {
                epicsInt32 val_i32 = val_long;
                status = dbPutField(&addr, put_type,&val_i32,1L);
            }
            break;
        case DBR_ULONG:
            if ((valid = validULong)) {
                epicsUInt32 val_u32 = val_ulong;
                status = dbPutField(&addr, put_type, &val_u32, 1L);
            }
            break;
        case DBR_FLOAT:
            if ((valid = epicsScanFloat(pvalue, &fvalue) == 1))
                status = dbPutField(&addr, put_type, &fvalue, 1L);
            break;
        case DBR_DOUBLE:
            if ((valid = epicsScanDouble(pvalue, &dvalue) == 1))
                status = dbPutField(&addr, put_type, &dvalue, 1L);
            break;
        case DBR_ENUM:
            if ((valid = validULong)) {
                epicsEnum16 val_e16 = (epicsEnum16) val_ulong;
                status = dbPutField(&addr, put_type, &val_e16, 1L);
            }
            break;
        }
        if (valid) {
            if (status) {
                printf("Put as DBR_%s Failed.\n", dbr[put_type]);
            } else {
                printf("Put as DBR_%-6s Ok, result as ", dbr[put_type]);
                no_elements = MIN(addr.no_elements,
                    ((sizeof(buffer))/addr.field_size));
                options = 0;
                status = dbGetField(&addr, addr.dbr_field_type, pbuffer,
                    &options, &no_elements, NULL);
                printBuffer(status, addr.dbr_field_type, pbuffer, 0L, 0L,
                    no_elements, pMsgBuff, tab_size);
            }
        }
    }

    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    return(0);
}

long dbior(const char *pdrvName,int interest_level)
{
    drvSup *pdrvSup;
    struct drvet *pdrvet;
    dbRecordType *pdbRecordType;

    if (!pdbbase) {
        printf("No database loaded\n");
        return 0;
    }

    if (pdrvName && ((*pdrvName == '\0') || !strcmp(pdrvName,"*")))
        pdrvName = NULL;

    for (pdrvSup = (drvSup *)ellFirst(&pdbbase->drvList);
         pdrvSup;
         pdrvSup = (drvSup *)ellNext(&pdrvSup->node)) {
        const char *pname = pdrvSup->name;

        if (pdrvName!=NULL && *pdrvName!='\0' &&
            (strcmp(pdrvName,pname)!=0))
            continue;

        pdrvet = pdrvSup->pdrvet ;
        if (pdrvet == NULL) {
            printf("No driver entry table is present for %s\n", pname);
            continue;
        }

        if (pdrvet->report == NULL)
            printf("Driver: %s No report available\n", pname);
        else {
            printf("Driver: %s\n", pname);
            pdrvet->report(interest_level);
        }
    }

    /* now check devSup reports */
    for (pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        devSup *pdevSup;

        for (pdevSup = (devSup *)ellFirst(&pdbRecordType->devList);
             pdevSup;
             pdevSup = (devSup *)ellNext(&pdevSup->node)) {
            struct dset *pdset = pdevSup->pdset;
            const char *pname  = pdevSup->name;

            if (!pdset || !pname)
                continue;

            if (pdrvName != NULL && *pdrvName != '\0' &&
                (strcmp(pdrvName, pname) != 0))
                continue;

            if (pdset->report != NULL) {
                printf("Device Support: %s\n", pname);
                pdset->report(interest_level);
            }
        }
    }
    return 0;
}

int dbhcr(void)
{
    if (!pdbbase) {
        printf("No database loaded\n");
        return 0;
    }

    dbReportDeviceConfig(pdbbase, stdout);
    return 0;
}

static long nameToAddr(const char *pname, DBADDR *paddr)
{
    long status = dbNameToAddr(pname, paddr);

    if (status) {
        printf("PV '%s' not found\n", pname);
    }
    return status;
}

static void printDbAddr(DBADDR *paddr)
{
    dbFldDes *pdbFldDes = paddr->pfldDes;
    short field_type = paddr->field_type;
    short dbr_field_type = paddr->dbr_field_type;

    printf("Record Address: %p", paddr->precord);
    printf(" Field Address: %p", paddr->pfield);
    printf(" Field Description: %p\n", pdbFldDes);
    printf("   No Elements: %ld\n", paddr->no_elements);
    printf("   Record Type: %s\n", pdbFldDes->pdbRecordType->name);
    printf("    Field Type: %d = DBF_%s\n", field_type,
        (field_type < 0 || field_type > DBR_NOACCESS) ? "????" :
         dbf[field_type]);
    printf("    Field Size: %d\n", paddr->field_size);
    printf("       Special: %d\n", paddr->special);

    if (dbr_field_type == DBR_NOACCESS)
        dbr_field_type = DBR_ENUM + 1;
    printf("DBR Field Type: %d = DBR_%s\n", paddr->dbr_field_type,
        (dbr_field_type < 0 || dbr_field_type > (DBR_ENUM+1)) ? "????" :
        dbr[dbr_field_type]);
}


static void printBuffer(
    long status, short dbr_type, void *pbuffer, long reqOptions,
    long retOptions, long no_elements, TAB_BUFFER *pMsgBuff, int tab_size)
{
    epicsInt32 val_i32;
    epicsUInt32 val_u32;
    char *pmsg = pMsgBuff->message;
    size_t i, len;

    if (reqOptions & DBR_STATUS) {
        if (retOptions & DBR_STATUS) {
            struct dbr_status *pdbr_status = (void *)pbuffer;

            printf("status = %u, severity = %u\n",
                pdbr_status->status,
                pdbr_status->severity);
        }
        else {
            printf("status and severity not returned\n");
        }
        pbuffer = (char *)pbuffer + dbr_status_size;
    }

    if (reqOptions & DBR_UNITS) {
        if (retOptions & DBR_UNITS) {
            struct dbr_units *pdbr_units = (void *)pbuffer;

            printf("units = \"%s\"\n",
                pdbr_units->units);
        }
        else {
            printf("units not returned\n");
        }
        pbuffer = (char *)pbuffer + dbr_units_size;
    }

    if (reqOptions & DBR_PRECISION) {
        if (retOptions & DBR_PRECISION){
            struct dbr_precision *pdbr_precision = (void *)pbuffer;

            printf("precision = %ld\n",
                pdbr_precision->precision.dp);
        }
        else {
            printf("precision not returned\n");
        }
        pbuffer = (char *)pbuffer + dbr_precision_size;
    }

    if (reqOptions & DBR_TIME) {
        if (retOptions & DBR_TIME) {
            struct dbr_time *pdbr_time = (void *)pbuffer;
            char time_buf[40];
            epicsTimeToStrftime(time_buf, 40, "%Y-%m-%d %H:%M:%S.%09f",
                &pdbr_time->time);
            printf("time = %s\n", time_buf);
        }
        else {
            printf("time not returned\n");
        }
        pbuffer = (char *)pbuffer + dbr_time_size;
    }

    if (reqOptions & DBR_ENUM_STRS) {
        if (retOptions & DBR_ENUM_STRS) {
            struct dbr_enumStrs *pdbr_enumStrs = (void *)pbuffer;

            printf("no_strs = %u:\n",
                pdbr_enumStrs->no_str);
            for (i = 0; i < pdbr_enumStrs->no_str; i++) 
                printf("\t\"%s\"\n", pdbr_enumStrs->strs[i]);
        }
        else {
            printf("enum strings not returned\n");
        }
        pbuffer = (char *)pbuffer + dbr_enumStrs_size;
    }

    if (reqOptions & DBR_GR_LONG) {
        if (retOptions & DBR_GR_LONG) {
            struct dbr_grLong *pdbr_grLong = (void *)pbuffer;

            printf("grLong: %d .. %d\n",
                pdbr_grLong->lower_disp_limit,
                pdbr_grLong->upper_disp_limit);
        }
        else {
            printf("DBRgrLong not returned\n");
        }
        pbuffer = (char *)pbuffer + dbr_grLong_size;
    }

    if (reqOptions & DBR_GR_DOUBLE) {
        if (retOptions & DBR_GR_DOUBLE) {
            struct dbr_grDouble *pdbr_grDouble = (void *)pbuffer;

            printf("grDouble: %g .. %g\n",
                pdbr_grDouble->lower_disp_limit,
                pdbr_grDouble->upper_disp_limit);
        }
        else {
            printf("DBRgrDouble not returned\n");
        }
        pbuffer = (char *)pbuffer + dbr_grDouble_size;
    }

    if (reqOptions & DBR_CTRL_LONG) {
        if (retOptions & DBR_CTRL_LONG){
            struct dbr_ctrlLong *pdbr_ctrlLong = (void *)pbuffer;

            printf("ctrlLong: %d .. %d\n",
                pdbr_ctrlLong->lower_ctrl_limit,
                pdbr_ctrlLong->upper_ctrl_limit);
        }
        else {
            printf("DBRctrlLong not returned\n");
        }
        pbuffer = (char *)pbuffer + dbr_ctrlLong_size;
    }

    if (reqOptions & DBR_CTRL_DOUBLE) {
        if (retOptions & DBR_CTRL_DOUBLE) {
            struct dbr_ctrlDouble *pdbr_ctrlDouble = (void *)pbuffer;

            printf("ctrlDouble: %g .. %g\n",
                pdbr_ctrlDouble->lower_ctrl_limit,
                pdbr_ctrlDouble->upper_ctrl_limit);
        }
        else {
            printf("DBRctrlDouble not returned\n");
        }
        pbuffer = (char *)pbuffer + dbr_ctrlDouble_size;
    }

    if (reqOptions & DBR_AL_LONG) {
        if (retOptions & DBR_AL_LONG) {
            struct dbr_alLong *pdbr_alLong = (void *)pbuffer;

            printf("alLong: %d < %d .. %d < %d\n",
                pdbr_alLong->lower_alarm_limit,
                pdbr_alLong->lower_warning_limit,
                pdbr_alLong->upper_warning_limit,
                pdbr_alLong->upper_alarm_limit);
        }
        else {
            printf("DBRalLong not returned\n");
        }
        pbuffer = (char *)pbuffer + dbr_alLong_size;
    }

    if (reqOptions & DBR_AL_DOUBLE) {
        if (retOptions & DBR_AL_DOUBLE) {
            struct dbr_alDouble *pdbr_alDouble = (void *)pbuffer;

            printf("alDouble: %g < %g .. %g < %g\n",
                pdbr_alDouble->lower_alarm_limit,
                pdbr_alDouble->lower_warning_limit,
                pdbr_alDouble->upper_warning_limit,
                pdbr_alDouble->upper_alarm_limit);
        }
        else {
            printf("DBRalDouble not returned\n");
        }
        pbuffer = (char *)pbuffer + dbr_alDouble_size;
    }

    /* Now print values */
    if (no_elements == 0)
        return;

    switch (dbr_type) {
    case (DBR_STRING):
        if (no_elements == 1)
            sprintf(pmsg, "DBR_STRING: ");
        else
            sprintf(pmsg, "DBR_STRING[%ld]: ", no_elements);
        dbpr_msgOut(pMsgBuff, tab_size);
        if (status != 0) {
            sprintf(pmsg, "DBR_STRING: failed.");
            dbpr_msgOut(pMsgBuff, tab_size);
            break;
        }

        for(i=0; i<no_elements; i++) {
            len = strlen(pbuffer);
            if (len > 0) {
                sprintf(pmsg, " \"%s\"", (char *)pbuffer);
                dbpr_msgOut(pMsgBuff, tab_size);
            }
            pbuffer = (char *)pbuffer + MAX_STRING_SIZE;
        }
        break;

    case (DBR_CHAR):
        if (no_elements == 1)
            sprintf(pmsg, "DBR_CHAR: ");
        else
            sprintf(pmsg, "DBR_CHAR[%ld]: ", no_elements);
        dbpr_msgOut(pMsgBuff, tab_size);
        if (status != 0) {
            sprintf(pmsg, " failed.");
            dbpr_msgOut(pMsgBuff, tab_size);
            break;
        }

        if (no_elements == 1) {
            val_i32 = *(epicsInt8 *) pbuffer;
            sprintf(pmsg, "%-9d 0x%-9x", val_i32, val_i32);
            dbpr_msgOut(pMsgBuff, tab_size);
        }
        else {
            for (i = 0; i < no_elements; i+= MAXLINE - 5) {
                int width = no_elements - i;
                if (width > MAXLINE - 5) width = MAXLINE - 5;
                sprintf(pmsg, " \"%.*s\"", width, (char *)pbuffer + i);
                if (i + MAXLINE - 5 < no_elements) strcat(pmsg, " +");
                dbpr_msgOut(pMsgBuff, tab_size);
            }
        }
        break;

    case (DBR_UCHAR):
        if (no_elements == 1)
            sprintf(pmsg, "DBR_UCHAR: ");
        else
            sprintf(pmsg, "DBR_UCHAR[%ld]: ", no_elements);
        dbpr_msgOut(pMsgBuff, tab_size);
        if (status != 0) {
            sprintf(pmsg, " failed.");
            dbpr_msgOut(pMsgBuff, tab_size);
            break;
        }

        for (i = 0; i < no_elements; i++) {
            val_u32 = *(epicsUInt8 *) pbuffer;
            sprintf(pmsg, "%-9u 0x%-9x", val_u32, val_u32);
            dbpr_msgOut(pMsgBuff, tab_size);
            pbuffer = (char *)pbuffer + sizeof(epicsUInt8);
        }
        break;

    case (DBR_SHORT):
        if (no_elements == 1)
            sprintf(pmsg, "DBR_SHORT: ");
        else
            sprintf(pmsg, "DBR_SHORT[%ld]: ", no_elements);
        dbpr_msgOut(pMsgBuff, tab_size);
        if (status != 0) {
            sprintf(pmsg, " failed.");
            dbpr_msgOut(pMsgBuff, tab_size);
            break;
        }

        for (i = 0; i < no_elements; i++) {
            val_i32 = *(epicsInt16 *) pbuffer;
            sprintf(pmsg, "%-9d 0x%-9x", val_i32, val_i32);
            dbpr_msgOut(pMsgBuff, tab_size);
            pbuffer = (char *)pbuffer + sizeof(epicsInt16);
        }
        break;

    case (DBR_USHORT):
        if (no_elements == 1)
            sprintf(pmsg, "DBR_USHORT: ");
        else
            sprintf(pmsg, "DBR_USHORT[%ld]: ", no_elements);
        dbpr_msgOut(pMsgBuff, tab_size);
        if (status != 0) {
            sprintf(pmsg, " failed.");
            dbpr_msgOut(pMsgBuff, tab_size);
            break;
        }

        for (i = 0; i < no_elements; i++) {
            val_u32 = *(epicsUInt16 *) pbuffer;
            sprintf(pmsg, "%-9u 0x%-9x", val_u32, val_u32);
            dbpr_msgOut(pMsgBuff, tab_size);
            pbuffer = (char *)pbuffer + sizeof(epicsUInt16);
        }
        break;

    case (DBR_LONG):
        if (no_elements == 1)
            sprintf(pmsg, "DBR_LONG: ");
        else
            sprintf(pmsg, "DBR_LONG[%ld]: ", no_elements);
        dbpr_msgOut(pMsgBuff, tab_size);
        if (status != 0) {
            sprintf(pmsg, " failed.");
            dbpr_msgOut(pMsgBuff, tab_size);
            break;
        }

        for (i = 0; i < no_elements; i++) {
            val_i32 = *(epicsInt32 *) pbuffer;
            sprintf(pmsg, "%-9d 0x%-9x", val_i32, val_i32);
            dbpr_msgOut(pMsgBuff, tab_size);
            pbuffer = (char *)pbuffer + sizeof(epicsInt32);
        }
        break;

    case (DBR_ULONG):
        if (no_elements == 1)
            sprintf(pmsg, "DBR_ULONG: ");
        else
            sprintf(pmsg, "DBR_ULONG[%ld]: ", no_elements);
        dbpr_msgOut(pMsgBuff, tab_size);
        if (status != 0) {
            sprintf(pmsg, " failed.");
            dbpr_msgOut(pMsgBuff, tab_size);
            break;
        }

        for (i = 0; i < no_elements; i++) {
            val_u32 = *(epicsUInt32 *) pbuffer;
            sprintf(pmsg, "%-9u 0x%-9x", val_u32, val_u32);
            dbpr_msgOut(pMsgBuff, tab_size);
            pbuffer = (char *)pbuffer + sizeof(epicsUInt32);
        }
        break;

    case (DBR_FLOAT):
        if (no_elements == 1)
            sprintf(pmsg, "DBR_FLOAT: ");
        else
            sprintf(pmsg, "DBR_FLOAT[%ld]: ", no_elements);
        dbpr_msgOut(pMsgBuff, tab_size);
        if (status != 0) {
            sprintf(pmsg, " failed.");
            dbpr_msgOut(pMsgBuff, tab_size);
            break;
        }

        for (i = 0; i < no_elements; i++) {
            sprintf(pmsg, "%-13.6g", *((epicsFloat32 *) pbuffer));
            dbpr_msgOut(pMsgBuff, tab_size);
            pbuffer = (char *)pbuffer + sizeof(epicsFloat32);
        }
        break;

    case (DBR_DOUBLE):
        if (no_elements == 1)
            sprintf(pmsg, "DBR_DOUBLE: ");
        else
            sprintf(pmsg, "DBR_DOUBLE[%ld]: ", no_elements);
        dbpr_msgOut(pMsgBuff, tab_size);
        if (status != 0) {
            sprintf(pmsg, " failed.");
            dbpr_msgOut(pMsgBuff, tab_size);
            break;
        }

        for (i = 0; i < no_elements; i++) {
            sprintf(pmsg, "%-13.6g", *((epicsFloat64 *) pbuffer));
            dbpr_msgOut(pMsgBuff, tab_size);
            pbuffer = (char *)pbuffer + sizeof(epicsFloat64);
        }
        break;

    case (DBR_ENUM):
        if (no_elements == 1)
            sprintf(pmsg, "DBR_ENUM: ");
        else
            sprintf(pmsg, "DBR_ENUM[%ld]: ", no_elements);
        dbpr_msgOut(pMsgBuff, tab_size);
        if (status != 0) {
            sprintf(pmsg, " failed.");
            dbpr_msgOut(pMsgBuff, tab_size);
            break;
        }

        for (i = 0; i < no_elements; i++) {
            sprintf(pmsg, "%-9u", *((epicsEnum16 *) pbuffer));
            dbpr_msgOut(pMsgBuff, tab_size);
            pbuffer = (char *)pbuffer + sizeof(epicsEnum16);
        }
        break;

    default:
        printf(" illegal request type.");
        break;
    }

    dbpr_msg_flush(pMsgBuff, tab_size);
    return;
}

static int dbpr_report(
    const char *pname, DBADDR *paddr, int interest_level,
    TAB_BUFFER *pMsgBuff, int tab_size)
{
    char        *pmsg;
    dbFldDes    *pdbFldDes = paddr->pfldDes;
    dbRecordType *pdbRecordType = pdbFldDes->pdbRecordType;
    short       n2;
    void        *pfield;
    char        *pfield_name;
    char        *pfield_value;
    DBENTRY     dbentry;
    DBENTRY     *pdbentry = &dbentry;
    long        status;

    dbInitEntry(pdbbase,pdbentry);
    status = dbFindRecord(pdbentry,pname);
    if (status) {
        errMessage(status,pname);
        return -1;
    }

    pmsg = pMsgBuff->message;
    for (n2 = 0; n2 <= pdbRecordType->no_fields - 1; n2++) {
        pdbFldDes = pdbRecordType->papFldDes[pdbRecordType->sortFldInd[n2]];
        pfield_name = pdbFldDes->name;
        pfield = ((char *)paddr->precord) + pdbFldDes->offset;
        if (pdbFldDes->interest > interest_level )
            continue;

        switch (pdbFldDes->field_type) {
        case DBF_STRING:
        case DBF_USHORT:
        case DBF_ENUM:
        case DBF_FLOAT:
        case DBF_CHAR:
        case DBF_UCHAR:
        case DBF_SHORT:
        case DBF_LONG:
        case DBF_ULONG:
        case DBF_DOUBLE:
        case DBF_MENU:
        case DBF_DEVICE:
            status = dbFindField(pdbentry,pfield_name);
            pfield_value = dbGetString(pdbentry);
            sprintf(pmsg, "%s: %s", pfield_name,
                (pfield_value ? pfield_value : "<nil>"));
            dbpr_msgOut(pMsgBuff, tab_size);
            break;

        case DBF_INLINK:
        case DBF_OUTLINK:
        case DBF_FWDLINK: {
                DBLINK  *plink = (DBLINK *)pfield;
                int     ind;

                status = dbFindField(pdbentry,pfield_name);
                for (ind=0; ind<LINK_NTYPES; ind++) {
                    if (pamaplinkType[ind].value == plink->type)
                        break;
                }
                if (ind>=LINK_NTYPES) {
                    sprintf(pmsg,"%s: Illegal Link Type", pfield_name);
                }
                else {
                    sprintf(pmsg,"%s:%s %s", pfield_name,
                        pamaplinkType[ind].strvalue,dbGetString(pdbentry));
                }
                dbpr_msgOut(pMsgBuff, tab_size);
            }
            break;

        case DBF_NOACCESS:
            if (pfield == (void *)&paddr->precord->time) {
                /* Special for the TIME field, make it human-readable */
                char time_buf[40];
                epicsTimeToStrftime(time_buf, 40, "%Y-%m-%d %H:%M:%S.%09f",
                    &paddr->precord->time);
                sprintf(pmsg, "%s: %s", pfield_name, time_buf);
                dbpr_msgOut(pMsgBuff, tab_size);
            }
            else if (pdbFldDes->size == sizeof(void *) &&
                strchr(pdbFldDes->extra, '*')) {
                /* Special for pointers, needed on little-endian CPUs */
                sprintf(pmsg, "%s: %p", pfield_name, *(void **)pfield);
                dbpr_msgOut(pMsgBuff, tab_size);
            }
            else { /* just print field as hex bytes */
                unsigned char *pchar = (unsigned char *)pfield;
                char   temp_buf[61];
                char *ptemp_buf = &temp_buf[0];
                short n = pdbFldDes->size;
                short i;
                unsigned int value;

                if (n > sizeof(temp_buf)/3) n = sizeof(temp_buf)/3;
                for (i=0; i<n; i++, ptemp_buf += 3, pchar++) {
                        value = (unsigned int)*pchar;
                        sprintf(ptemp_buf, "%02x ", value);
                }
                sprintf(pmsg, "%s: %s", pfield_name,temp_buf);
                dbpr_msgOut(pMsgBuff, tab_size);
            }
            break;

        default:
            sprintf(pmsg, "%s: dbpr: Unknown field_type", pfield_name);
            dbpr_msgOut(pMsgBuff, tab_size);
            break;
        }
    }
    pmsg[0] = '\0';
    dbpr_msgOut(pMsgBuff, tab_size);
    dbFinishEntry(pdbentry);
    return (0);
}

static void dbpr_msgOut(TAB_BUFFER *pMsgBuff,int tab_size)
{
    size_t     len;
    int        err = 0;
    char       *pmsg = pMsgBuff->message;
    static int last_tabsize;

    if (!((tab_size == 10) || (tab_size == 20))) {
        printf("tab_size not 10 or 20 - dbpr_msgOut()\n");
        return;
    }
    /* init if first time */
    if (!(pMsgBuff->pNext))
        dbpr_init_msg(pMsgBuff, tab_size);
    if (tab_size != last_tabsize)
        pMsgBuff->pNexTab = pMsgBuff->out_buff + tab_size;

    last_tabsize = tab_size;
    /* flush output if NULL string command */
    if (*pmsg == 0) {
        dbpr_msg_flush(pMsgBuff, tab_size);
        return;
    }
    /* truncate if too long */
    len = strlen(pmsg);
    if (len > MAXLINE) {
        err = 1;
        len = MAXLINE;
    }

    dbpr_insert_msg(pMsgBuff, len, tab_size);

    /* warn if msg gt 80 */
    if (err == 1) {
        len = strlen(pmsg);
        sprintf(pmsg, "dbpr_msgOut: ERROR - msg length=%d limit=%d ",
            (int)len, MAXLINE);
        dbpr_insert_msg(pMsgBuff, len, tab_size);
    }
}

static void dbpr_init_msg(TAB_BUFFER *pMsgBuff,int tab_size)
{
    pMsgBuff->pNext = pMsgBuff->out_buff;
    pMsgBuff->pLast = pMsgBuff->out_buff + MAXLINE;
    pMsgBuff->pNexTab = pMsgBuff->out_buff + tab_size;
}

static void dbpr_insert_msg(TAB_BUFFER *pMsgBuff,size_t len,int tab_size)
{
    size_t          current_len;
    size_t          n;
    size_t          tot_line;
    char           *pmsg = pMsgBuff->message;
    current_len = strlen(pMsgBuff->out_buff);
    tot_line = current_len + len;

    /* flush buffer if overflow would occor */
    if (tot_line > MAXLINE)
        dbpr_msg_flush(pMsgBuff, tab_size);

    /* append message to buffer */
    n = 0;
    while ((*pmsg) && (n < len)) {
        *pMsgBuff->pNext++ = *pmsg++;

        /* position to next tab stop */
        if (*(pMsgBuff->pNexTab - 1) != '\0')
            pMsgBuff->pNexTab = pMsgBuff->pNexTab + tab_size;
        n++;
    }

    /* fill spaces to next tab stop */
    while (*(pMsgBuff->pNexTab - 1) != ' '
            && pMsgBuff->pNext < pMsgBuff->pLast) {
        *pMsgBuff->pNext++ = ' ';
    }
}

static void dbpr_msg_flush(TAB_BUFFER *pMsgBuff,int tab_size)
{
    /* skip print if buffer empty */
    if (pMsgBuff->pNext != pMsgBuff->out_buff)
        printf("%s\n", pMsgBuff->out_buff);

    memset(pMsgBuff->out_buff,'\0', (int) MAXLINE + 1);
    pMsgBuff->pNext = pMsgBuff->out_buff;
    pMsgBuff->pNexTab = pMsgBuff->out_buff + tab_size;
}
