/* $Id$
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
 * .01	jrw	07-02-92	created
 * .02	rcz	08-26-92	moved to libCom
 */

/* #define	DEBUG_DRIVER */

#ifdef vxWorks
#include <vxWorks.h>
#endif

#include <stdlib.h>

#define epicsExportSharedSymbols
#include "ellLib.h"

#if     !defined(NULL) 
#define NULL            0
#endif

/****************************************************************************
 *
 * Initialize a new linked list header structure.
 *
 *****************************************************************************/
#ifndef DLLLIB_USE_MACROS
#ifdef __STDC__
void epicsShareAPI ellInit (ELLLIST *pList)
#else
void epicsShareAPI ellInit (plist)
ELLLIST	*plist;
#endif
{
  plist->node.next = NULL;
  plist->node.previous = NULL;
  plist->count = 0;
  return;
}
#endif
/****************************************************************************
 *
 * This function adds a node to the end of a linked list.
 *
 *****************************************************************************/
#ifdef __STDC__
void epicsShareAPI ellAdd (ELLLIST *pList, ELLNODE *pNode)
#else
void epicsShareAPI ellAdd (pList, pNode)
ELLLIST *pList;
ELLNODE *pNode;
#endif
{
  pNode->next = NULL;
  pNode->previous = pList->node.previous;

  if (pList->count)
    pList->node.previous->next = pNode;
  else
    pList->node.next = pNode;

  pList->node.previous = pNode;
  pList->count++;

  return;
}
/****************************************************************************
 *
 * This function concatinates the second linked list to the end of the first
 * list.  The second list is left empty.  Either list (or both) lists may
 * be empty at the begining of the operation.
 *
 *****************************************************************************/
#ifdef __STDC__
void epicsShareAPI ellConcat (ELLLIST *pDstList, ELLLIST *pAddList)
#else
void epicsShareAPI ellConcat (pDstList, pAddList)
ELLLIST *pDstList;
ELLLIST *pAddList;
#endif
{
  if (pAddList->count == 0)
    return;	/* Add list is empty, nothing to add. */

  if (pDstList->count == 0)
  { /* Destination list is empty... just transfer the add list over. */
    pDstList->node.next = pAddList->node.next;
    pDstList->node.previous = pAddList->node.previous;
    pDstList->count = pAddList->count;
  }
  else
  { /* Destination list not empty... append the add list. */
    pDstList->node.previous->next = pAddList->node.next;
    pAddList->node.next->previous = pDstList->node.previous;
    pDstList->node.previous = pAddList->node.previous;
    pDstList->count += pAddList->count;
  }

  pAddList->count = 0;
  pAddList->node.next = NULL;
  pAddList->node.previous = NULL;

  return;
}
/****************************************************************************
 *
 * This function returns the number of nodes that are found in a list.
 *
 *****************************************************************************/
#ifndef DLLLIB_USE_MACROS
#ifdef __STDC__
int  epicsShareAPI ellCount (ELLLIST *pList)
#else
int  epicsShareAPI ellCount (pList)
ELLLIST *pList;
#endif
{
  return(pList->count);
}
#endif
/****************************************************************************
 *
 * This function deletes a specific node from a specified list;
 *
 *****************************************************************************/
#ifdef __STDC__
void epicsShareAPI ellDelete (ELLLIST *pList, ELLNODE *pNode)
#else
void epicsShareAPI ellDelete (pList, pNode)
ELLLIST *pList;
ELLNODE *pNode;
#endif
{
  if (pList->node.previous == pNode)
    pList->node.previous = pNode->previous;
  else
    pNode->next->previous = pNode->previous;

  if (pList->node.next == pNode)
    pList->node.next = pNode->next;
  else
    pNode->previous->next = pNode->next;

  pList->count--;

  return;
}
/****************************************************************************
 *
 * This function extracts a sublist that starts with pStartNode and ends with
 * pEndNode from pSrcList and places it in pDstList.
 *
 * WRS is unclear as to what happens when pDstList is non-empty at the start
 * of the operation.  We will place the extracted list at the END of pDstList
 * when it is non-empty.
 *
 *****************************************************************************/
