/* $Id$
 *
 *	Author:		Marty Kraimer
 *      Date:           06-08-93
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  06-08-93	mrk	Replace dbManipulate
 * .02  06-07-95	mrk	Get rid of default.dctsdr info
 */

#ifndef INCdbStaticLibh
#define INCdbStaticLibh 1

#include <stddef.h>
#include <stdio.h>
#include <dbFldTypes.h>
#include <dbBase.h>
#include <link.h>
#include <errMdef.h>

/*Field types as seen by static database access clients*/
#define DCT_STRING	0
#define DCT_INTEGER	1
#define DCT_REAL	2
#define DCT_MENU	3
#define DCT_MENUFORM	4
#define DCT_INLINK	5
#define DCT_OUTLINK	6
#define DCT_FWDLINK	7
#define DCT_NOACCESS	8

/*Link types as seen by static database access clients*/
#define DCT_LINK_CONSTANT	0
#define DCT_LINK_FORM		1
#define DCT_LINK_PV		2

typedef dbBase DBBASE;

typedef struct{
	DBBASE		*pdbbase;
	dbRecordType	*precordType;
	dbFldDes	*pflddes;
	dbRecordNode	*precnode;
	void	 	*pfield;
	char		*message;
	short		indfield;
	void		*formpvt;
} DBENTRY;

/* Static database access routines*/
epicsShareFunc DBBASE * epicsShareAPI dbAllocBase(void);
epicsShareFunc void epicsShareAPI dbFreeBase(DBBASE *pdbbase);
epicsShareFunc DBENTRY * epicsShareAPI dbAllocEntry(DBBASE *pdbbase);
epicsShareFunc void epicsShareAPI dbFreeEntry(DBENTRY *pdbentry);
epicsShareFunc void epicsShareAPI dbInitEntry(DBBASE *pdbbase,DBENTRY *pdbentry);
epicsShareFunc void epicsShareAPI dbFinishEntry(DBENTRY *pdbentry);
epicsShareFunc DBENTRY * epicsShareAPI dbCopyEntry(DBENTRY *pdbentry);
epicsShareFunc void epicsShareAPI dbCopyEntryContents(DBENTRY *pfrom,DBENTRY *pto);

epicsShareFunc long epicsShareAPI dbReadDatabase(DBBASE **ppdbbase,const char *filename,
	const char *path,const char *substitutions);
epicsShareFunc long epicsShareAPI dbReadDatabaseFP(DBBASE **ppdbbase,FILE *fp,
	const char *path,const char *substitutions);
epicsShareFunc long epicsShareAPI dbPath(DBBASE *pdbbase,const char *path);
epicsShareFunc long epicsShareAPI dbAddPath(DBBASE *pdbbase,const char *path);
epicsShareFunc long epicsShareAPI dbWriteRecord(DBBASE *ppdbbase,const char *filename,
	char *precordTypename,int level);
epicsShareFunc long epicsShareAPI dbWriteRecordFP(DBBASE *ppdbbase,FILE *fp,
	char *precordTypename,int level);
epicsShareFunc long epicsShareAPI dbWriteMenu(DBBASE *pdbbase,const char *filename,char *menuName);
epicsShareFunc long epicsShareAPI dbWriteMenuFP(DBBASE *pdbbase,FILE *fp,char *menuName);
epicsShareFunc long epicsShareAPI dbWriteRecordType(DBBASE *pdbbase,const char *filename,char *recordTypeName);
epicsShareFunc long epicsShareAPI dbWriteRecordTypeFP(DBBASE *pdbbase,FILE *fp,char *recordTypeName);
epicsShareFunc long epicsShareAPI dbWriteDevice(DBBASE *pdbbase,const char *filename);
epicsShareFunc long epicsShareAPI dbWriteDeviceFP(DBBASE *pdbbase,FILE *fp);
epicsShareFunc long epicsShareAPI dbWriteDriver(DBBASE *pdbbase,const char *filename);
epicsShareFunc long epicsShareAPI dbWriteDriverFP(DBBASE *pdbbase,FILE *fp);
epicsShareFunc long epicsShareAPI dbWriteBreaktable(DBBASE *pdbbase,const char *filename);
epicsShareFunc long epicsShareAPI dbWriteBreaktableFP(DBBASE *pdbbase,FILE *fp);

