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
    void         *formpvt;
} DBENTRY;

/*dbDumpFldDes is obsolete. It is only provided for compatibility*/
#define dbDumpFldDes dbDumpField

/* Static database access routines*/
epicsShareFunc DBBASE * epicsShareAPI dbAllocBase(void);
epicsShareFunc void epicsShareAPI dbFreeBase(DBBASE *pdbbase);
epicsShareFunc DBENTRY * epicsShareAPI dbAllocEntry(DBBASE *pdbbase);
epicsShareFunc void epicsShareAPI dbFreeEntry(DBENTRY *pdbentry);
epicsShareFunc void epicsShareAPI dbInitEntry(DBBASE *pdbbase,
    DBENTRY *pdbentry);
epicsShareFunc void epicsShareAPI dbFinishEntry(DBENTRY *pdbentry);
epicsShareFunc DBENTRY * epicsShareAPI dbCopyEntry(DBENTRY *pdbentry);
epicsShareFunc void epicsShareAPI dbCopyEntryContents(DBENTRY *pfrom,
    DBENTRY *pto);

epicsShareExtern int dbBptNotMonotonic;

epicsShareFunc long epicsShareAPI dbReadDatabase(DBBASE **ppdbbase,
    const char *filename, const char *path, const char *substitutions);
epicsShareFunc long epicsShareAPI dbReadDatabaseFP(DBBASE **ppdbbase,
    FILE *fp, const char *path, const char *substitutions);
epicsShareFunc long epicsShareAPI dbPath(DBBASE *pdbbase, const char *path);
epicsShareFunc long epicsShareAPI dbAddPath(DBBASE *pdbbase, const char *path);
epicsShareFunc long epicsShareAPI dbWriteRecord(DBBASE *ppdbbase,
    const char *filename, const char *precordTypename, int level);
epicsShareFunc long epicsShareAPI dbWriteRecordFP(DBBASE *ppdbbase,
    FILE *fp, const char *precordTypename, int level);
epicsShareFunc long epicsShareAPI dbWriteMenu(DBBASE *pdbbase,
    const char *filename, const char *menuName);
epicsShareFunc long epicsShareAPI dbWriteMenuFP(DBBASE *pdbbase,
    FILE *fp, const char *menuName);
epicsShareFunc long epicsShareAPI dbWriteRecordType(DBBASE *pdbbase,
    const char *filename, const char *recordTypeName);
epicsShareFunc long epicsShareAPI dbWriteRecordTypeFP(DBBASE *pdbbase,
    FILE *fp, const char *recordTypeName);
epicsShareFunc long epicsShareAPI dbWriteDevice(DBBASE *pdbbase,
    const char *filename);
epicsShareFunc long epicsShareAPI dbWriteDeviceFP(DBBASE *pdbbase, FILE *fp);
epicsShareFunc long epicsShareAPI dbWriteDriver(DBBASE *pdbbase,
    const char *filename);
epicsShareFunc long epicsShareAPI dbWriteDriverFP(DBBASE *pdbbase, FILE *fp);
epicsShareFunc long epicsShareAPI dbWriteRegistrarFP(DBBASE *pdbbase, FILE *fp);
epicsShareFunc long epicsShareAPI dbWriteFunctionFP(DBBASE *pdbbase, FILE *fp);
epicsShareFunc long epicsShareAPI dbWriteVariableFP(DBBASE *pdbbase, FILE *fp);
epicsShareFunc long epicsShareAPI dbWriteBreaktable(DBBASE *pdbbase,
    const char *filename);
epicsShareFunc long epicsShareAPI dbWriteBreaktableFP(DBBASE *pdbbase,
    FILE *fp);

epicsShareFunc long epicsShareAPI dbFindRecordType(DBENTRY *pdbentry,
    const char *recordTypename);