#ifdef __STDC__
void epicsShareAPI ellExtract (ELLLIST *pSrcList, ELLNODE *pStartNode, ELLNODE *pEndNode, ELLLIST *pDstList)
#else
void epicsShareAPI ellExtract (pSrcList, pStartNode, pEndNode, pDstList)
ELLLIST *pSrcList;
ELLNODE *pStartNode;
ELLNODE *pEndNode;
ELLLIST *pDstList;
#endif
{
  ELLNODE	*pnode;
  int	count;

  /* Cut the list out of the source list (update count later) */
  if (pStartNode->previous != NULL)
    pStartNode->previous->next = pEndNode->next;
  else
    pSrcList->node.next = pEndNode->next;

  if (pEndNode->next != NULL)
  {
    pEndNode->next->previous = pStartNode->previous;
    pEndNode->next = NULL;
  }
  else
    pSrcList->node.previous = pStartNode->previous;

  /* Place the sublist into the destination list */
  pStartNode->previous = pDstList->node.previous;
  if (pDstList->count)
    pDstList->node.previous->next = pStartNode;
  else
    pDstList->node.next = pStartNode;

  pDstList->node.previous = pEndNode;

  /* Adjust the counts */
  pnode = pStartNode;
  count = 1;
  while(pnode != pEndNode)
  {
    pnode = pnode->next;
    count++;
  }
  pSrcList->count -= count;
  pDstList->count += count;

  return;
}
/****************************************************************************
 *
 * This function returns the first node in the specified list.  The node is 
 * NOT removed from the list.  If the list is empty, NULL will be returned.
 *
 *****************************************************************************/
#ifndef DLLLIB_USE_MACROS
#ifdef __STDC__
ELLNODE * epicsShareAPI ellFirst (ELLLIST *pList)
#else
ELLNODE * epicsShareAPI ellFirst (pList)
ELLLIST *pList;
#endif
{
  return(pList->node.next);
}
#endif
/****************************************************************************
 *
 * This function returns the first node in the specified list.  The node is
 * removed from the list.  If the list is empty, NULL will be returned.
 *
 *****************************************************************************/
#ifdef __STDC__
ELLNODE * epicsShareAPI ellGet (ELLLIST *pList)
#else
ELLNODE * epicsShareAPI ellGet (pList)
ELLLIST *pList;
#endif
{
  ELLNODE	*pnode = pList->node.next;

  if (pnode != NULL)
    ellDelete(pList, pnode);

  return(pnode);
}
/****************************************************************************
 *
 * This function inserts the specified node pNode after pPrev in the list
 * plist.  If pPrev is NULL, then pNode will be inserted at the head of the
 * list.
 *
 *****************************************************************************/
#ifdef __STDC__
void epicsShareAPI ellInsert (ELLLIST *plist, ELLNODE *pPrev, ELLNODE *pNode)
#else
void epicsShareAPI ellInsert (plist, pPrev, pNode)
ELLLIST *plist;
ELLNODE *pPrev;
ELLNODE *pNode;
#endif
{
  if (pPrev != NULL)
  {
    pNode->previous = pPrev;
    pNode->next = pPrev->next;
    pPrev->next = pNode;
  }
  else
  {
    pNode->previous = NULL;
    pNode->next = plist->node.next;
    plist->node.next = pNode;
  }
  if (pNode->next == NULL)
    plist->node.previous = pNode;
  else
    pNode->next->previous = pNode;

  plist->count++;

  return;
}
/****************************************************************************
 *
 * This function returns the last node in a list.  The node is NOT removed 
 * from the list.  If the list is empty, NULL will be returned.
 *
 *****************************************************************************/
