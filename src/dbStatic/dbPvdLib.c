/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*dbPvdLib.c*/
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "dbDefs.h"
#include "ellLib.h"
#include "dbBase.h"
#include "epicsStdio.h"
#define epicsExportSharedSymbols
#include "dbStaticLib.h"
#include "dbStaticPvt.h"


int dbPvdHashTableSize = 512;
static int dbPvdHashTableShift;
#define NTABLESIZES 9
static struct {
	unsigned int	tablesize;
	int		shift;
}hashTableParms[9] = {
	{256,0},
	{512,1},
	{1024,2},
	{2048,3},
	{4096,4},
	{8192,5},
	{16384,6},
	{32768,7},
	{65536,8}
};

/*The hash algorithm is a modification of the algorithm described in	*/
/* Fast Hashing of Variable Length Text Strings, Peter K. Pearson,	*/
/* Communications of the ACM, June 1990					*/
/* The modifications were designed by Marty Kraimer			*/

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


static unsigned short hash( const char *pname, int length)
{
    unsigned char h0=0;
    unsigned char h1=0;
    unsigned short ind0,ind1;
    int		isOdd;
    int		i,n;

    isOdd = length%2; /*See if length is odd number of chanacters*/
    n = (isOdd ? (length-1) : length);
    if(isOdd) h0 = T[h0^*(pname + length -1)];
    for(i=0; i<n; i+=2, pname+=2) {
        h0 = T[h0^*pname];
        h1 = T[h1^*(pname+1)];
    }
    ind0 = (unsigned short)h0;
    ind1 = (unsigned short)h1;
    return((ind1<<dbPvdHashTableShift) ^ ind0);
}

int epicsShareAPI dbPvdTableSize(int size)
{
    int		i;

    for(i=0; i< NTABLESIZES; i++) {
	if(size==hashTableParms[i].tablesize) {
	    dbPvdHashTableSize = hashTableParms[i].tablesize;
	    dbPvdHashTableShift = hashTableParms[i].shift;
	    return(0);
	}
    }
    printf("Illegal Size for Process Variable Directory\n");
    return(-1);
}


void    dbPvdInitPvt(dbBase *pdbbase)
{
    ELLLIST	**ppvd;
    int		i;

    for(i=0; i< NTABLESIZES; i++) {
	if((i==NTABLESIZES-1)
	||((dbPvdHashTableSize>=hashTableParms[i].tablesize)
	  && (dbPvdHashTableSize<hashTableParms[i+1].tablesize))) {
	    dbPvdHashTableSize = hashTableParms[i].tablesize;
	    dbPvdHashTableShift = hashTableParms[i].shift;
	    break;
	}
    }
    ppvd = dbCalloc(dbPvdHashTableSize, sizeof(ELLLIST *));
    pdbbase->ppvd = (void *) ppvd;
    return;
}

PVDENTRY *dbPvdFind(dbBase *pdbbase,const char *name,int lenName)
{
    unsigned short	hashInd;
    ELLLIST		**ppvd = (ELLLIST **) pdbbase->ppvd;
    ELLLIST		*pvdlist;
    PVDENTRY		*ppvdNode;
    
    hashInd = hash(name, lenName);
    if ((pvdlist=ppvd[hashInd]) == NULL) return (NULL);
    ppvdNode = (PVDENTRY *) ellFirst(pvdlist);
    while(ppvdNode) {
	if(strcmp(name,ppvdNode->precnode->recordname) == 0)
		return(ppvdNode);
	ppvdNode = (PVDENTRY *) ellNext((ELLNODE*)ppvdNode);
    }
    return (NULL);
}

