/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* ellLib.h  $Id$
 *
 *      Author: John Winans (ANL)
 *      Date:   07-02-92
 *
 */
#ifndef INCellLibh
#define INCellLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

#define DLLLIB_USE_MACROS

struct ELLNODE {
  struct ELLNODE  *next;
  struct ELLNODE  *previous;
};
typedef struct ELLNODE ELLNODE;

struct ELLLIST {
  ELLNODE  node;
  int   count;
};
typedef struct ELLLIST ELLLIST;

#if defined(__STDC__) || defined(__cplusplus)

#ifdef DLLLIB_USE_MACROS

#define ellInit(PLIST)	{ ((ELLLIST *)(PLIST))->node.next = NULL;\
			  ((ELLLIST *)(PLIST))->node.previous = NULL;\
			  ((ELLLIST *)(PLIST))->count = 0; }

#define ellCount(PLIST)		(((ELLLIST *)(PLIST))->count)

#define	ellFirst(PLIST)		(((ELLLIST *)(PLIST))->node.next)

#define ellLast(PLIST)		(((ELLLIST *)(PLIST))->node.previous)

#define ellNext(PNODE)		(((ELLNODE *)(PNODE))->next)

#define	ellPrevious(PNODE)	(((ELLNODE *)(PNODE))->previous)

#else				/*DLLLIB_USE_MACROS*/

epicsShareFunc void epicsShareAPI ellInit (ELLLIST *pList);
epicsShareFunc int epicsShareAPI ellCount (ELLLIST *pList);
epicsShareFunc ELLNODE * epicsShareAPI ellFirst (ELLLIST *pList);
epicsShareFunc ELLNODE * epicsShareAPI ellLast (ELLLIST *pList);
epicsShareFunc ELLNODE * epicsShareAPI ellNext (ELLNODE *pNode);
epicsShareFunc ELLNODE * epicsShareAPI ellPrevious (ELLNODE *pNode);

#endif				/*DLLLIB_USE_MACROS*/

epicsShareFunc void epicsShareAPI ellAdd (ELLLIST *pList, ELLNODE *pNode);
epicsShareFunc void epicsShareAPI ellConcat (ELLLIST *pDstList, ELLLIST *pAddList);
epicsShareFunc void epicsShareAPI ellDelete (ELLLIST *pList, ELLNODE *pNode);
epicsShareFunc void epicsShareAPI ellExtract (ELLLIST *pSrcList, ELLNODE *pStartNode, ELLNODE *pEndNode, ELLLIST *pDstList);
epicsShareFunc ELLNODE * epicsShareAPI ellGet (ELLLIST *pList);
epicsShareFunc void epicsShareAPI ellInsert (ELLLIST *plist, ELLNODE *pPrev, ELLNODE *pNode);
epicsShareFunc ELLNODE * epicsShareAPI ellNth (ELLLIST *pList, int nodeNum);
epicsShareFunc ELLNODE * epicsShareAPI ellNStep (ELLNODE *pNode, int nStep);
epicsShareFunc int  epicsShareAPI ellFind (ELLLIST *pList, ELLNODE *pNode);
epicsShareFunc void epicsShareAPI ellFree (ELLLIST *pList);
epicsShareFunc void epicsShareAPI ellVerify (ELLLIST *pList);

#else				/*__STDC__*/

#ifdef DLLLIB_USE_MACROS

#define ellInit(PLIST)	{ ((ELLLIST *)(PLIST))->node.next = NULL;\
			  ((ELLLIST *)(PLIST))->node.previous = NULL;\
			  ((ELLLIST *)(PLIST))->count = 0; }

#define ellCount(PLIST)		(((ELLLIST *)(PLIST))->count)

#define	ellFirst(PLIST)		(((ELLLIST *)(PLIST))->node.next)

#define ellLast(PLIST)		(((ELLLIST *)(PLIST))->node.previous)

#define ellNext(PNODE)		(((ELLNODE *)(PNODE))->next)

#define	ellPrevious(PNODE)	(((ELLNODE *)(PNODE))->previous)

#else				/*DLLLIB_USE_MACROS*/

void ellInit ();
int  ellCount ();
ELLNODE *ellFirst ();
ELLNODE *ellNext ();
ELLNODE *ellLast ();
ELLNODE *ellPrevious ();

#endif				/*DLLLIB_USE_MACROS*/

epicsShareFunc void epicsShareAPI ellAdd ();
epicsShareFunc void epicsShareAPI ellConcat ();
epicsShareFunc void epicsShareAPI ellDelete ();
epicsShareFunc void epicsShareAPI ellExtract ();
epicsShareFunc ELLNODE * epicsShareAPI ellGet ();
epicsShareFunc void epicsShareAPI ellInsert ();
epicsShareFunc ELLNODE * epicsShareAPI ellNth ();
epicsShareFunc ELLNODE * epicsShareAPI ellNStep ();
epicsShareFunc int epicsShareAPI ellFind ();
epicsShareFunc void epicsShareAPI ellFree ();
epicsShareFunc void ellVerify ();

#endif				/*__STDC__*/

#ifdef __cplusplus
}
#endif

#endif				/*INCellLibh*/