#ifndef DLLLIB_USE_MACROS
#ifdef __STDC__
ELLNODE * epicsShareAPI ellLast (ELLLIST *pList)
#else
ELLNODE * epicsShareAPI ellLast (pList)
ELLLIST *pList;
#endif
{
  return(pList->node.previous);
}
#endif
/****************************************************************************
 *
 * This function returns the node following pNode.  The node is NOT remodev from
 * the list.  If pNode is the last element in the list, NULL will be returned.
 *
 *****************************************************************************/
#ifndef DLLLIB_USE_MACROS
#ifdef __STDC__
ELLNODE * epicsShareAPI ellNext (ELLNODE *pNode)
#else
ELLNODE * epicsShareAPI ellNext (pNode)
ELLNODE *pNode;
#endif
{
  return(pNode->next);
}
#endif
/****************************************************************************
 *
 * This function returns the nodeNum'th element in pList.  If there is no 
 * nodeNum'th node in the list, NULL will be returned.
 *
 *****************************************************************************/
#ifdef __STDC__
ELLNODE * epicsShareAPI ellNth (ELLLIST *pList, int nodeNum)
#else
ELLNODE * epicsShareAPI ellNth (pList, nodeNum)
ELLLIST *pList;
int nodeNum;
#endif
{
  ELLNODE *pnode;

  if ((nodeNum < 1)||(pList->count == 0))
    return(NULL);

  if (nodeNum > pList->count/2)
  {
    if (nodeNum > pList->count)
      return(NULL);

    pnode = pList->node.previous;
    nodeNum = pList->count - nodeNum;
    while(nodeNum)
    {
      pnode = pnode->previous;
      nodeNum--;
    }
    return(pnode);
  }

  pnode = pList->node.next;
  while(--nodeNum > 0)
    pnode = pnode->next;

  return(pnode);
}
/****************************************************************************
 *
 * This function returns the node that preceeds pNode.  If pNode is the first
 * element in the list, NULL will be returned.
 *
 *****************************************************************************/
#ifndef DLLLIB_USE_MACROS
#ifdef __STDC__
ELLNODE * epicsShareAPI ellPrevious (ELLNODE *pNode)
#else
ELLNODE * epicsShareAPI ellPrevious (pNode)
ELLNODE *pNode;
#endif
{
  return(pNode->previous);
}
#endif
/****************************************************************************
 *
 * This function returns the node, nStep nodes forward from pNode.  If there is
 * no node that many steps from pNode, NULL will be returned.
 *
 *****************************************************************************/
#ifdef __STDC__
ELLNODE * epicsShareAPI ellNStep (ELLNODE *pNode, int nStep)
#else
ELLNODE * epicsShareAPI ellNStep (pNode, nStep)
ELLNODE *pNode;
int nStep;
#endif
{
  if (nStep > 0)
  {
    while ((pNode != NULL) && nStep)
    {
      pNode = pNode->next;
      nStep--;
    }
  }
  else
  {
    while ((pNode != NULL) && nStep)
    {
      pNode = pNode->previous;
      nStep++;
    }
  }
  return(pNode);
}
/****************************************************************************
 *
 * This function returns the node number of pNode within pList.  If the node is
 * not in pList, -1 is returned.  Note that the first node is 1.
 *
 *****************************************************************************/
#ifdef __STDC__
int epicsShareAPI ellFind (ELLLIST *pList, ELLNODE *pNode)
#else
int epicsShareAPI ellFind (pList, pNode)
ELLLIST *pList;
ELLNODE *pNode;
#endif
{
  ELLNODE *got = pList->node.next;
  int	count = 1;

  while ((got != pNode) && (got != NULL))
  {
    got = got->next;
    count++;
  }
  if (got == NULL)
    return(-1);
    
  return(count);
}
/****************************************************************************
 *
 * This function frees the nodes in a list.  It makes the list into an empty
 * list, and free()'s all the nodes that are (were) in that list.
 *
 * NOTE: the nodes in the list are free()'d on the assumption that the node
 * structures were malloc()'d one-at-a-time and that the ELLNODE structure is
 * the first thing in the "rest of" the node structure.  
 * In other words, this is a pretty worthless function.
 *
 *****************************************************************************/
