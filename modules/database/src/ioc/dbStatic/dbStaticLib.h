/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
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

#include "dbCoreAPI.h"
#include "dbFldTypes.h"
#include "dbBase.h"
#include "link.h"
#include "errMdef.h"
#include "cantProceed.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef dbBase DBBASE;

typedef struct dbEntry {
    DBBASE       *pdbbase;
    dbRecordType *precordType;
    dbFldDes     *pflddes;
    dbRecordNode *precnode;
    dbInfoNode   *pinfonode;
    void         *pfield;
    char         *message;
    short        indfield;
} DBENTRY;

/* Static database access routines*/
DBCORE_API DBBASE * dbAllocBase(void);
DBCORE_API void dbFreeBase(DBBASE *pdbbase);
DBCORE_API DBENTRY * dbAllocEntry(DBBASE *pdbbase);
DBCORE_API void dbFreeEntry(DBENTRY *pdbentry);
DBCORE_API void dbInitEntry(DBBASE *pdbbase,
    DBENTRY *pdbentry);

DBCORE_API void dbFinishEntry(DBENTRY *pdbentry);
DBCORE_API DBENTRY * dbCopyEntry(DBENTRY *pdbentry);
DBCORE_API void dbCopyEntryContents(DBENTRY *pfrom,
    DBENTRY *pto);

DBCORE_API extern int dbBptNotMonotonic;

/** \brief Open .dbd or .db file and read definitions.
 *  \param ppdbbase The database.  Typically the "pdbbase" global
 *  \param filename Filename to read/search.  May be absolute, or relative.
 *  \param path If !NULL, search path when filename is relative, of for 'include' statements.
 *         Split by ':' or ';' (cf. OSI_PATH_LIST_SEPARATOR)
 *  \param substitutions If !NULL, macro definitions like "NAME=VAL,OTHER=SOME"
 *  \return 0 on success
 */
DBCORE_API long dbReadDatabase(DBBASE **ppdbbase,
    const char *filename, const char *path, const char *substitutions);
/** \brief Read definitions from already opened .dbd or .db file.
 *  \param ppdbbase The database.  Typically the "&pdbbase" global
 *  \param fp FILE* from which to read definitions.  Will always be fclose()'d
 *  \param path If !NULL, search path when filename is relative, of for 'include' statements.
 *         Split by ':' or ';' (cf. OSI_PATH_LIST_SEPARATOR)
 *  \param substitutions If !NULL, macro definitions like "NAME=VAL,OTHER=SOME"
 *  \return 0 on success
 *
 *  \note This function will always close the provided 'fp'.
 */
DBCORE_API long dbReadDatabaseFP(DBBASE **ppdbbase,
    FILE *fp, const char *path, const char *substitutions);
DBCORE_API long dbPath(DBBASE *pdbbase, const char *path);
DBCORE_API long dbAddPath(DBBASE *pdbbase, const char *path);
DBCORE_API char * dbGetPromptGroupNameFromKey(DBBASE *pdbbase,
    const short key);
DBCORE_API short dbGetPromptGroupKeyFromName(DBBASE *pdbbase,
    const char *name);
DBCORE_API long dbWriteRecord(DBBASE *ppdbbase,
    const char *filename, const char *precordTypename, int level);
DBCORE_API long dbWriteRecordFP(DBBASE *ppdbbase,
    FILE *fp, const char *precordTypename, int level);
DBCORE_API long dbWriteMenu(DBBASE *pdbbase,
    const char *filename, const char *menuName);
DBCORE_API long dbWriteMenuFP(DBBASE *pdbbase,
    FILE *fp, const char *menuName);
DBCORE_API long dbWriteRecordType(DBBASE *pdbbase,
    const char *filename, const char *recordTypeName);
DBCORE_API long dbWriteRecordTypeFP(DBBASE *pdbbase,
    FILE *fp, const char *recordTypeName);
DBCORE_API long dbWriteDevice(DBBASE *pdbbase,
    const char *filename);
DBCORE_API long dbWriteDeviceFP(DBBASE *pdbbase, FILE *fp);
DBCORE_API long dbWriteDriver(DBBASE *pdbbase,
    const char *filename);