epicsShareFunc long epicsShareAPI dbFirstRecordType(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbNextRecordType(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetRecordTypeName(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbGetNRecordTypes(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbPutRecordAttribute(DBENTRY *pdbentry,
    const char *name, const char*value);
epicsShareFunc long epicsShareAPI dbGetRecordAttribute(DBENTRY *pdbentry,
    const char *name);
epicsShareFunc long epicsShareAPI dbGetAttributePart(DBENTRY *pdbentry,
    const char **ppname);

epicsShareFunc long epicsShareAPI dbFirstField(DBENTRY *pdbentry, int dctonly);
epicsShareFunc long epicsShareAPI dbNextField(DBENTRY *pdbentry, int dctonly);
epicsShareFunc int  epicsShareAPI dbGetFieldType(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbGetNFields(DBENTRY *pdbentry, int dctonly);
epicsShareFunc char * epicsShareAPI dbGetFieldName(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetDefault(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetPrompt(DBENTRY *pdbentry);
epicsShareFunc int epicsShareAPI dbGetPromptGroup(DBENTRY *pdbentry);

epicsShareFunc long epicsShareAPI dbCreateRecord(DBENTRY *pdbentry,
    const char *pname);
epicsShareFunc long epicsShareAPI dbDeleteRecord(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbFreeRecords(DBBASE *pdbbase);
epicsShareFunc long epicsShareAPI dbFindRecordPart(DBENTRY *pdbentry,
    const char **ppname);
epicsShareFunc long epicsShareAPI dbFindRecord(DBENTRY *pdbentry,
    const char *pname);

epicsShareFunc long epicsShareAPI dbFirstRecord(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbNextRecord(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbGetNRecords(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbGetNAliases(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetRecordName(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbRenameRecord(DBENTRY *pdbentry,
    const char *newName);
epicsShareFunc long epicsShareAPI dbCopyRecord(DBENTRY *pdbentry,
    const char *newRecordName, int overWriteOK);

epicsShareFunc long epicsShareAPI dbVisibleRecord(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbInvisibleRecord(DBENTRY *pdbentry);
epicsShareFunc int epicsShareAPI dbIsVisibleRecord(DBENTRY *pdbentry);

epicsShareFunc long epicsShareAPI dbCreateAlias(DBENTRY *pdbentry,
    const char *paliasName);
epicsShareFunc int epicsShareAPI dbIsAlias(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbDeleteAliases(DBENTRY *pdbentry);

epicsShareFunc long epicsShareAPI dbFindFieldPart(DBENTRY *pdbentry,
    const char **ppname);
epicsShareFunc long epicsShareAPI dbFindField(DBENTRY *pdbentry,
    const char *pfieldName);
epicsShareFunc int epicsShareAPI dbFoundField(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetString(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbPutString(DBENTRY *pdbentry,
    const char *pstring);
epicsShareFunc char * epicsShareAPI dbVerify(DBENTRY *pdbentry,
    const char *pstring);
epicsShareFunc char * epicsShareAPI dbGetRange(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbIsDefaultValue(DBENTRY *pdbentry);

epicsShareFunc long epicsShareAPI dbFirstInfo(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbNextInfo(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbFindInfo(DBENTRY *pdbentry,
    const char *name);
epicsShareFunc long epicsShareAPI dbDeleteInfo(DBENTRY *pdbentry);
epicsShareFunc const char * epicsShareAPI dbGetInfoName(DBENTRY *pdbentry);
epicsShareFunc const char * epicsShareAPI dbGetInfoString(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbPutInfoString(DBENTRY *pdbentry,
    const char *string);
epicsShareFunc long epicsShareAPI dbPutInfoPointer(DBENTRY *pdbentry,
    void *pointer);
epicsShareFunc void * epicsShareAPI dbGetInfoPointer(DBENTRY *pdbentry);
epicsShareFunc const char * epicsShareAPI dbGetInfo(DBENTRY *pdbentry,
    const char *name);
epicsShareFunc long epicsShareAPI dbPutInfo(DBENTRY *pdbentry,
    const char *name, const char *string);

epicsShareFunc brkTable * epicsShareAPI dbFindBrkTable(DBBASE *pdbbase,
    const char *name);

epicsShareFunc dbMenu * epicsShareAPI dbFindMenu(DBBASE *pdbbase,
    const char *name);
epicsShareFunc char ** epicsShareAPI dbGetMenuChoices(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbGetMenuIndex(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbPutMenuIndex(DBENTRY *pdbentry, int index);
epicsShareFunc int  epicsShareAPI dbGetNMenuChoices(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetMenuStringFromIndex(DBENTRY *pdbentry,
    int index);
epicsShareFunc int epicsShareAPI dbGetMenuIndexFromString(DBENTRY *pdbentry,
    const char *choice);

epicsShareFunc drvSup * epicsShareAPI dbFindDriver(dbBase *pdbbase,
    const char *name);

epicsShareFunc int  epicsShareAPI dbAllocForm(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbFreeForm(DBENTRY *pdbentry);
epicsShareFunc char ** epicsShareAPI dbGetFormPrompt(DBENTRY *pdbentry);
epicsShareFunc char ** epicsShareAPI dbGetFormValue(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbPutForm(DBENTRY *pdbentry, char **value);
epicsShareFunc char ** epicsShareAPI dbVerifyForm(DBENTRY *pdbentry,
    char **value);
epicsShareFunc char * epicsShareAPI dbGetRelatedField(DBENTRY *pdbentry);

epicsShareFunc int  epicsShareAPI dbGetNLinks(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbGetLinkField(DBENTRY *pdbentry, int index);
epicsShareFunc int  epicsShareAPI dbGetLinkType(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbCvtLinkToConstant(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbCvtLinkToPvlink(DBENTRY *pdbentry);

/* Dump routines */
epicsShareFunc void epicsShareAPI dbDumpPath(DBBASE *pdbbase);
epicsShareFunc void epicsShareAPI dbDumpRecord(DBBASE *pdbbase,
    const char *precordTypename, int level);
epicsShareFunc void epicsShareAPI dbDumpMenu(DBBASE *pdbbase,
    const char *menuName);
epicsShareFunc void epicsShareAPI dbDumpRecordType(DBBASE *pdbbase,
    const char *recordTypeName);
epicsShareFunc void epicsShareAPI dbDumpField(DBBASE *pdbbase,
    const char *recordTypeName, const char *fname);
epicsShareFunc void epicsShareAPI dbDumpDevice(DBBASE *pdbbase,
    const char *recordTypeName);
epicsShareFunc void epicsShareAPI dbDumpDriver(DBBASE *pdbbase);
epicsShareFunc void epicsShareAPI dbDumpRegistrar(DBBASE *pdbbase);
epicsShareFunc void epicsShareAPI dbDumpFunction(DBBASE *pdbbase);
epicsShareFunc void epicsShareAPI dbDumpVariable(DBBASE *pdbbase);
epicsShareFunc void epicsShareAPI dbDumpBreaktable(DBBASE *pdbbase,
    const char *name);
epicsShareFunc void epicsShareAPI dbPvdDump(DBBASE *pdbbase, int verbose);
epicsShareFunc void epicsShareAPI dbReportDeviceConfig(DBBASE *pdbbase,
    FILE *report);

/* Misc useful routines*/
#define dbCalloc(nobj,size) callocMustSucceed(nobj,size,"dbCalloc")
#define dbMalloc(size) mallocMustSucceed(size,"dbMalloc")
epicsShareFunc void epicsShareAPI dbCatString(char **string, int *stringLength,
    char *pnew, char *separator);

extern int dbStaticDebug;

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
#define S_dbLib_noForm (M_dbLib|25)            /* dbAllocForm was not called */
#define S_dbLib_outMem (M_dbLib|27)            /* Out of memory */
#define S_dbLib_infoNotFound (M_dbLib|29)      /* Info item Not Found */

#ifdef __cplusplus
}
#endif

#endif /*INCdbStaticLibh*/
