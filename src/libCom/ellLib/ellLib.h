/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author: John Winans (ANL)
 *              Andrew Johnson <anj@aps.anl.gov>
 */
#ifndef INC_ellLib_H
#define INC_ellLib_H

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ELLNODE {
    struct ELLNODE *next;
    struct ELLNODE *previous;
} ELLNODE;

#define ELLNODE_INIT {NULL, NULL}

typedef struct ELLLIST {
    ELLNODE node;
    int     count;
} ELLLIST;

#define ELLLIST_INIT {ELLNODE_INIT, 0}

typedef void (*FREEFUNC)(void *);

#define ellInit(PLIST) {\
    (PLIST)->node.next = (PLIST)->node.previous = NULL;\
    (PLIST)->count = 0;\
}
#define ellCount(PLIST)    ((PLIST)->count)
#define ellFirst(PLIST)    ((PLIST)->node.next)
#define ellLast(PLIST)     ((PLIST)->node.previous)
#define ellNext(PNODE)     ((PNODE)->next)
#define ellPrevious(PNODE) ((PNODE)->previous)
#define ellFree(PLIST)     ellFree2(PLIST, free)

epicsShareFunc void ellAdd (ELLLIST *pList, ELLNODE *pNode);
epicsShareFunc void ellConcat (ELLLIST *pDstList, ELLLIST *pAddList);
epicsShareFunc void ellDelete (ELLLIST *pList, ELLNODE *pNode);
epicsShareFunc void ellExtract (ELLLIST *pSrcList, ELLNODE *pStartNode, ELLNODE *pEndNode, ELLLIST *pDstList);
epicsShareFunc ELLNODE * ellGet (ELLLIST *pList);
epicsShareFunc ELLNODE * ellPop (ELLLIST *pList);
epicsShareFunc void ellInsert (ELLLIST *plist, ELLNODE *pPrev, ELLNODE *pNode);
epicsShareFunc ELLNODE * ellNth (ELLLIST *pList, int nodeNum);
epicsShareFunc ELLNODE * ellNStep (ELLNODE *pNode, int nStep);
epicsShareFunc int  ellFind (ELLLIST *pList, ELLNODE *pNode);
typedef int (*pListCmp)(const ELLNODE* A, const ELLNODE* B);
epicsShareFunc void ellSortStable(ELLLIST *pList, pListCmp);
epicsShareFunc void ellFree2 (ELLLIST *pList, FREEFUNC freeFunc);
epicsShareFunc void ellVerify (ELLLIST *pList);

#ifdef __cplusplus
}
#endif

#endif /* INC_ellLib_H */