DBCORE_API long dbWriteDriverFP(DBBASE *pdbbase, FILE *fp);
DBCORE_API long dbWriteLinkFP(DBBASE *pdbbase, FILE *fp);
DBCORE_API long dbWriteRegistrarFP(DBBASE *pdbbase, FILE *fp);
DBCORE_API long dbWriteFunctionFP(DBBASE *pdbbase, FILE *fp);
DBCORE_API long dbWriteVariableFP(DBBASE *pdbbase, FILE *fp);
DBCORE_API long dbWriteBreaktable(DBBASE *pdbbase,
    const char *filename);
DBCORE_API long dbWriteBreaktableFP(DBBASE *pdbbase,
    FILE *fp);

DBCORE_API long dbFindRecordType(DBENTRY *pdbentry,
    const char *recordTypename);
DBCORE_API long dbFirstRecordType(DBENTRY *pdbentry);
DBCORE_API long dbNextRecordType(DBENTRY *pdbentry);
DBCORE_API char * dbGetRecordTypeName(DBENTRY *pdbentry);
DBCORE_API int  dbGetNRecordTypes(DBENTRY *pdbentry);
DBCORE_API long dbPutRecordAttribute(DBENTRY *pdbentry,
    const char *name, const char*value);
DBCORE_API long dbGetRecordAttribute(DBENTRY *pdbentry,
    const char *name);
DBCORE_API long dbGetAttributePart(DBENTRY *pdbentry,
    const char **ppname);

DBCORE_API long dbFirstField(DBENTRY *pdbentry, int dctonly);
DBCORE_API long dbNextField(DBENTRY *pdbentry, int dctonly);
DBCORE_API int  dbGetNFields(DBENTRY *pdbentry, int dctonly);
DBCORE_API char * dbGetFieldName(DBENTRY *pdbentry);
DBCORE_API int dbGetFieldDbfType(DBENTRY *pdbentry);
DBCORE_API char * dbGetDefault(DBENTRY *pdbentry);
DBCORE_API char * dbGetPrompt(DBENTRY *pdbentry);
DBCORE_API int dbGetPromptGroup(DBENTRY *pdbentry);

DBCORE_API long dbCreateRecord(DBENTRY *pdbentry,
    const char *pname);
DBCORE_API long dbDeleteRecord(DBENTRY *pdbentry);
DBCORE_API long dbFreeRecords(DBBASE *pdbbase);
DBCORE_API long dbFindRecordPart(DBENTRY *pdbentry,
    const char **ppname);
DBCORE_API long dbFindRecord(DBENTRY *pdbentry,
    const char *pname);

DBCORE_API long dbFirstRecord(DBENTRY *pdbentry);
DBCORE_API long dbNextRecord(DBENTRY *pdbentry);
DBCORE_API int  dbGetNRecords(DBENTRY *pdbentry);
DBCORE_API int  dbGetNAliases(DBENTRY *pdbentry);
DBCORE_API char * dbGetRecordName(DBENTRY *pdbentry);
DBCORE_API long dbCopyRecord(DBENTRY *pdbentry,
    const char *newRecordName, int overWriteOK);

DBCORE_API long dbVisibleRecord(DBENTRY *pdbentry);
DBCORE_API long dbInvisibleRecord(DBENTRY *pdbentry);
DBCORE_API int dbIsVisibleRecord(DBENTRY *pdbentry);

DBCORE_API long dbCreateAlias(DBENTRY *pdbentry,
    const char *paliasName);
DBCORE_API int dbIsAlias(DBENTRY *pdbentry);
/* Follow alias to actual record */
DBCORE_API int dbFollowAlias(DBENTRY *pdbentry);
DBCORE_API long dbDeleteAliases(DBENTRY *pdbentry);

DBCORE_API long dbFindFieldPart(DBENTRY *pdbentry,
    const char **ppname);
DBCORE_API long dbFindField(DBENTRY *pdbentry,
    const char *pfieldName);
DBCORE_API int dbFoundField(DBENTRY *pdbentry);
DBCORE_API char * dbGetString(DBENTRY *pdbentry);
DBCORE_API long dbPutString(DBENTRY *pdbentry,
    const char *pstring);
