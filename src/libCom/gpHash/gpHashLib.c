/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$ */
/* Author:  Marty Kraimer Date:    04-07-94 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#define epicsExportSharedSymbols
#include "cantProceed.h"
#include "epicsMutex.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "epicsPrint.h"
#include "gpHash.h"

typedef struct gphPvt {
    int		tableSize;
    int		nShift;
    ELLLIST	**paplist; /*pointer to array of pointers to ELLLIST */
    epicsMutexId lock;
}gphPvt;


/*The hash algorithm is the algorithm described in			*/
/* Fast Hashing of Variable Length Text Strings, Peter K. Pearson,	*/
/* Communications of the ACM, June 1990					*/

static unsigned char T[256] = {
 39,159,180,252, 71,  6, 13,164,232, 35,226,155, 98,120,154, 69,
157, 24,137, 29,147, 78,121, 85,112,  8,248,130, 55,117,190,160,
176,131,228, 64,211,106, 38, 27,140, 30, 88,210,227,104, 84, 77,
 75,107,169,138,195,184, 70, 90, 61,166,  7,244,165,108,219, 51,
  9,139,209, 40, 31,202, 58,179,116, 33,207,146, 76, 60,242,124,
254,197, 80,167,153,145,129,233,132, 48,246, 86,156,177, 36,187,
 45,  1, 96, 18, 19, 62,185,234, 99, 16,218, 95,128,224,123,253,
 42,109,  4,247, 72,  5,151,136,  0,152,148,127,204,133, 17, 14,
182,217, 54,199,119,174, 82, 57,215, 41,114,208,206,110,239, 23,
189, 15,  3, 22,188, 79,113,172, 28,  2,222, 21,251,225,237,105,
102, 32, 56,181,126, 83,230, 53,158, 52, 59,213,118,100, 67,142,
220,170,144,115,205, 26,125,168,249, 66,175, 97,255, 92,229, 91,
214,236,178,243, 46, 44,201,250,135,186,150,221,163,216,162, 43,
 11,101, 34, 37,194, 25, 50, 12, 87,198,173,240,193,171,143,231,
111,141,191,103, 74,245,223, 20,161,235,122, 63, 89,149, 73,238,
134, 68, 93,183,241, 81,196, 49,192, 65,212, 94,203, 10,200, 47 
};

#define NSIZES 9
static int allowSize[NSIZES] = {256,512,1024,2048,4096,8192,16384,32768,65536};

static int hash(const char *pname,int nShift)
{
    unsigned char	h0=0;
    unsigned char	h1=0;
    unsigned short	ind0,ind1;
    int			even = TRUE;
    unsigned char	c;

    while(*pname) {
	c = *pname;
	if(even) {h0 = T[h0^c]; even = FALSE;}
	else {h1 = T[h1^c]; even = TRUE;}
	pname++;
    }
    ind0 = (unsigned short)h0;
    ind1 = (unsigned short)h1;
    return((ind1<<nShift) ^ ind0);
}

void epicsShareAPI gphInitPvt(void **ppvt,int size)
{
    gphPvt *pgphPvt;
    int	   i;
    int	   tableSize=0;
    int	   nShift=0;

    for(i=0; i<NSIZES; i++) {
	if(size==allowSize[i]) {
	    tableSize = size;
	    nShift = i;
	}
    }
    if(tableSize==0) {
	epicsPrintf("gphInitPvt: Illegal size\n");
	return;
    }
    pgphPvt = callocMustSucceed(1, sizeof(gphPvt), "gphInitPvt");
    pgphPvt->tableSize = tableSize;
    pgphPvt->nShift = nShift;
    pgphPvt->paplist = callocMustSucceed(tableSize, sizeof(ELLLIST *), "gphInitPvt");
    pgphPvt->lock = epicsMutexMustCreate();
    *ppvt = (void *)pgphPvt;
    return;
}
	
GPHENTRY * epicsShareAPI gphFind(void *pvt,const char *name,void *pvtid)
{
    int		hashInd;
    gphPvt	*pgphPvt = (gphPvt *)pvt;
    ELLLIST	**paplist;
    ELLLIST	*gphlist;
    GPHENTRY	*pgphNode;
    
    if(pgphPvt==NULL) return(NULL);
    paplist = pgphPvt->paplist;
    hashInd = hash(name,pgphPvt->nShift);
    epicsMutexMustLock(pgphPvt->lock);
    if ((gphlist=paplist[hashInd]) == NULL) {
	pgphNode = NULL;
    } else {
	pgphNode = (GPHENTRY *) ellFirst(gphlist);
    }
    while(pgphNode) {
	if(strcmp(name,(char *)pgphNode->name) == 0) {
	    if(pvtid==pgphNode->pvtid) break;
	}
	pgphNode = (GPHENTRY *) ellNext((ELLNODE*)pgphNode);
    }
    epicsMutexUnlock(pgphPvt->lock);
    return(pgphNode);
}

