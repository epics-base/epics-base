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
 */

#ifndef INCdbStaticLibh
#define INCdbStaticLibh 1

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <ellLib.h>
#include <dbDefs.h>
#include <dbRecords.h>
#include <dbRecType.h>
#include <dbFldTypes.h>
#include <dbRecDes.h>
#include <choice.h>
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
#define DCT_LINK_DEVICE		3

/* conversion types for DBFLDDES.cvt_type*/
#define CT_DECIMAL 0
#define CT_HEX     1

/*options for dbRead and dbWrite*/
#define DB_RECDES_IO		0x1
#define DB_RECORD_IO		0x2

typedef struct dbBase DBBASE;

typedef struct recNode {
    ELLNODE            node;
    void           *precord;
}RECNODE;

typedef struct{
	DBBASE		*pdbbase;
	RECNODE		*precnode;
	struct fldDes	*pflddes;
	void	 	*pfield;
	char		*message;
	short		record_type;
	short		indfield;
	void		*formpvt;
} DBENTRY;


/*directory*/
typedef struct{
	ELLNODE            node;
	unsigned short	record_type;
	RECNODE		*precnode;
} PVDENTRY;

/* Static database access routines*/
#if defined(__STDC__) || defined(__cplusplus)
void * dbCalloc(size_t nobj,size_t size);
void * dbMalloc(size_t size);
DBBASE *dbAllocBase(void);
void dbFreeBase(DBBASE *pdbbase);
DBENTRY *dbAllocEntry(DBBASE *pdbbase);
void dbFreeEntry(DBENTRY *pdbentry);
void dbInitEntry(DBBASE *pdbbase,DBENTRY *pdbentry);
void dbFinishEntry(DBENTRY *pdbentry);
DBENTRY *dbCopyEntry(DBENTRY *pdbentry);

long dbRead(DBBASE *pdbbase,FILE *fp);
long dbWrite(DBBASE *pdbbase,FILE *fpdctsdr,FILE *fp);

long dbFindRecdes(DBENTRY *pdbentry,char *recdesname);
long dbFirstRecdes(DBENTRY *pdbentry);
long dbNextRecdes(DBENTRY *pdbentry);
char *dbGetRecdesName(DBENTRY *pdbentry);
int  dbGetNRecdes(DBENTRY *pdbentry);
long dbCopyRecdes(DBENTRY *from,DBENTRY *to);

long dbCreateRecord(DBENTRY *pdbentry,char *precordName);
long dbDeleteRecord(DBENTRY *pdbentry);
long dbFindRecord(DBENTRY *pdbentry,char *precordName);
long dbFirstRecord(DBENTRY *pdbentry); /*first of record type*/
long dbNextRecord(DBENTRY *pdbentry);
int  dbGetNRecords(DBENTRY *pdbentry);
char *dbGetRecordName(DBENTRY *pdbentry);
long dbRenameRecord(DBENTRY *pdbentry,char *newName);
long dbCopyRecord(DBENTRY *from,DBENTRY *to);

long dbFindField(DBENTRY *pdbentry,char *pfieldName);
long dbFirstFielddes(DBENTRY *pdbentry,int dctonly);
long dbNextFielddes(DBENTRY *pdbentry,int dctonly);
int  dbGetFieldType(DBENTRY *pdbentry);
int  dbGetNFields(DBENTRY *pdbentry,int dctonly);
char *dbGetFieldName(DBENTRY *pdbentry);
char *dbGetPrompt(DBENTRY *pdbentry);
int dbGetPromptGroup(DBENTRY *pdbentry);

char *dbGetString(DBENTRY *pdbentry);
long dbPutString(DBENTRY *pdbentry,char *pstring);
char *dbVerify(DBENTRY *pdbentry,char *pstring);
char *dbGetRange(DBENTRY *pdbentry);
int  dbIsDefaultValue(DBENTRY *pdbentry);

char **dbGetChoices(DBENTRY *pdbentry);
int  dbGetMenuIndex(DBENTRY *pdbentry);
long dbPutMenuIndex(DBENTRY *pdbentry,int index);
int  dbGetNMenuChoices(DBENTRY *pdbentry);
long dbCopyMenu(DBENTRY *from,DBENTRY *to);

int  dbAllocForm(DBENTRY *pdbentry);
long  dbFreeForm(DBENTRY *pdbentry);
char  **dbGetFormPrompt(DBENTRY *pdbentry);
char  **dbGetFormValue(DBENTRY *pdbentry);
long  dbPutForm(DBENTRY *pdbentry,char **value);
char  **dbVerifyForm(DBENTRY *pdbentry,char **value);