#ifdef __STDC__
void epicsShareAPI ellFree (ELLLIST *pList)
#else
void epicsShareAPI ellFree (pList)
ELLLIST *pList;
#endif
{
  ELLNODE *nnode = pList->node.next;
  ELLNODE *pnode;

  while (nnode != NULL)
  {
    pnode = nnode;
    nnode = nnode->next;
    free(pnode);
  }
  pList->node.next = NULL;
  pList->node.previous = NULL;
  pList->count = 0;

  return;
}
#ifdef DEBUG_DRIVER
/****************************************************************************
 *
 * This is a bunch of code that can be used to test the ellLib code.  It is
 * provided here so that you may use it if and when you decide to alter 
 * something in this file, you need not re-create it for testing purposes.
 *
 * In general, I believe that you will end up with a core dump if the tests
 * fail.  If all tests complete properly, there will be a:
 *            printf("All tests completed successful\n") 
 * to assure you of it.
 *
 *****************************************************************************/

#include <malloc.h>
#undef NULL
#include <stdio.h>

struct	myNode {
  ELLNODE	ellNode;
  int	list;
  int	num;
};
main()
{
  ELLLIST	list1;
  ELLLIST	list2;
  int	i1, i2, i3;
  struct myNode	*pmyNode, *pfirst, *pn1;

  printf("\nellInit() check...  ");
  list1.count = 27;
  list1.node.next = (ELLNODE *) 0x01010101;
  list1.node.previous = (ELLNODE *) 0x10101010;

  ellInit(&list1);
  ellInit(&list2);

  if ((list1.count != 0)||(list1.node.next != NULL)||(list1.node.previous != NULL))
  {
    printf("FAILED!!!");
    printf("list1.count %d, list1.node.next %08.8X, list1.node.previous %08.8X\n", list1.count, list1.node.next, list1.node.previous);
    exit(1);
  }
  printf("ok");


  /* First build a couple lists and fill them with nodes. */
  printf("\nellAdd() check...  ");
  i1 = 2;
  pmyNode = (struct myNode *) malloc(sizeof(struct myNode));
  pmyNode->ellNode.next = (ELLNODE *) 0x10101010;
  pmyNode->ellNode.previous = (ELLNODE *) 0x10101010;

  ellAdd(&list1, pmyNode);
  pmyNode->list = 1;
  pmyNode->num = 1;

  if ((list1.count != 1)||(list1.node.next != (ELLNODE *)pmyNode)||
      (list1.node.previous != (ELLNODE *)pmyNode)||(pmyNode->ellNode.next != NULL)||
      (pmyNode->ellNode.previous != NULL))
  {
    printf("FAILED!!!!!\n");
    exit(2);
  }
  pfirst = pmyNode;
  while(i1 <= 21)	/* put 21 nodes into LIST1 */
  {
    pmyNode = (struct myNode *) malloc(sizeof(struct myNode));
    ellAdd(&list1, pmyNode);
    pmyNode->list = 1;
    pmyNode->num = i1;
    i1++;
  }
  if ((list1.count != 21)||(list1.node.next != (ELLNODE *)pfirst)||
      (list1.node.previous != (ELLNODE *)pmyNode)||(pmyNode->ellNode.next != NULL))
  {
    printf("FAILED!!!\n");
    exit(3);
  }
  printf("ok\nellFirst() check...  ");

  if (ellFirst(&list1) != (ELLNODE *)pfirst)
  {
    printf("FAILED!!!\n");
    exit(5);
  }
  printf("ok\nellLast() check...  ");
  if (ellLast(&list1) != (ELLNODE *)pmyNode)
  {
    printf("FAILED!!!\n");
    exit(6);
  }
  printf("ok\nellNext() check...  ");
  if ((ellNext(pmyNode) != NULL)||(ellNext(pfirst) != (ELLNODE *)(pfirst->ellNode.next)))
  {
    printf("FAILED!!!\n");
    exit(7);
  }
  printf("ok\nellNth() check...  ");
  if ((ellNth(&list1, 0) != NULL)||
      ((pn1 = (struct myNode *)ellNth(&list1, 21)) != pmyNode)||
      (ellNth(&list1, 22) != NULL)||
      (ellNth(&list1, 1) != (ELLNODE *)(pfirst))||
      (ellNth(&list1, 2) != (ELLNODE *)(pfirst->ellNode.next))||
      (ellNth(&list1, 20) != (ELLNODE *)(pmyNode->ellNode.previous)))
  {
    printf("FAILED!!!\n");
    exit(8);
  }
  printf("ok\nellPrevious() check...  ");

  if ((ellPrevious(pmyNode) != (ELLNODE *)(pmyNode->ellNode.previous))||
     (ellPrevious(pfirst) != NULL)||
     (ellPrevious(pfirst->ellNode.next) != (ELLNODE *)pfirst))
  {
    printf("FAILED!!!\n");
    exit(9);
  }
  printf("ok\nellGet() check...  ");
  if (((pn1 = (struct myNode *)ellGet(&list1)) != pfirst)||
      (list1.node.next != pfirst->ellNode.next))
  {
    printf("FAILED!!!\n");
    exit(10);
  }
  ellAdd(&list2, pn1);
  if (((pn1 = (struct myNode *)ellGet(&list2)) != pfirst)||
      (list2.node.next != NULL)||(list2.node.previous != NULL))
  {
    printf("FAILED!!!\n");
    exit(11);
  }
  printf("ok\nellCount() check...  ");
  if ((ellCount(&list1) != 20)||(ellCount(&list2) != 0))
  {
    printf("FAILED!!!\n");
    exit(4);
  }
  printf("ok\nellConcat() check...  ");
  ellConcat(&list1, &list2);
  if ((ellCount(&list1) != 20)||(ellCount(&list2) != 0)||
      (list1.node.previous != (ELLNODE *)pmyNode))
  {
    printf("FAILED!!! (12)\n");
    exit(12);
  }
  ellAdd(&list2, pn1);
  ellConcat(&list1, &list2);
  if ((ellCount(&list1) != 21)||(ellCount(&list2) != 0)||
      (list1.node.previous != (ELLNODE *)pn1)||
      (list2.node.next != NULL)||(list2.node.previous!=NULL))
  {
    printf("FAILED!!! (13)\n");
    printf("list1.count = %d list2.count = %d\n", ellCount(&list1), ellCount(&list2));
    exit(13);
  }
  printf("ok\nellDelete() check...  ");
  ellDelete(&list1, pn1);
  if (ellCount(&list1) != 20)
  {
    printf("FAILED!!! (14)\n");
    exit(14);
  }
  printf("ok\nellFind() check...  ");
  ellAdd(&list2, pn1);
  ellConcat(&list2, &list1);
  i1 = -23;
  if (ellFind(&list1, pn1) != -1)	/* empty list */
  {
    printf("FAILED!!! (15)\n");
    exit(15);
  }
  if ((i1 = ellFind(&list2, pn1)) != pn1->num)	/* first node */
  {
    printf("FAILED!!! (16)\n");
    exit(16);
  }
  pn1 = (struct myNode *)ellNth(&list2, 18);
  if ((i1 = ellFind(&list2, pn1)) != 18)	/* 18th node */
  {
    printf("FAILED!!! (17)\n");
    exit(17);
  }
  printf("ok\nellInsert() check...  ");
  ellDelete(&list2, pn1);
  ellInsert(&list2, NULL, pn1);			/* move node 18 to top */
  if ((ellCount(&list2) != 21)||(((struct myNode *)(list2.node.next))->num != 18))
  {
    printf("FAILED!!! (18)\n");
    exit (18);
  }
  if (ellFind(&list2, ellNth(&list2, 21)) != 21)
  { /* Run thru all pointers to check integrity */
    printf("FAILED!!! (19)\n");
    exit(19);
  }
  pn1 = (struct myNode *)ellGet(&list2);
  pmyNode = (struct myNode *)ellNth(&list2, 17);
  ellInsert(&list2, pmyNode, pn1);
  if (ellFind(&list2, ellNth(&list2, 21)) != 21)
  { /* Run thru all pointers to check integrity */
    printf("FAILED!!! (20)\n");
    exit(20);
  }
  if ((((struct myNode *)(ellFirst(&list2)))->num != 1)||
      (((struct myNode *)(ellNth(&list2,17)))->num != 17)||
      (((struct myNode *)(ellNth(&list2,18)))->num != 18))
  {
    printf("FAILED!!! (21)\n");
    exit(21);
  }
  pn1 = (struct myNode *)ellLast(&list2);
  pmyNode = (struct myNode *) malloc(sizeof(struct myNode));
  ellInsert(&list2, pn1, pmyNode);
  if ((ellCount(&list2) != 22)||(ellFind(&list2, ellNth(&list2, 22)) != 22))
  {
    printf("FAILED!!! (33)\n");
    exit(33);
  }
  ellDelete(&list2, pmyNode);
  free(pmyNode);

  printf("ok\nellExtract() check...  ");
  ellExtract(&list2, ellNth(&list2,9), ellNth(&list2, 19), &list1);
  if ((ellCount(&list2) != 10)||(ellCount(&list1) != 11))
  {
    printf("FAILED!!! (22)\n");
    exit(22);
  }
  if ((ellFind(&list2, ellNth(&list2, 10)) != 10)||
      (ellFind(&list1, ellNth(&list1, 11)) != 11))
  {
    printf("FAILED!!! (23)\n");
    exit(23);
  }
  printf("ok\nellFree() check...  ");
  ellFree(&list2);
  if (ellCount(&list2) != 0)
  {
    printf("FAILED!!! (24)\n");
    exit(24);
  }
  printf("ok\nellNStep() check...  ");
  pn1 = (struct myNode *)ellFirst(&list1);
  i1 = 1;
  while(pn1 != NULL)
  {
    pn1->num = i1++;
    pn1 = (struct myNode *)ellNext(pn1);
  }
  pn1 = (struct myNode *)ellFirst(&list1);
  if (pn1 == NULL)
  {
    printf("FAILED!!! (30)\n");
    exit(30);
  }
  pmyNode = (struct myNode *)ellNStep(pn1, 3);
  if (pmyNode == NULL)
  {
    printf("FAILED!!! (27)\n");
    exit(27);
  }
  if (pmyNode->num != 4)
  {
    printf("FAILED!!! (25)\n");
    printf("got number %d\n", pmyNode->num);
    exit(25);

  }
  pmyNode = (struct myNode *)ellNStep(pn1, 30);
  if (pmyNode != NULL)
  {
    printf("FAILED!!! (26)\n");
    exit(26);
  }
  pmyNode = (struct myNode *)ellNStep(pn1, 10);
  if (pmyNode == NULL)
  {
    printf("FAILED!!! (28)\n");
    exit(28);
  }
  if (pmyNode->num != 11)
  {
    printf("FAILED!!! (29)\n");
    exit(29);
  }
  pmyNode = (struct myNode *)ellNStep(pmyNode, 0);
  if (pmyNode->num != 11)
  {
    printf("FAILED!!! (31)\n");
    exit(31);
  }
  pmyNode = (struct myNode *)ellNStep(pmyNode, -4);
  if (pmyNode->num != 7)
  {
    printf("FAILED!!! (32)\n");
    exit(32);
  }
  printf("ok\n\nAll tests completed successful\n");
}
#endif
