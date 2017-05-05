/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*dbStaticLibRun.c*/

#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "cvtFast.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "epicsPrint.h"
#include "epicsStdlib.h"
#include "epicsTypes.h"
#include "errMdef.h"

#include "epicsExport.h" /* #define epicsExportSharedSymbols */
#include "dbBase.h"
#include "dbCommonPvt.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "devSup.h"
#include "special.h"

epicsShareDef int dbConvertStrict = 0;
epicsExportAddress(int, dbConvertStrict);

static long do_nothing(struct dbCommon *precord) { return 0; }

/* Dummy DSXT used for soft device supports */
struct dsxt devSoft_DSXT = {
    do_nothing,
    do_nothing
};

static devSup *pthisDevSup = NULL;

void dbInitDevSup(devSup *pdevSup, dset *pdset)
{
    pdevSup->pdset = pdset;
    if (pdevSup->link_type == CONSTANT)
        pdevSup->pdsxt = &devSoft_DSXT;

    if (pdset->init) {
        pthisDevSup = pdevSup;
        pdset->init(0);
        pthisDevSup = NULL;
    }
}

void devExtend(dsxt *pdsxt)
{
    if (!pthisDevSup)
        errlogPrintf("devExtend() called outside of dbInitDevSup()\n");
    else {
        pthisDevSup->pdsxt = pdsxt;
    }
}

long dbAllocRecord(DBENTRY *pdbentry,const char *precordName)
{
    dbRecordType	*pdbRecordType = pdbentry->precordType;
    dbRecordNode	*precnode = pdbentry->precnode;
    dbFldDes		*pflddes;
    int			i;
    dbCommonPvt *ppvt;
    dbCommon		*precord;
    char		*pfield;
    
    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(pdbRecordType->rec_size == 0) {
        printf("\t*** Did you run x_RegisterRecordDeviceDriver(pdbbase) yet? ***\n");
        epicsPrintf("dbAllocRecord(%s) with %s rec_size = 0\n",
                    precordName, pdbRecordType->name);
        return(S_dbLib_noRecSup);
    } else if(pdbRecordType->rec_size<sizeof(*precord)) {
        printf("\t*** Recordtype %s must include \"dbCommon.dbd\"\n", pdbRecordType->name);
        epicsPrintf("dbAllocRecord(%s) with %s rec_size = %d\n",
                    precordName, pdbRecordType->name, pdbRecordType->rec_size);
        return(S_dbLib_noRecSup);
    }
    ppvt = dbCalloc(1, offsetof(dbCommonPvt, common) + pdbRecordType->rec_size);
    precord = &ppvt->common;
    ppvt->recnode = precnode;
    precord->rdes = pdbRecordType;
    precnode->precord = precord;
    pflddes = pdbRecordType->papFldDes[0];
    if(!pflddes) {
        epicsPrintf("dbAllocRecord pflddes for NAME not found\n");
        return(S_dbLib_flddesNotFound);
    }
    assert(pflddes->offset == 0);
    assert(pflddes->size == sizeof(precord->name));
    if(strlen(precordName) >= sizeof(precord->name)) {
        epicsPrintf("dbAllocRecord: NAME(%s) too long\n",precordName);
        return(S_dbLib_nameLength);
    }
    strcpy(precord->name, precordName);
    for(i=1; i<pdbRecordType->no_fields; i++) {

        pflddes = pdbRecordType->papFldDes[i];
        if(!pflddes) continue;
        pfield = (char*)precord + pflddes->offset;
        pdbentry->pfield = (void *)pfield;
        pdbentry->pflddes = pflddes;
        pdbentry->indfield = i;
        switch(pflddes->field_type) {
        case DBF_STRING:
            if(pflddes->initial)  {
                if(strlen(pflddes->initial) >= pflddes->size) {
                    epicsPrintf("initial size > size for %s.%s\n",
                                pdbRecordType->name,pflddes->name);
                } else {
                    strcpy(pfield,pflddes->initial);
                }
            }
            break;
        case DBF_CHAR:
        case DBF_UCHAR:
        case DBF_SHORT:
        case DBF_USHORT:
        case DBF_LONG:
        case DBF_ULONG:
	case DBF_INT64:
	case DBF_UINT64:
        case DBF_FLOAT:
        case DBF_DOUBLE:
        case DBF_ENUM:
        case DBF_MENU:
            if(pflddes->initial) {
                long status;

                status = dbPutStringNum(pdbentry,pflddes->initial);
                if(status)
                    epicsPrintf("Error initializing %s.%s initial %s\n",
                                pdbRecordType->name,pflddes->name,pflddes->initial);
            }
            break;
        case DBF_DEVICE:
            if(!pflddes->ftPvt) dbGetDeviceMenu(pdbentry);
            break;
        case DBF_INLINK:
        case DBF_OUTLINK:
        case DBF_FWDLINK: {
            DBLINK *plink = (DBLINK *)pfield;

            plink->type = CONSTANT;
            if(pflddes->initial) {
                plink->text =
                        dbCalloc(strlen(pflddes->initial)+1,sizeof(char));
                strcpy(plink->text,pflddes->initial);
            }
        }
            break;
        case DBF_NOACCESS:
            break;
        default:
            epicsPrintf("dbAllocRecord: Illegal field type\n");
        }
    }
    return(0);
}