DBCORE_API char * dbVerify(DBENTRY *pdbentry,
    const char *pstring);
DBCORE_API int  dbIsDefaultValue(DBENTRY *pdbentry);

DBCORE_API long dbFirstInfo(DBENTRY *pdbentry);
DBCORE_API long dbNextInfo(DBENTRY *pdbentry);
DBCORE_API long dbFindInfo(DBENTRY *pdbentry,
    const char *name);
DBCORE_API long dbNextMatchingInfo(DBENTRY *pdbentry,
    const char *pattern);
DBCORE_API long dbDeleteInfo(DBENTRY *pdbentry);
DBCORE_API const char * dbGetInfoName(DBENTRY *pdbentry);
DBCORE_API const char * dbGetInfoString(DBENTRY *pdbentry);
DBCORE_API long dbPutInfoString(DBENTRY *pdbentry,
    const char *string);
DBCORE_API long dbPutInfoPointer(DBENTRY *pdbentry,
    void *pointer);
DBCORE_API void * dbGetInfoPointer(DBENTRY *pdbentry);
DBCORE_API const char * dbGetInfo(DBENTRY *pdbentry,
    const char *name);
DBCORE_API long dbPutInfo(DBENTRY *pdbentry,
    const char *name, const char *string);

DBCORE_API brkTable * dbFindBrkTable(DBBASE *pdbbase,
    const char *name);

DBCORE_API const char * dbGetFieldTypeString(int dbfType);
DBCORE_API int dbFindFieldType(const char *type);

DBCORE_API dbMenu * dbFindMenu(DBBASE *pdbbase,
    const char *name);
DBCORE_API char ** dbGetMenuChoices(DBENTRY *pdbentry);
DBCORE_API int  dbGetMenuIndex(DBENTRY *pdbentry);
DBCORE_API long dbPutMenuIndex(DBENTRY *pdbentry, int index);
DBCORE_API int  dbGetNMenuChoices(DBENTRY *pdbentry);
DBCORE_API char * dbGetMenuStringFromIndex(DBENTRY *pdbentry,
    int index);
DBCORE_API int dbGetMenuIndexFromString(DBENTRY *pdbentry,
    const char *choice);

DBCORE_API drvSup * dbFindDriver(dbBase *pdbbase,
    const char *name);
DBCORE_API char * dbGetRelatedField(DBENTRY *pdbentry);

DBCORE_API linkSup * dbFindLinkSup(dbBase *pdbbase,
    const char *name);

DBCORE_API int  dbGetNLinks(DBENTRY *pdbentry);
DBCORE_API long dbGetLinkField(DBENTRY *pdbentry, int index);

/* Dump routines */
DBCORE_API void dbDumpPath(DBBASE *pdbbase);
DBCORE_API void dbDumpRecord(DBBASE *pdbbase,
    const char *precordTypename, int level);
DBCORE_API void dbDumpMenu(DBBASE *pdbbase,
    const char *menuName);
DBCORE_API void dbDumpRecordType(DBBASE *pdbbase,
    const char *recordTypeName);
DBCORE_API void dbDumpField(DBBASE *pdbbase,
    const char *recordTypeName, const char *fname);
DBCORE_API void dbDumpDevice(DBBASE *pdbbase,
    const char *recordTypeName);
DBCORE_API void dbDumpDriver(DBBASE *pdbbase);
DBCORE_API void dbDumpLink(DBBASE *pdbbase);
DBCORE_API void dbDumpRegistrar(DBBASE *pdbbase);
DBCORE_API void dbDumpFunction(DBBASE *pdbbase);
DBCORE_API void dbDumpVariable(DBBASE *pdbbase);
DBCORE_API void dbDumpBreaktable(DBBASE *pdbbase,
    const char *name);
DBCORE_API void dbPvdDump(DBBASE *pdbbase, int verbose);
DBCORE_API void dbReportDeviceConfig(DBBASE *pdbbase,
    FILE *report);

/* Misc useful routines*/
#define dbCalloc(nobj,size) callocMustSucceed(nobj,size,"dbCalloc")
#define dbMalloc(size) mallocMustSucceed(size,"dbMalloc")
DBCORE_API void dbCatString(char **string, int *stringLength,
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
