/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*  dbStaticPvt.h */
/*
 *	Author:		Marty Kraimer
 *      Date:           06Jun95
 *
 */

#ifndef INCdbStaticPvth
#define INCdbStaticPvth 1

/*Following are not intended for client code */
dbDeviceMenu *dbGetDeviceMenu(DBENTRY *pdbentry);
void dbFreeLinkContents(struct link *plink);
void dbFreePath(DBBASE *pdbbase);
int dbIsMacroOk(DBENTRY *pdbentry);

/*The following routines have different versions for run-time no-run-time*/
long dbAllocRecord(DBENTRY *pdbentry,char *precordName);
long dbFreeRecord(DBENTRY *pdbentry);

long dbGetFieldAddress(DBENTRY *pdbentry);
char *dbRecordName(DBENTRY *pdbentry);

char *dbGetStringNum(DBENTRY *pdbentry);
long dbPutStringNum(DBENTRY *pdbentry,char *pstring);

void dbGetRecordtypeSizeOffset(dbRecordType *pdbRecordType);

/* The following is for path */
typedef struct dbPathNode {
	ELLNODE		node;
	char		*directory;
} dbPathNode;

/*The following are in dbPvdLib.c*/
/*directory*/
typedef struct{
	ELLNODE		node;
	dbRecordType	*precordType;
	dbRecordNode	*precnode;
}PVDENTRY;
int dbPvdTableSize(int size);
extern int dbStaticDebug;
void	dbPvdInitPvt(DBBASE *pdbbase);
PVDENTRY *dbPvdFind(DBBASE *pdbbase,char *name,int lenname);
PVDENTRY *dbPvdAdd(DBBASE *pdbbase,dbRecordType *precordType,dbRecordNode *precnode);
void dbPvdDelete(DBBASE *pdbbase,dbRecordNode *precnode);
void dbPvdFreeMem(DBBASE *pdbbase);
#endif /*INCdbStaticPvth*/