long dbFreeRecord(DBENTRY *pdbentry)
{
    dbRecordType *pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;

    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!precnode->precord) return(S_dbLib_recNotFound);
    free(CONTAINER(precnode->precord, dbCommonPvt, common));
    precnode->precord = NULL;
    return(0);
}

long dbGetFieldAddress(DBENTRY *pdbentry)
{
    dbRecordType *pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes = pdbentry->pflddes;

    if(!pdbRecordType) return(S_dbLib_recordTypeNotFound);
    if(!precnode) return(S_dbLib_recNotFound);
    if(!pflddes) return(S_dbLib_flddesNotFound);
    if(!precnode->precord) return(0);
    pdbentry->pfield = ((char *)precnode->precord) + pflddes->offset;
    return(0);
}

char *dbRecordName(DBENTRY *pdbentry)
{
    dbRecordType *pdbRecordType = pdbentry->precordType;
    dbRecordNode *precnode = pdbentry->precnode;
    dbFldDes	*pflddes;
    char	*precord;

    if(!pdbRecordType) return(0);
    if(!precnode) return(0);
    if(!precnode->precord) return(0);
    precord = (char *)precnode->precord;
    pflddes = pdbRecordType->papFldDes[0];
    if(!pflddes) return(NULL);
    return(precord + pflddes->offset);
}

int dbIsMacroOk(DBENTRY *pdbentry) { return(FALSE); }

