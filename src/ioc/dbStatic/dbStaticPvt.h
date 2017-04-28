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
 */

#ifndef INCdbStaticPvth
#define INCdbStaticPvth 1

#ifdef __cplusplus
extern "C" {
#endif

/*Following are not intended for client code */
dbDeviceMenu *dbGetDeviceMenu(DBENTRY *pdbentry);
void dbFreeLinkContents(struct link *plink);
void dbFreePath(DBBASE *pdbbase);
int dbIsMacroOk(DBENTRY *pdbentry);

/*The following routines have different versions for run-time no-run-time*/
long dbAllocRecord(DBENTRY *pdbentry,const char *precordName);
long dbFreeRecord(DBENTRY *pdbentry);

long dbGetFieldAddress(DBENTRY *pdbentry);
char *dbRecordName(DBENTRY *pdbentry);

char *dbGetStringNum(DBENTRY *pdbentry);
long dbPutStringNum(DBENTRY *pdbentry,const char *pstring);

struct jlink;

typedef struct dbLinkInfo {
    short ltype;

    /* full link string for CONSTANT and PV_LINK,
     * parm string for HW links, JSON for JSON_LINK
     */
    char *target;

    /* for PV_LINK */
    short modifiers;

    /* for HW links */
    char hwid[6]; /* one extra element for a nil */
    int  hwnums[5];

    /* for JSON_LINK */
    struct jlink *jlink;
} dbLinkInfo;

long dbInitRecordLinks(dbRecordType *rtyp, struct dbCommon *prec);

#define LINK_DEBUG_LSET 1
#define LINK_DEBUG_JPARSE 2

/* Parse link string.  no record locks needed.
 * on success caller must free pinfo->target
 */
epicsShareFunc long dbParseLink(const char *str, short ftype, dbLinkInfo *pinfo, unsigned opts);
/* Check if link type allow the parsed link value pinfo
 * to be assigned to the given link.
 * Record containing plink must be locked.
 * Frees pinfo->target on failure.
 */
long dbCanSetLink(DBLINK *plink, dbLinkInfo *pinfo, devSup *devsup);
/* Set link field.  source record must be locked (target record too
 * when a DB_LINK is created)
 * Unconditionally takes ownership of pinfo->target
 */
long dbSetLink(DBLINK *plink, dbLinkInfo *pinfo, devSup *dset);
/* Free dbLinkInfo storage */
epicsShareFunc void dbFreeLinkInfo(dbLinkInfo *pinfo);

/* The following is for path */
typedef struct dbPathNode {
	ELLNODE		node;
	char		*directory;
} dbPathNode;

/* Element of the global gui group list */
typedef struct dbGuiGroup {
    ELLNODE     node;
    short       key;
    char        *name;
} dbGuiGroup;

/*The following are in dbPvdLib.c*/
/*directory*/
typedef struct{
	ELLNODE		node;
	dbRecordType	*precordType;
	dbRecordNode	*precnode;
}PVDENTRY;
epicsShareFunc int dbPvdTableSize(int size);
extern int dbStaticDebug;
void	dbPvdInitPvt(DBBASE *pdbbase);
PVDENTRY *dbPvdFind(DBBASE *pdbbase,const char *name,size_t lenname);
PVDENTRY *dbPvdAdd(DBBASE *pdbbase,dbRecordType *precordType,dbRecordNode *precnode);
void dbPvdDelete(DBBASE *pdbbase,dbRecordNode *precnode);
void dbPvdFreeMem(DBBASE *pdbbase);

#ifdef __cplusplus
}
#endif
#endif /*INCdbStaticPvth*/