GPHENTRY * epicsShareAPI gphAdd(void *pvt,const char *name,void *pvtid)
{
    int		hashInd;
    gphPvt	*pgphPvt = (gphPvt *)pvt;
    ELLLIST	**paplist;
    ELLLIST	*plist;
    GPHENTRY	*pgphNode;
    
    if(pgphPvt==NULL) return(NULL);
    paplist = pgphPvt->paplist;
    hashInd = hash(name,pgphPvt->nShift);
    epicsMutexMustLock(pgphPvt->lock);
    if(paplist[hashInd] == NULL) {
	paplist[hashInd] = callocMustSucceed(1, sizeof(ELLLIST), "gphAdd");
	ellInit(paplist[hashInd]);
    }
    plist=paplist[hashInd];
    pgphNode = (GPHENTRY *) ellFirst(plist);
    while(pgphNode) {
	if((strcmp(name,(char *)pgphNode->name) == 0)
	&&(pvtid == pgphNode->pvtid)) {
            epicsMutexUnlock(pgphPvt->lock);
	    return(NULL);
	}
	pgphNode = (GPHENTRY *) ellNext((ELLNODE*)pgphNode);
    }
    pgphNode = callocMustSucceed(1, (unsigned) sizeof(GPHENTRY), "gphAdd");
    pgphNode->name = name;
    pgphNode->pvtid = pvtid;
    ellAdd(plist, (ELLNODE*)pgphNode);
    epicsMutexUnlock(pgphPvt->lock);
    return (pgphNode);
}

void epicsShareAPI gphDelete(void *pvt,const char *name,void *pvtid)
{
    int		hashInd;
    gphPvt	*pgphPvt = (gphPvt *)pvt;
    ELLLIST	**paplist;
    ELLLIST	*plist = NULL;
    GPHENTRY	*pgphNode;
    
    if(pgphPvt==NULL) return;
    paplist = pgphPvt->paplist;
    hashInd = hash(name,pgphPvt->nShift);
    epicsMutexMustLock(pgphPvt->lock);
    if(paplist[hashInd] == NULL) {
	pgphNode = NULL;
    } else {
	plist=paplist[hashInd];
	pgphNode = (GPHENTRY *) ellFirst(plist);
    }
    while(pgphNode) {
	if((strcmp(name,(char *)pgphNode->name) == 0)
	&&(pvtid == pgphNode->pvtid)) {
	    ellDelete(plist, (ELLNODE*)pgphNode);
	    free((void *)pgphNode);
	    break;
	}
	pgphNode = (GPHENTRY *) ellNext((ELLNODE*)pgphNode);
    }
    epicsMutexUnlock(pgphPvt->lock);
    return;
}

void epicsShareAPI gphFreeMem(void *pvt)
{
    int		hashInd;
    gphPvt	*pgphPvt = (gphPvt *)pvt;
    ELLLIST	**paplist;
    ELLLIST	*plist;
    GPHENTRY	*pgphNode;
    GPHENTRY	*next;;
    
    /*caller must ensure that no other thread is using *pvt */
    if(pgphPvt==NULL) return;
    paplist = pgphPvt->paplist;
    for (hashInd=0; hashInd<pgphPvt->tableSize; hashInd++) {
	if(paplist[hashInd] == NULL) continue;
	plist=paplist[hashInd];
	pgphNode = (GPHENTRY *) ellFirst(plist);
	while(pgphNode) {
	    next = (GPHENTRY *) ellNext((ELLNODE*)pgphNode);
	    ellDelete(plist,(ELLNODE*)pgphNode);
	    free((void *)pgphNode);
	    pgphNode = next;
	}
	free((void *)paplist[hashInd]);
    }
    epicsMutexDestroy(pgphPvt->lock);
    free((void *)paplist);
    free((void *)pgphPvt);
}

void epicsShareAPI gphDump(void *pvt)
{
    gphDumpFP(stdout,pvt);
}

void epicsShareAPI gphDumpFP(FILE *fp,void *pvt)
{
    int		hashInd;
    gphPvt	*pgphPvt = (gphPvt *)pvt;
    ELLLIST	**paplist;
    ELLLIST	*plist;
    GPHENTRY	*pgphNode;
    int		number;
    
    if(pgphPvt==NULL) return;
    paplist = pgphPvt->paplist;
    for (hashInd=0; hashInd<pgphPvt->tableSize; hashInd++) {
	if(paplist[hashInd] == NULL) continue;
	plist=paplist[hashInd];
	pgphNode = (GPHENTRY *) ellFirst(plist);
	fprintf(fp,"\n %3.3hd=%3.3d",hashInd,ellCount(plist));
	number=0;
	while(pgphNode) {
	    fprintf(fp," %s %p",pgphNode->name,pgphNode->pvtid);
	    if(number++ ==2) {number=0;fprintf(fp,"\n        ");}
	    pgphNode = (GPHENTRY *) ellNext((ELLNODE*)pgphNode);
	}
    }
    fprintf(fp,"\n End of General Purpose Hash\n");
}
