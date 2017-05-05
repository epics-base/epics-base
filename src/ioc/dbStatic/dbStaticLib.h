/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author: Marty Kraimer
 *      Date:   06-08-93
 */

#ifndef INCdbStaticLibh
#define INCdbStaticLibh 1

#include <stddef.h>
#include <stdio.h>

#include "shareLib.h"
#include "dbFldTypes.h"
#include "dbBase.h"
#include "link.h"
#include "errMdef.h"
#include "cantProceed.h"

#ifdef __cplusplus
extern "C" {
#endif

/*Field types as seen by static database access clients*/
#define DCT_STRING      0
#define DCT_INTEGER     1
#define DCT_REAL        2
#define DCT_MENU        3
#define DCT_MENUFORM    4
#define DCT_INLINK      5
#define DCT_OUTLINK     6
#define DCT_FWDLINK     7
#define DCT_NOACCESS    8

/*Link types as seen by static database access clients*/
#define DCT_LINK_CONSTANT       0
#define DCT_LINK_FORM           1
#define DCT_LINK_PV             2

typedef dbBase DBBASE;

typedef struct{
    DBBASE       *pdbbase;
    dbRecordType *precordType;
    dbFldDes     *pflddes;
    dbRecordNode *precnode;
    dbInfoNode   *pinfonode;
    void         *pfield;
    char         *message;
    short        indfield;
} DBENTRY;

struct dbAddr;
struct dbCommon;

/*dbDumpFldDes is obsolete. It is only provided for compatibility*/
#define dbDumpFldDes dbDumpField

/* Static database access routines*/
epicsShareFunc DBBASE * dbAllocBase(void);
epicsShareFunc void dbFreeBase(DBBASE *pdbbase);
epicsShareFunc DBENTRY * dbAllocEntry(DBBASE *pdbbase);
epicsShareFunc void dbFreeEntry(DBENTRY *pdbentry);
epicsShareFunc void dbInitEntry(DBBASE *pdbbase,
    DBENTRY *pdbentry);
/** Initialize DBENTRY from a valid dbAddr*.
 * Constant time equivalent of dbInitEntry() then dbFindRecord(), and finally dbFollowAlias()
 * except that DBENTRY::indfield is not set
 */
epicsShareFunc void dbInitEntryFromAddr(struct dbAddr *paddr, DBENTRY *pdbentry);
/** Initialize DBENTRY from a valid record (dbCommon*).
 * Constant time equivalent of dbInitEntry() then dbFindRecord(), and finally dbFollowAlias()
 * when no field is specified (pflddes and pfield are NULL).
 * except that DBENTRY::indfield is not set.
 */
epicsShareFunc void dbInitEntryFromRecord(struct dbCommon *prec, DBENTRY *pdbentry);
epicsShareFunc void dbFinishEntry(DBENTRY *pdbentry);
epicsShareFunc DBENTRY * dbCopyEntry(DBENTRY *pdbentry);
epicsShareFunc void dbCopyEntryContents(DBENTRY *pfrom,
    DBENTRY *pto);

epicsShareExtern int dbBptNotMonotonic;

epicsShareFunc long dbReadDatabase(DBBASE **ppdbbase,
    const char *filename, const char *path, const char *substitutions);
epicsShareFunc long dbReadDatabaseFP(DBBASE **ppdbbase,
    FILE *fp, const char *path, const char *substitutions);
epicsShareFunc long dbPath(DBBASE *pdbbase, const char *path);
epicsShareFunc long dbAddPath(DBBASE *pdbbase, const char *path);
epicsShareFunc char * dbGetPromptGroupNameFromKey(DBBASE *pdbbase,
    const short key);
epicsShareFunc short dbGetPromptGroupKeyFromName(DBBASE *pdbbase,
    const char *name);
epicsShareFunc long dbWriteRecord(DBBASE *ppdbbase,
    const char *filename, const char *precordTypename, int level);
epicsShareFunc long dbWriteRecordFP(DBBASE *ppdbbase,
    FILE *fp, const char *precordTypename, int level);
epicsShareFunc long dbWriteMenu(DBBASE *pdbbase,
    const char *filename, const char *menuName);
epicsShareFunc long dbWriteMenuFP(DBBASE *pdbbase,
    FILE *fp, const char *menuName);
epicsShareFunc long dbWriteRecordType(DBBASE *pdbbase,
    const char *filename, const char *recordTypeName);
epicsShareFunc long dbWriteRecordTypeFP(DBBASE *pdbbase,
    FILE *fp, const char *recordTypeName);
epicsShareFunc long dbWriteDevice(DBBASE *pdbbase,
    const char *filename);
epicsShareFunc long dbWriteDeviceFP(DBBASE *pdbbase, FILE *fp);
epicsShareFunc long dbWriteDriver(DBBASE *pdbbase,
    const char *filename);
epicsShareFunc long dbWriteDriverFP(DBBASE *pdbbase, FILE *fp);
epicsShareFunc long dbWriteLinkFP(DBBASE *pdbbase, FILE *fp);
epicsShareFunc long dbWriteRegistrarFP(DBBASE *pdbbase, FILE *fp);
epicsShareFunc long dbWriteFunctionFP(DBBASE *pdbbase, FILE *fp);
epicsShareFunc long dbWriteVariableFP(DBBASE *pdbbase, FILE *fp);
epicsShareFunc long dbWriteBreaktable(DBBASE *pdbbase,
    const char *filename);
epicsShareFunc long dbWriteBreaktableFP(DBBASE *pdbbase,
    FILE *fp);

epicsShareFunc long dbFindRecordType(DBENTRY *pdbentry,
    const char *recordTypename);
epicsShareFunc long dbFirstRecordType(DBENTRY *pdbentry);
epicsShareFunc long dbNextRecordType(DBENTRY *pdbentry);
epicsShareFunc char * dbGetRecordTypeName(DBENTRY *pdbentry);
epicsShareFunc int  dbGetNRecordTypes(DBENTRY *pdbentry);
epicsShareFunc long dbPutRecordAttribute(DBENTRY *pdbentry,
    const char *name, const char*value);
epicsShareFunc long dbGetRecordAttribute(DBENTRY *pdbentry,
    const char *name);
epicsShareFunc long dbGetAttributePart(DBENTRY *pdbentry,
    const char **ppname);

epicsShareFunc long dbFirstField(DBENTRY *pdbentry, int dctonly);
epicsShareFunc long dbNextField(DBENTRY *pdbentry, int dctonly);
epicsShareFunc int  dbGetFieldType(DBENTRY *pdbentry);
epicsShareFunc int  dbGetNFields(DBENTRY *pdbentry, int dctonly);
epicsShareFunc char * dbGetFieldName(DBENTRY *pdbentry);
epicsShareFunc char * dbGetDefault(DBENTRY *pdbentry);
epicsShareFunc char * dbGetPrompt(DBENTRY *pdbentry);
epicsShareFunc int dbGetPromptGroup(DBENTRY *pdbentry);

epicsShareFunc long dbCreateRecord(DBENTRY *pdbentry,
    const char *pname);
epicsShareFunc long dbDeleteRecord(DBENTRY *pdbentry);
epicsShareFunc long dbFreeRecords(DBBASE *pdbbase);
epicsShareFunc long dbFindRecordPart(DBENTRY *pdbentry,
    const char **ppname);
epicsShareFunc long dbFindRecord(DBENTRY *pdbentry,
    const char *pname);

epicsShareFunc long dbFirstRecord(DBENTRY *pdbentry);
epicsShareFunc long dbNextRecord(DBENTRY *pdbentry);
epicsShareFunc int  dbGetNRecords(DBENTRY *pdbentry);
epicsShareFunc int  dbGetNAliases(DBENTRY *pdbentry);
epicsShareFunc char * dbGetRecordName(DBENTRY *pdbentry);
epicsShareFunc long dbCopyRecord(DBENTRY *pdbentry,
    const char *newRecordName, int overWriteOK);

epicsShareFunc long dbVisibleRecord(DBENTRY *pdbentry);
epicsShareFunc long dbInvisibleRecord(DBENTRY *pdbentry);
epicsShareFunc int dbIsVisibleRecord(DBENTRY *pdbentry);

epicsShareFunc long dbCreateAlias(DBENTRY *pdbentry,
    const char *paliasName);
epicsShareFunc int dbIsAlias(DBENTRY *pdbentry);
/* Follow alias to actual record */
epicsShareFunc int dbFollowAlias(DBENTRY *pdbentry);
epicsShareFunc long dbDeleteAliases(DBENTRY *pdbentry);

epicsShareFunc long dbFindFieldPart(DBENTRY *pdbentry,
    const char **ppname);
epicsShareFunc long dbFindField(DBENTRY *pdbentry,
    const char *pfieldName);
epicsShareFunc int dbFoundField(DBENTRY *pdbentry);
epicsShareFunc char * dbGetString(DBENTRY *pdbentry);
epicsShareFunc long dbPutString(DBENTRY *pdbentry,
    const char *pstring);
epicsShareFunc char * dbVerify(DBENTRY *pdbentry,
    const char *pstring);
epicsShareFunc char * dbGetRange(DBENTRY *pdbentry);
epicsShareFunc int  dbIsDefaultValue(DBENTRY *pdbentry);

epicsShareFunc long dbFirstInfo(DBENTRY *pdbentry);
epicsShareFunc long dbNextInfo(DBENTRY *pdbentry);
epicsShareFunc long dbFindInfo(DBENTRY *pdbentry,
    const char *name);
epicsShareFunc long dbDeleteInfo(DBENTRY *pdbentry);
epicsShareFunc const char * dbGetInfoName(DBENTRY *pdbentry);
epicsShareFunc const char * dbGetInfoString(DBENTRY *pdbentry);
epicsShareFunc long dbPutInfoString(DBENTRY *pdbentry,
    const char *string);
epicsShareFunc long dbPutInfoPointer(DBENTRY *pdbentry,
    void *pointer);
epicsShareFunc void * dbGetInfoPointer(DBENTRY *pdbentry);
epicsShareFunc const char * dbGetInfo(DBENTRY *pdbentry,
    const char *name);
epicsShareFunc long dbPutInfo(DBENTRY *pdbentry,
    const char *name, const char *string);

epicsShareFunc brkTable * dbFindBrkTable(DBBASE *pdbbase,
    const char *name);

epicsShareFunc const char * dbGetFieldTypeString(int dbfType);
epicsShareFunc int dbFindFieldType(const char *type);

epicsShareFunc dbMenu * dbFindMenu(DBBASE *pdbbase,
    const char *name);
epicsShareFunc char ** dbGetMenuChoices(DBENTRY *pdbentry);
epicsShareFunc int  dbGetMenuIndex(DBENTRY *pdbentry);
epicsShareFunc long dbPutMenuIndex(DBENTRY *pdbentry, int index);
epicsShareFunc int  dbGetNMenuChoices(DBENTRY *pdbentry);
epicsShareFunc char * dbGetMenuStringFromIndex(DBENTRY *pdbentry,
    int index);
epicsShareFunc int dbGetMenuIndexFromString(DBENTRY *pdbentry,
    const char *choice);

epicsShareFunc drvSup * dbFindDriver(dbBase *pdbbase,
    const char *name);
epicsShareFunc char * dbGetRelatedField(DBENTRY *pdbentry);

epicsShareFunc linkSup * dbFindLinkSup(dbBase *pdbbase,
    const char *name);

epicsShareFunc int  dbGetNLinks(DBENTRY *pdbentry);
epicsShareFunc long dbGetLinkField(DBENTRY *pdbentry, int index);
epicsShareFunc int  dbGetLinkType(DBENTRY *pdbentry);

/* Dump routines */
epicsShareFunc void dbDumpPath(DBBASE *pdbbase);
epicsShareFunc void dbDumpRecord(DBBASE *pdbbase,
    const char *precordTypename, int level);
epicsShareFunc void dbDumpMenu(DBBASE *pdbbase,
    const char *menuName);
epicsShareFunc void dbDumpRecordType(DBBASE *pdbbase,
    const char *recordTypeName);
epicsShareFunc void dbDumpField(DBBASE *pdbbase,
    const char *recordTypeName, const char *fname);
epicsShareFunc void dbDumpDevice(DBBASE *pdbbase,
    const char *recordTypeName);
epicsShareFunc void dbDumpDriver(DBBASE *pdbbase);
epicsShareFunc void dbDumpLink(DBBASE *pdbbase);
epicsShareFunc void dbDumpRegistrar(DBBASE *pdbbase);
epicsShareFunc void dbDumpFunction(DBBASE *pdbbase);
epicsShareFunc void dbDumpVariable(DBBASE *pdbbase);
epicsShareFunc void dbDumpBreaktable(DBBASE *pdbbase,
    const char *name);
epicsShareFunc void dbPvdDump(DBBASE *pdbbase, int verbose);
epicsShareFunc void dbReportDeviceConfig(DBBASE *pdbbase,
    FILE *report);

/* Misc useful routines*/
#define dbCalloc(nobj,size) callocMustSucceed(nobj,size,"dbCalloc")
#define dbMalloc(size) mallocMustSucceed(size,"dbMalloc")
epicsShareFunc void dbCatString(char **string, int *stringLength,
    char *pnew, char *separator);

extern int dbStaticDebug;
extern int dbConvertStrict;

#define S_dbLib_recordTypeNotFound (M_dbLib|1) /* Record Type does not exist */
#define S_dbLib_recExists (M_dbLib|3)          /* Record Already exists */
#define S_dbLib_recNotFound (M_dbLib|5)        /* Record Not Found */
#define S_dbLib_flddesNotFound (M_dbLib|7)     /* Field Description Not Found */
#define S_dbLib_fieldNotFound (M_dbLib|9)      /* Field Not Found */
#define S_dbLib_badField (M_dbLib|11)          /* Bad Field value */
#define S_dbLib_menuNotFound (M_dbLib|13)      /* Menu not found */
#define S_dbLib_badLink (M_dbLib|15)           /* Bad Link Field */
#define S_dbLib_nameLength (M_dbLib|17)        /* Record Name is too long */
#define S_dbLib_noRecSup (M_dbLib|19)          /* Record support not found */
#define S_dbLib_strLen (M_dbLib|21)            /* String is too long */
#define S_dbLib_noSizeOffset (M_dbLib|23)      /* Missing SizeOffset Routine - No record support? */
#define S_dbLib_outMem (M_dbLib|27)            /* Out of memory */
#define S_dbLib_infoNotFound (M_dbLib|29)      /* Info item Not Found */

#ifdef __cplusplus
}
#endif

#endif /*INCdbStaticLibh*/
