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
 */
#ifndef INCellLibh
#define INCellLibh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DLLLIB_USE_MACROS

typedef struct ELLNODE {
  struct ELLNODE  *next;
  struct ELLNODE  *previous;
}ELLNODE;

typedef struct ELLLIST {
  ELLNODE  node;
  int   count;
}ELLLIST;

#ifdef DLLLIB_USE_MACROS

#define ellInit(PLIST)	{ (PLIST)->node.next = NULL;\
			  (PLIST)->node.previous = NULL;\
			  (PLIST)->count = 0; }

#define ellCount(PLIST)		((PLIST)->count)

#define	ellFirst(PLIST)		((PLIST)->node.next)

#define ellLast(PLIST)		((PLIST)->node.previous)

#define ellNext(PNODE)		((PNODE)->next)

#define	ellPrevious(PNODE)	((PNODE)->previous)

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
/* use of ellFree on windows causes problems because the malloc and free are not in the same DLL */
epicsShareFunc void epicsShareAPI ellFree (ELLLIST *pList);
epicsShareFunc void epicsShareAPI ellVerify (ELLLIST *pList);

#ifdef __cplusplus
}
#endif

#endif				/*INCellLibh*/