epicsShareFunc int dbIsDefaultValue(DBENTRY *pdbentry)
{
    dbFldDes *pflddes = pdbentry->pflddes;
    void *pfield = pdbentry->pfield;

    if (!pflddes || !pfield)
        return FALSE;

    switch (pflddes->field_type) {
        case DBF_STRING: {
            char *p = (char *)pfield;

            return pflddes->initial ? ! strcmp(pflddes->initial, p)
                : ! strlen(p);
        }
        case DBF_CHAR: {
            epicsInt8 field = *(epicsInt8 *)pfield;
            epicsInt8 def;

            if (!pflddes->initial)
                return field == 0;

            return ! epicsParseInt8(pflddes->initial, &def, 0, NULL)
                && field == def;
        }
        case DBF_UCHAR: {
            epicsUInt8 field = *(epicsUInt8 *)pfield;
            epicsUInt8 def;

            if (!pflddes->initial)
                return field == 0;

            return ! epicsParseUInt8(pflddes->initial, &def, 0, NULL)
                && field == def;
        }
        case DBF_SHORT: {
            epicsInt16 field = *(epicsInt16 *)pfield;
            epicsInt16 def;

            if (!pflddes->initial)
                return field == 0;

            return ! epicsParseInt16(pflddes->initial, &def, 0, NULL)
                && field == def;
        }
        case DBF_ENUM:
        case DBF_USHORT: {
            epicsUInt16 field = *(epicsUInt16 *)pfield;
            epicsUInt16 def;

            if (!pflddes->initial)
                return field == 0;

            return ! epicsParseUInt16(pflddes->initial, &def, 0, NULL)
                && field == def;
        }
        case DBF_LONG: {
            epicsInt32 field = *(epicsInt32 *)pfield;
            epicsInt32 def;

            if (!pflddes->initial)
                return field == 0;

            return ! epicsParseInt32(pflddes->initial, &def, 0, NULL)
                && field == def;
        }
        case DBF_ULONG: {
            epicsUInt32 field = *(epicsUInt32 *)pfield;
            epicsUInt32 def;

            if (!pflddes->initial)
                return field == 0;

            return ! epicsParseUInt32(pflddes->initial, &def, 0, NULL)
                && field == def;
        }
        case DBF_INT64: {
            epicsInt64 field = *(epicsInt64 *)pfield;
            epicsInt64 def;

            if (!pflddes->initial)
                return field == 0;

            return ! epicsParseInt64(pflddes->initial, &def, 0, NULL)
                && field == def;
        }
        case DBF_UINT64: {
            epicsUInt64 field = *(epicsUInt64 *)pfield;
            epicsUInt64 def;

            if (!pflddes->initial)
                return field == 0;

            return ! epicsParseUInt64(pflddes->initial, &def, 0, NULL)
                && field == def;
        }
        case DBF_FLOAT: {
            epicsFloat32 field = *(epicsFloat32 *)pfield;
            epicsFloat32 def;

            if (!pflddes->initial)
                return field == 0;

            return ! epicsParseFloat32(pflddes->initial, &def, NULL)
                && field == def;
        }
        case DBF_DOUBLE: {
            epicsFloat64 field = *(epicsFloat64 *)pfield;
            epicsFloat64 def;

            if (!pflddes->initial)
                return field == 0;

            return ! epicsParseFloat64(pflddes->initial, &def, NULL)
                && field == def;
        }
        case DBF_MENU: {
            epicsEnum16 field = *(epicsEnum16 *)pfield;
            epicsEnum16 def;
            int index;

            if (!pflddes->initial)
                return field == 0;

            index = dbGetMenuIndexFromString(pdbentry, pflddes->initial);
            if (index < 0) {
                if (epicsParseUInt16(pflddes->initial, &def, 0, NULL))
                    return FALSE;
            }
            else
                def = index;
            return field == def;
        }
        case DBF_DEVICE: {
            dbRecordType *precordType = pdbentry->precordType;

            if (!precordType) {
                epicsPrintf("dbIsDefaultValue: pdbRecordType is NULL??\n");
                return FALSE;
            }
            return ellCount(&precordType->devList) == 0;
        }
        case DBF_INLINK:
        case DBF_OUTLINK:
        case DBF_FWDLINK: {
            struct link *plink = (struct link *)pfield;

            if (!plink || plink->type != CONSTANT)
                return FALSE;

            /* These conditions don't make a lot of sense... */
            if (!plink->value.constantStr)
                return TRUE;

            if (!pflddes->initial)  /* Default value for a link field? */
                return FALSE;

            return !strcmp(plink->value.constantStr, pflddes->initial);
        }
        default:
            return TRUE;
    }
}

