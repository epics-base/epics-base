/* ellLib.h  $Id$
 *
 *      Author: John Winans (ANL)
 *      Date:   07-02-92
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
 * .01  jrw     07-02-92        created
 * .02  rcz     07-23-93        changed name
 * .03  rcz     07-26-93        changed name again
 * .04  joh		12-12-97		added ellverify()
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