int  dbGetNLinks(DBENTRY *pdbentry);
long dbGetLinkField(DBENTRY *pdbentry,int index);
int  dbGetLinkType(DBENTRY *pdbentry);
long dbCvtLinkToConstant(DBENTRY *pdbentry);
long dbCvtLinkToPvlink(DBENTRY *pdbentry);
long dbPutPvlink(DBENTRY *pdbentry,int pp,int ms,char *pvname);
long dbGetPvlink(DBENTRY *pdbentry,int *pp,int *ms,char *pvname);

/*dump routines*/
void dbDumpRecords(DBBASE *pdbbase,char *precdesname,int modOnly);
void dbDumpPvd(DBBASE *pdbbase);
void dbReportDeviceConfig(DBBASE *pdbbase,FILE *report);

/* Misc useful routines*/
/*general purpose allocation routines to invoke calloc and maloc	*/
/* NOTE: These routines do NOT return if they fail.			*/
void *dbCalloc(size_t nobj,size_t size);
void *dbMalloc(size_t size);

/* The following routines should only be used by access routines*/
void	dbPvdInitPvt(DBBASE *pdbbase);
PVDENTRY *dbPvdFind(DBBASE *pdbbase,char *name,int lenname);
PVDENTRY *dbPvdAdd(DBBASE *pdbbase,unsigned short record_type,RECNODE *precnode);
void dbPvdDelete(DBBASE *pdbbase,RECNODE *precnod);
void dbPvdFreeMem(DBBASE *pdbbase);
void dbPvdDump(DBBASE *pdbbase);
#else
void * dbCalloc();
void * dbMalloc();
DBBASE *dbAllocBase();
void dbFreeBase();
DBENTRY *dbAllocEntry();
void dbFreeEntry();
void dbInitEntry();
void dbFinishEntry();
DBENTRY *dbCopyEntry();

long dbRead();
long dbWrite();

long dbFindRecdes();
long dbFirstRecdes();
long dbNextRecdes();
char *dbGetRecdesName();
int  dbGetNRecdes();
long dbCopyRecdes();

long dbCreateRecord();
long dbDeleteRecord();
long dbFindRecord();
long dbFirstRecord();
long dbNextRecord();
int  dbGetNRecords();
char *dbGetRecordName();
long dbRenameRecord();
long dbCopyRecord();

long dbFindField();
long dbFirstFielddes();
long dbNextFielddes();
int  dbGetFieldType();
int  dbGetNFields();
char *dbGetFieldName();
char *dbGetPrompt();

char *dbGetString();
long dbPutString();
char *dbVerify();
char *dbGetRange();
int  dbIsDefaultValue();

char **dbGetChoices();
int  dbGetMenuIndex();
long dbPutMenuIndex();
int  dbGetNMenuChoices();
long dbCopyMenu();

int  dbAllocForm();
long  dbFreeForm();
char  **dbGetFormPrompt();
char  **dbGetFormValue();
long  dbPutForm();
char  **dbVerifyForm();

int  dbGetNLinks();
long dbGetLinkField();
int  dbGetLinkType();
long dbCvtLinkToConstant();
long dbCvtLinkToPvlink();
long dbPutPvlink();
long dbGetPvlink();

/*dump routines*/
void dbDumpRecords();
void dbDumpPvd();

/* Misc useful routines*/
/*general purpose allocation routines to invoke calloc and maloc	*/
/* NOTE: These routines do NOT return if they fail.			*/
void *dbCalloc();
void *dbMalloc();

/* The following routines should only be used by access routines*/
void	dbPvdInitPvt();
PVDENTRY *dbPvdFind();
PVDENTRY *dbPvdAdd();
void dbPvdDelete();
void dbPvdFreeMem();
void dbPvdDump();
#endif /*__STDC__*/

#define S_dbLib_recdesNotFound (M_dbLib| 1)	/*Record Type does not exist*/
#define S_dbLib_recExists (M_dbLib| 3)		/*Record Already exists*/
#define S_dbLib_recNotFound (M_dbLib| 5)	/*Record Not Found*/
#define S_dbLib_flddesNotFound (M_dbLib| 7)	/*Field Description Not Found*/
#define S_dbLib_fieldNotFound (M_dbLib| 9)	/*Field Not Found*/
#define S_dbLib_badField (M_dbLib|11)		/*Bad Field value*/
#define S_dbLib_menuNotFound (M_dbLib|13)	/*Menu not found*/
#define S_dbLib_badLink (M_dbLib|15)		/*Bad Link Field*/
#define S_dbLib_nameLength (M_dbLib|17)		/*Record Name is too long*/
#endif /*INCdbStaticLibh*/