PVDENTRY *dbPvdAdd(dbBase *pdbbase,dbRecordType *precordType,dbRecordNode *precnode)
{
    unsigned short	hashInd;
    ELLLIST		**ppvd = (ELLLIST **) pdbbase->ppvd;
    ELLLIST		*ppvdlist;
    PVDENTRY		*ppvdNode;
    int			lenName;
    char		*name=precnode->recordname;
    
    lenName=strlen(name);
    hashInd = hash(name, lenName);
    if (ppvd[hashInd] == NULL) {
	ppvd[hashInd] = dbCalloc(1, sizeof(ELLLIST));
	ellInit(ppvd[hashInd]);
    }
    ppvdlist=ppvd[hashInd];
    ppvdNode = (PVDENTRY *) ellFirst(ppvdlist);
    while(ppvdNode) {
	if(strcmp(name,(char *)ppvdNode->precnode->recordname)==0) return(NULL);
	ppvdNode = (PVDENTRY *) ellNext((ELLNODE*)ppvdNode);
    }
    ppvdNode = dbCalloc(1, sizeof(PVDENTRY));
    ellAdd(ppvdlist, (ELLNODE*)ppvdNode);
    ppvdNode->precordType = precordType;
    ppvdNode->precnode = precnode;
    return (ppvdNode);
}

void dbPvdDelete(dbBase *pdbbase,dbRecordNode *precnode)
{
    char	*name=precnode->recordname;
    unsigned short  hashInd;
    ELLLIST	**ppvd = (ELLLIST **) pdbbase->ppvd;
    ELLLIST	*ppvdlist;
    PVDENTRY	*ppvdNode;
    int		lenName;
    
    lenName=strlen(name);
    hashInd = hash(name, lenName);
    if (ppvd[hashInd] == NULL)return;
    ppvdlist=ppvd[hashInd];
    ppvdNode = (PVDENTRY *) ellFirst(ppvdlist);
    while(ppvdNode) {
	if(ppvdNode->precnode && ppvdNode->precnode->recordname
	&& strcmp(name,(char *)ppvdNode->precnode->recordname) == 0) {
	    ellDelete(ppvdlist, (ELLNODE*)ppvdNode);
	    free((void *)ppvdNode);
	    return;
	}
	ppvdNode = (PVDENTRY *) ellNext((ELLNODE*)ppvdNode);
    }
    return;
}

void dbPvdFreeMem(dbBase *pdbbase)
{
    unsigned short  hashInd;
    ELLLIST	**ppvd = (ELLLIST **) pdbbase->ppvd;
    ELLLIST	*ppvdlist;
    PVDENTRY	*ppvdNode;
    PVDENTRY	*next;
    
    if (ppvd == NULL) return;
    for (hashInd=0; hashInd<(unsigned short)dbPvdHashTableSize; hashInd++) {
	if(ppvd[hashInd] == NULL) continue;
	ppvdlist=ppvd[hashInd];
	ppvdNode = (PVDENTRY *) ellFirst(ppvdlist);
	while(ppvdNode) {
	    next = (PVDENTRY *) ellNext((ELLNODE*)ppvdNode);
	    ellDelete(ppvdlist,(ELLNODE*)ppvdNode);
	    free((void *)ppvdNode);
	    ppvdNode = next;
	}
	free((void *)ppvd[hashInd]);
    }
    free((void *)ppvd);
    pdbbase->ppvd = NULL;
}

void epicsShareAPI dbPvdDump(dbBase *pdbbase,int verbose)
{
    unsigned short hashInd;
    ELLLIST	   **ppvd;
    ELLLIST	   *ppvdlist;
    PVDENTRY	   *ppvdNode;
    int		   number;
    
    if(!pdbbase) {
        fprintf(stderr,"pdbbase not specified\n");
        return;
    }
    ppvd = (ELLLIST **)pdbbase->ppvd;
    if (ppvd == NULL) return;
    printf("Process Variable Directory\n");
    printf("dbPvdHashTableSize %d dbPvdHashTableShift %d\n",
	dbPvdHashTableSize,dbPvdHashTableShift);
    for (hashInd=0; hashInd<(unsigned short)dbPvdHashTableSize; hashInd++) {
	if(ppvd[hashInd] == NULL) continue;
	ppvdlist=ppvd[hashInd];
	ppvdNode = (PVDENTRY *) ellFirst(ppvdlist);
	printf("\n%3.3hd=%3.3d ",hashInd,ellCount(ppvdlist));
	number=0;
	while(ppvdNode && verbose) {
	    printf(" %s",(char *)ppvdNode->precnode->recordname);
	    if(number++ ==2) {number=0;printf("\n        ");}
	    ppvdNode = (PVDENTRY *) ellNext((ELLNODE*)ppvdNode);
	}
    }
    printf("\nEnd of Process Variable Directory\n");
}