long dbPutStringNum(DBENTRY *pdbentry, const char *pstring)
{
    dbFldDes *pflddes = pdbentry->pflddes;
    void *pfield = pdbentry->pfield;
    long status;
    epicsUInt64 u64;
    epicsInt64 i64;

    if (!pfield)
        return S_dbLib_fieldNotFound;

    /* empty string is the same as writing numeric zero */
    if (pstring[0] == '\0')
        pstring = "0";

    switch (pflddes->field_type) {
    case DBF_CHAR:
        if (dbConvertStrict)
            return epicsParseInt8(pstring, pfield, 0, NULL);
        goto lax_signed;

    case DBF_SHORT:
        if (dbConvertStrict)
            return epicsParseInt16(pstring, pfield, 0, NULL);
        goto lax_signed;

    case DBF_LONG:
        if (dbConvertStrict)
            return epicsParseInt32(pstring, pfield, 0, NULL);
        goto lax_signed;

    case DBF_INT64:
        if (dbConvertStrict)
            return epicsParseInt64(pstring, pfield, 0, NULL);

    lax_signed:
        status = epicsParseInt64(pstring, &i64, 0, NULL);
        if (status)
            return status;

        switch (pflddes->field_type) {
        case DBF_CHAR:  *(epicsInt8 *)pfield = (epicsInt8) i64; break;
        case DBF_SHORT: *(epicsInt16*)pfield = (epicsInt16)i64; break;
        case DBF_LONG:  *(epicsInt32*)pfield = (epicsInt32)i64; break;
        case DBF_INT64: *(epicsInt64*)pfield = (epicsInt64)i64; break;
        default: break;
        }
        return status;

    case DBF_UCHAR:
        if (dbConvertStrict)
            return epicsParseUInt8(pstring, pfield, 0, NULL);
        goto lax_unsigned;

    case DBF_ENUM:
    case DBF_USHORT:
        if (dbConvertStrict)
            return epicsParseUInt16(pstring, pfield, 0, NULL);
        goto lax_unsigned;

    case DBF_ULONG:
        if (dbConvertStrict)
            return epicsParseUInt32(pstring, pfield, 0, NULL);
        goto lax_unsigned;

    case DBF_UINT64:
        if (dbConvertStrict)
            return epicsParseUInt64(pstring, pfield, 0, NULL);

    lax_unsigned:
        status = epicsParseUInt64(pstring, &u64, 0, NULL);
        if (status)
            return status;

        switch (pflddes->field_type) {
        case DBF_UCHAR:  *(epicsUInt8 *)pfield = (epicsInt8) u64; break;
        case DBF_ENUM:
        case DBF_USHORT: *(epicsUInt16*)pfield = (epicsInt16)u64; break;
        case DBF_ULONG:  *(epicsUInt32*)pfield = (epicsInt32)u64; break;
        case DBF_UINT64: *(epicsUInt64*)pfield = (epicsInt64)u64; break;
        default: break;
        }
        return status;

    case DBF_FLOAT:
        return epicsParseFloat32(pstring, pfield, NULL);

    case DBF_DOUBLE:
        return epicsParseFloat64(pstring, pfield, NULL);

    case DBF_MENU:
    case DBF_DEVICE: {
            epicsEnum16 *field = (epicsEnum16 *) pfield;
            int index = dbGetMenuIndexFromString(pdbentry, pstring);

            if (index < 0) {
                epicsEnum16 value;
                long status = epicsParseUInt16(pstring, &value, 0, NULL);

                if (status)
                    return status;

                index = dbGetNMenuChoices(pdbentry);
                if (value > index && index > 0)
                    return S_dbLib_badField;

                *field = value;
            }
            else
                *field = index;
            return 0;
        }

    default:
        return S_dbLib_badField;
    }
}

epicsShareFunc int dbGetMenuIndex(DBENTRY *pdbentry)
{
    dbFldDes *pflddes = pdbentry->pflddes;
    void *pfield = pdbentry->pfield;

    if (!pflddes || !pfield)
        return -1;

    switch (pflddes->field_type) {
    case DBF_MENU:
    case DBF_DEVICE:
        return * (epicsEnum16 *) pfield;
    default:
        epicsPrintf("dbGetMenuIndex: Called for field type %d\n",
            pflddes->field_type);
    }
    return -1;
}

epicsShareFunc long dbPutMenuIndex(DBENTRY *pdbentry, int index)
{
    dbFldDes *pflddes = pdbentry->pflddes;
    epicsEnum16 *pfield = pdbentry->pfield;

    if (!pflddes)
        return S_dbLib_flddesNotFound;
    if (!pfield)
        return S_dbLib_fieldNotFound;

    switch (pflddes->field_type) {
    case DBF_MENU: {
        dbMenu *pdbMenu = (dbMenu *) pflddes->ftPvt;

        if (!pdbMenu)
            return S_dbLib_menuNotFound;
        if (index < 0 || index >= pdbMenu->nChoice)
            return S_dbLib_badField;

        *pfield = index;
        return 0;
    }

    case DBF_DEVICE: {
        dbDeviceMenu *pdbDeviceMenu = dbGetDeviceMenu(pdbentry);

        if (!pdbDeviceMenu)
            return S_dbLib_menuNotFound;
        if (index < 0 || index >= pdbDeviceMenu->nChoice)
            return S_dbLib_badField;

        return dbPutString(pdbentry, pdbDeviceMenu->papChoice[index]);
    }

    default:
        break;
    }
    return S_dbLib_badField;
}
