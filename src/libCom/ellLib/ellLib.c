/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author: John Winans (ANL)
 *      Date:   07-02-92
 */

#include <stdlib.h>

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "ellLib.h"

/****************************************************************************
 *
 * This function adds a node to the end of a linked list.
 *
 *****************************************************************************/
void ellAdd (ELLLIST *pList, ELLNODE *pNode)
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
void ellConcat (ELLLIST *pDstList, ELLLIST *pAddList)
{
    if (pAddList->count == 0)
        return;	/* Add list is empty, nothing to add. */

    if (pDstList->count == 0) {
        /* Destination list is empty... just transfer the add list over. */
        pDstList->node.next = pAddList->node.next;
        pDstList->node.previous = pAddList->node.previous;
        pDstList->count = pAddList->count;
    } else {
        /* Destination list not empty... append the add list. */
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
 * This function deletes a specific node from a specified list;
 *
 *****************************************************************************/
void ellDelete (ELLLIST *pList, ELLNODE *pNode)
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
void ellExtract (ELLLIST *pSrcList, ELLNODE *pStartNode, ELLNODE *pEndNode, ELLLIST *pDstList)
{
    ELLNODE *pnode;
    int count;

    /* Cut the list out of the source list (update count later) */
    if (pStartNode->previous != NULL)
        pStartNode->previous->next = pEndNode->next;
    else
        pSrcList->node.next = pEndNode->next;

    if (pEndNode->next != NULL) {
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
 * removed from the list.  If the list is empty, NULL will be returned.
 *
 *****************************************************************************/
ELLNODE * ellGet (ELLLIST *pList)
{
    ELLNODE *pnode = pList->node.next;

    if (pnode != NULL)
        ellDelete(pList, pnode);

    return pnode;
}
/****************************************************************************
*
* This function returns the last node in the specified list.  The node is
* removed from the list.  If the list is empty, NULL will be returned.
*
*****************************************************************************/
ELLNODE * ellPop (ELLLIST *pList)
{
   ELLNODE *pnode = pList->node.previous;

   if (pnode != NULL)
       ellDelete(pList, pnode);

   return pnode;
}
/****************************************************************************
 *
 * This function inserts the specified node pNode after pPrev in the list
 * plist.  If pPrev is NULL, then pNode will be inserted at the head of the
 * list.
 *
 *****************************************************************************/
void ellInsert (ELLLIST *plist, ELLNODE *pPrev, ELLNODE *pNode)
{
    if (pPrev != NULL) {
        pNode->previous = pPrev;
        pNode->next = pPrev->next;
        pPrev->next = pNode;
    } else {
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
 * This function returns the nodeNum'th element in pList.  If there is no
 * nodeNum'th node in the list, NULL will be returned.
 *
 *****************************************************************************/
ELLNODE * ellNth (ELLLIST *pList, int nodeNum)
{
    ELLNODE *pnode;

    if ((nodeNum < 1) || (pList->count == 0))
        return NULL;

    if (nodeNum > pList->count/2) {
        if (nodeNum > pList->count)
            return NULL;

        pnode = pList->node.previous;
        nodeNum = pList->count - nodeNum;
        while(nodeNum) {
            pnode = pnode->previous;
            nodeNum--;
        }
        return pnode;
    }

    pnode = pList->node.next;
    while (--nodeNum > 0)
        pnode = pnode->next;

    return pnode;
}
/****************************************************************************
 *
 * This function returns the node, nStep nodes forward from pNode.  If there is
 * no node that many steps from pNode, NULL will be returned.
 *
 *****************************************************************************/
ELLNODE * ellNStep (ELLNODE *pNode, int nStep)
{
    if (nStep > 0) {
        while ((pNode != NULL) && nStep) {
            pNode = pNode->next;
            nStep--;
        }
    } else {
        while ((pNode != NULL) && nStep) {
            pNode = pNode->previous;
            nStep++;
        }
    }
    return pNode;
}
/****************************************************************************
 *
 * This function returns the node number of pNode within pList.  If the node is
 * not in pList, -1 is returned.  Note that the first node is 1.
 *
 *****************************************************************************/
int ellFind (ELLLIST *pList, ELLNODE *pNode)
{
    ELLNODE *got = pList->node.next;
    int count = 1;

    while ((got != pNode) && (got != NULL)) {
        got = got->next;
        count++;
    }
    if (got == NULL)
        return -1;

    return count;
}
/****************************************************************************
 *
 * This function frees the nodes in a list.  It makes the list into an empty
 * list, and calls freeFunc() for all the nodes that are (were) in that list.
 *
 * NOTE: the nodes in the list are free()'d on the assumption that the node
 * structures were malloc()'d one-at-a-time and that the ELLNODE structure is
 * the first member of the parent structure.
 *
 *****************************************************************************/
void ellFree2 (ELLLIST *pList, FREEFUNC freeFunc)
{
    ELLNODE *nnode = pList->node.next;
    ELLNODE *pnode;

    while (nnode != NULL)
    {
        pnode = nnode;
        nnode = nnode->next;
        freeFunc(pnode);
    }
    pList->node.next = NULL;
    pList->node.previous = NULL;
    pList->count = 0;
}

/****************************************************************************
 *
 * This function verifies that the list is consistent.
 * joh 12-12-97
 *
 *****************************************************************************/
void ellVerify (ELLLIST *pList)
{
    ELLNODE *pNode;
    ELLNODE *pNext;
    int count = 0;

    assert (pList);

    pNode = ellFirst(pList);
    if (pNode) {
        assert (ellPrevious(pNode) == NULL);
        while (1) {
            count++;
            pNext = ellNext(pNode);
            if (pNext) {
                assert (ellPrevious(pNext) == pNode);
            } else {
                break;
            }
            pNode = pNext;
        }
        assert (ellNext(pNode) == NULL);
    }

    assert (pNode == ellLast(pList));
    assert (count == ellCount(pList));
}
