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
 */
#ifndef INCellLibh
#define INCellLibh

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __STDC__

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

void ellInit      (ELLLIST *pList);
int  ellCount     (ELLLIST *pList);
ELLNODE *ellFirst    (ELLLIST *pList);
ELLNODE *ellLast     (ELLLIST *pList);
ELLNODE *ellNext     (ELLNODE *pNode);
ELLNODE *ellPrevious (ELLNODE *pNode);

#endif				/*DLLLIB_USE_MACROS*/

void ellAdd       (ELLLIST *pList, ELLNODE *pNode);
void ellConcat    (ELLLIST *pDstList, ELLLIST *pAddList);
void ellDelete    (ELLLIST *pList, ELLNODE *pNode);
void ellExtract   (ELLLIST *pSrcList, ELLNODE *pStartNode, ELLNODE *pEndNode, ELLLIST *pDstList);
ELLNODE *ellGet      (ELLLIST *pList);
void ellInsert    (ELLLIST *plist, ELLNODE *pPrev, ELLNODE *pNode);
ELLNODE *ellNth      (ELLLIST *pList, int nodeNum);
ELLNODE *ellNStep    (ELLNODE *pNode, int nStep);
int  ellFind      (ELLLIST *pList, ELLNODE *pNode);
void ellFree      (ELLLIST *pList);

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

void ellAdd ();
void ellConcat ();
void ellDelete ();
void ellExtract ();
ELLNODE *ellGet ();
void ellInsert ();
ELLNODE *ellNth ();
ELLNODE *ellNStep ();
int  ellFind ();
void ellFree ();
#endif				/*__STDC__*/

#ifdef __cplusplus
}
#endif

#endif				/*INCellLibh*/