/*Following  are obsolete. For now dbRead calls dbAsciiRead.*/
/*  dbWrite does nothing						*/
#define DB_RECORDTYPE_IO	0x1
#define DB_RECORD_IO		0x2
epicsShareFunc long epicsShareAPI dbRead(DBBASE *pdbbase,FILE *fp);
epicsShareFunc long epicsShareAPI dbWrite(DBBASE *pdbbase,FILE *fpdctsdr,FILE *fp);
epicsShareFunc long epicsShareAPI dbFindRecdes(DBENTRY *pdbentry,char *recdesname);
epicsShareFunc long epicsShareAPI dbFirstRecdes(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbNextRecdes(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetRecdesName(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbGetNRecdes(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbFirstFielddes(DBENTRY *pdbentry,int dctonly);
epicsShareFunc long epicsShareAPI dbNextFielddes(DBENTRY *pdbentry,int dctonly);
epicsShareFunc char ** epicsShareAPI dbGetChoices(DBENTRY *pdbentry);
epicsShareFunc void epicsShareAPI dbDumpRecDes(DBBASE *pdbbase,char *recordTypeName);
/*End obsolete routines*/

epicsShareFunc long epicsShareAPI dbFindRecordType(DBENTRY *pdbentry,char *recordTypename);
epicsShareFunc long epicsShareAPI dbFirstRecordType(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbNextRecordType(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetRecordTypeName(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbGetNRecordTypes(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbPutRecordAttribute(DBENTRY *pdbentry,char *name,char*value);
epicsShareFunc long epicsShareAPI dbGetRecordAttribute(DBENTRY *pdbentry,char *name);

epicsShareFunc long epicsShareAPI dbFirstField(DBENTRY *pdbentry,int dctonly);
epicsShareFunc long epicsShareAPI dbNextField(DBENTRY *pdbentry,int dctonly);
epicsShareFunc int  epicsShareAPI dbGetFieldType(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbGetNFields(DBENTRY *pdbentry,int dctonly);
epicsShareFunc char * epicsShareAPI dbGetFieldName(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetDefault(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetPrompt(DBENTRY *pdbentry);
epicsShareFunc int epicsShareAPI dbGetPromptGroup(DBENTRY *pdbentry);

epicsShareFunc long epicsShareAPI dbCreateRecord(DBENTRY *pdbentry,char *precordName);
epicsShareFunc long epicsShareAPI dbDeleteRecord(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbFreeRecords(DBBASE *pdbbase);
epicsShareFunc long epicsShareAPI dbFindRecord(DBENTRY *pdbentry,const char *precordName);
epicsShareFunc long epicsShareAPI dbFirstRecord(DBENTRY *pdbentry); /*first of record type*/
epicsShareFunc long epicsShareAPI dbNextRecord(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbGetNRecords(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetRecordName(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbRenameRecord(DBENTRY *pdbentry,char *newName);
epicsShareFunc long epicsShareAPI dbCopyRecord(DBENTRY *pdbentry,char *newRecordName,int overWriteOK);

epicsShareFunc long epicsShareAPI dbVisibleRecord(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbInvisibleRecord(DBENTRY *pdbentry);
epicsShareFunc int epicsShareAPI dbIsVisibleRecord(DBENTRY *pdbentry);

epicsShareFunc long epicsShareAPI dbFindField(DBENTRY *pdbentry,const char *pfieldName);
epicsShareFunc int epicsShareAPI dbFoundField(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetString(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbPutString(DBENTRY *pdbentry,char *pstring);
epicsShareFunc char * epicsShareAPI dbVerify(DBENTRY *pdbentry,char *pstring);
epicsShareFunc char * epicsShareAPI dbGetRange(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbIsDefaultValue(DBENTRY *pdbentry);

epicsShareFunc brkTable * epicsShareAPI dbFindBrkTable(DBBASE *pdbbase,char *name);

epicsShareFunc dbMenu * epicsShareAPI dbFindMenu(DBBASE *pdbbase,char *name);
epicsShareFunc char ** epicsShareAPI dbGetMenuChoices(DBENTRY *pdbentry);
epicsShareFunc int  epicsShareAPI dbGetMenuIndex(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbPutMenuIndex(DBENTRY *pdbentry,int index);
epicsShareFunc int  epicsShareAPI dbGetNMenuChoices(DBENTRY *pdbentry);
epicsShareFunc char * epicsShareAPI dbGetMenuStringFromIndex(DBENTRY *pdbentry, int index);
epicsShareFunc int epicsShareAPI dbGetMenuIndexFromString(DBENTRY *pdbentry, char *choice);

epicsShareFunc int  epicsShareAPI dbAllocForm(DBENTRY *pdbentry);
epicsShareFunc long  epicsShareAPI dbFreeForm(DBENTRY *pdbentry);
epicsShareFunc char  ** epicsShareAPI dbGetFormPrompt(DBENTRY *pdbentry);
epicsShareFunc char  ** epicsShareAPI dbGetFormValue(DBENTRY *pdbentry);
epicsShareFunc long  epicsShareAPI dbPutForm(DBENTRY *pdbentry,char **value);
epicsShareFunc char  ** epicsShareAPI dbVerifyForm(DBENTRY *pdbentry,char **value);
epicsShareFunc char * epicsShareAPI dbGetRelatedField(DBENTRY *pdbentry);

epicsShareFunc int  epicsShareAPI dbGetNLinks(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbGetLinkField(DBENTRY *pdbentry,int index);
epicsShareFunc int  epicsShareAPI dbGetLinkType(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbCvtLinkToConstant(DBENTRY *pdbentry);
epicsShareFunc long epicsShareAPI dbCvtLinkToPvlink(DBENTRY *pdbentry);

/*dump routines*/
epicsShareFunc void epicsShareAPI dbDumpPath(DBBASE *pdbbase);
epicsShareFunc void epicsShareAPI dbDumpRecord(DBBASE *pdbbase,char *precordTypename,int level);
epicsShareFunc void epicsShareAPI dbDumpMenu(DBBASE *pdbbase,char *menuName);
epicsShareFunc void epicsShareAPI dbDumpRecordType(DBBASE *pdbbase,char *recordTypeName);
epicsShareFunc void epicsShareAPI dbDumpFldDes(DBBASE *pdbbase,char *recordTypeName,char *fname);
epicsShareFunc void epicsShareAPI dbDumpDevice(DBBASE *pdbbase,char *recordTypeName);
epicsShareFunc void epicsShareAPI dbDumpDriver(DBBASE *pdbbase);
epicsShareFunc void epicsShareAPI dbDumpBreaktable(DBBASE *pdbbase,char *name);
epicsShareFunc void epicsShareAPI dbPvdDump(DBBASE *pdbbase,int verbose);
epicsShareFunc void epicsShareAPI dbReportDeviceConfig(DBBASE *pdbbase,FILE *report);

/* Misc useful routines*/
/*general purpose allocation routines to invoke calloc and maloc	*/
/* NOTE: These routines do NOT return if they fail.			*/
epicsShareFunc void * epicsShareAPI dbCalloc(size_t nobj,size_t size);
epicsShareFunc void * epicsShareAPI dbMalloc(size_t size);
epicsShareFunc void epicsShareAPI dbCatString(char **string,int *stringLength,char *new,char *separator);

extern int dbStaticDebug;

#define S_dbLib_recordTypeNotFound (M_dbLib| 1)	/*Record Type does not exist*/
#define S_dbLib_recExists (M_dbLib| 3)		/*Record Already exists*/
#define S_dbLib_recNotFound (M_dbLib| 5)	/*Record Not Found*/
#define S_dbLib_flddesNotFound (M_dbLib| 7)	/*Field Description Not Found*/
#define S_dbLib_fieldNotFound (M_dbLib| 9)	/*Field Not Found*/
#define S_dbLib_badField (M_dbLib|11)		/*Bad Field value*/
#define S_dbLib_menuNotFound (M_dbLib|13)	/*Menu not found*/
#define S_dbLib_badLink (M_dbLib|15)		/*Bad Link Field*/
#define S_dbLib_nameLength (M_dbLib|17)		/*Record Name is too long*/
#define S_dbLib_noRecSup (M_dbLib|19)		/*Record support not found*/
#define S_dbLib_strLen (M_dbLib|21)		/*String is too long*/
#define S_dbLib_noSizeOffset (M_dbLib|23)	/*Missing SizeOffset Routine - No record support?*/
#define S_dbLib_noForm (M_dbLib|25)		/*dbAllocForm was not called*/
#endif /*INCdbStaticLibh*/
