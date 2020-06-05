/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/**
 * \file ellLib.h
 * \author John Winans (ANL)
 * \author Andrew Johnson (ANL)
 *
 * \brief A doubly-linked list library
 *
 * This provides similar functionality to the VxWorks lstLib library.
 *
 * Supports the creation and maintenance of a doubly-linked list. The user
 * supplies a list descriptor (type \c ELLLIST) that will contain pointers to
 * the first and last nodes in the list, and a count of the number of
 * nodes in the list. The nodes in the list can be any user-defined structure,
 * but they must reserve space for two pointers as their first elements.
 * Both the forward and backward chains are terminated with a NULL pointer.
 */

#ifndef INC_ellLib_H
#define INC_ellLib_H

#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief List node type.
 *
 * A list node (an \c ELLNODE) must be embedded in the structure to be placed
 * on the list, ideally as the first member of that structure.
 *
 * If the node is elsewhere in the structure, care must be taken to convert
 * between the structure pointer and the node pointer and back every time when
 * calling routines in the ellLib API. The ellFree() and ellFree2() routines
 * cannot be used with such a list.
 */
typedef struct ELLNODE {
    struct ELLNODE *next;       /**< \brief Pointer to next node in list */
    struct ELLNODE *previous;   /**< \brief Pointer to previous node in list */
} ELLNODE;

/** \brief Value of a terminal node
 */
#define ELLNODE_INIT {NULL, NULL}

/** \brief List header type
 */
typedef struct ELLLIST {
    ELLNODE node;   /**< \brief Pointers to the first and last nodes on list */
    int     count;  /**< \brief Number of nodes on the list */
} ELLLIST;

/** \brief Value of an empty list
  */
#define ELLLIST_INIT {ELLNODE_INIT, 0}

/** \brief Pointer to free() for use by ellFree() macro.
 *
 * This is required for use on Windows, where each DLL has its own free()
 * function. The ellFree() macro passes the application's version of free()
 * to the ellFree2() function.
 */
typedef void (*FREEFUNC)(void *);

/** \brief Initialize a list type
  * \param PLIST Pointer to list header to be initialized
  */
#define ellInit(PLIST) {\
    (PLIST)->node.next = (PLIST)->node.previous = NULL;\
    (PLIST)->count = 0;\
}
/** \brief Report the number of nodes in a list
  * \param PLIST Pointer to list descriptor
  * \return Number of nodes in the list
  */
#define ellCount(PLIST)    ((PLIST)->count)
/** \brief Find the first node in list
  * \param PLIST Pointer to list descriptor
  * \return Pointer to first node in the list
  */
#define ellFirst(PLIST)    ((PLIST)->node.next)
/** \brief Find the last node in list
  * \param PLIST Pointer to list descriptor
  * \return Pointer to last node in the list
  */
#define ellLast(PLIST)     ((PLIST)->node.previous)
/** \brief Find the next node in list
  * \param PNODE Pointer to node whose successor is to be found
  * \return Pointer to the node following \c PNODE
  */
#define ellNext(PNODE)     ((PNODE)->next)
/** \brief Find the previous node in list
  * \param PNODE Pointer to node whose predecessor is to be found
  * \return Pointer to the node prior to \c PNODE
  */
#define ellPrevious(PNODE) ((PNODE)->previous)
/** \brief Free up the list
  * \param PLIST List for which to free all nodes
  */
#define ellFree(PLIST)     ellFree2(PLIST, free)

/**
 * \brief Adds a node to the end of a list
 * \param pList Pointer to list descriptor
 * \param pNode Pointer to node to be added
 */
LIBCOM_API void ellAdd (ELLLIST *pList, ELLNODE *pNode);
/**
 * \brief Concatenates a list to the end of another list.
 * The list to be added is left empty. Either list (or both)
 * can be empty at the beginning of the operation.
 * \param pDstList Destination list
 * \param pAddList List to be added to \c pDstList
 */
LIBCOM_API void ellConcat (ELLLIST *pDstList, ELLLIST *pAddList);
/**
 * \brief Deletes a node from a list.
 * \param pList Pointer to list descriptor
 * \param pNode Pointer to node to be deleted
 */
LIBCOM_API void ellDelete (ELLLIST *pList, ELLNODE *pNode);
/**
 * \brief Extract a sublist from a list.
 * \param pSrcList Pointer to source list
 * \param pStartNode First node in \c pSrcList to be extracted
 * \param pEndNode Last node in \c pSrcList to be extracted
 * \param pDstList Pointer to list where to put extracted list
 */
LIBCOM_API void ellExtract (ELLLIST *pSrcList, ELLNODE *pStartNode, ELLNODE *pEndNode, ELLLIST *pDstList);
/**
 * \brief Deletes and returns the first node from a list
 * \param pList Pointer to list from which to get node
 * \return Pointer to the first node from the list, or NULL if the list is empty
 */
LIBCOM_API ELLNODE * ellGet (ELLLIST *pList);
/**
 * \brief Deletes and returns the last node from a list.
 * \param pList Pointer to list from which to get node
 * \return Pointer to the last node from the list, or NULL if the list is empty
 */
LIBCOM_API ELLNODE * ellPop (ELLLIST *pList);
/**
 * \brief Inserts a node into a list immediately after a specific node
 * \param plist Pointer to list into which to insert node
 * \param pPrev Pointer to the node after which to insert
 * \param pNode Pointer to the node to be inserted
 * \note If \c pPrev is NULL \c pNode will be inserted at the head of the list
 */
LIBCOM_API void ellInsert (ELLLIST *plist, ELLNODE *pPrev, ELLNODE *pNode);
/**
 * \brief Find the Nth node in a list
 * \param pList Pointer to list from which to find node
 * \param nodeNum Index of the node to be found
 * \return Pointer to the element at index \c nodeNum in pList, or NULL if
 * there is no such node in the list.
 * \note The first node has index 1.
 */
LIBCOM_API ELLNODE * ellNth (ELLLIST *pList, int nodeNum);
/**
 * \brief Find the list node \c nStep steps away from a specified node
 * \param pNode The known node
 * \param nStep How many steps to take, may be negative to step backwards
 * \return Pointer to the node \c nStep nodes from \c pNode, or NULL if there
 * is no such node in the list.
 */
LIBCOM_API ELLNODE * ellNStep (ELLNODE *pNode, int nStep);
/**
 * \brief Find the index of a specific node in a list
 * \param pList Pointer to list to search
 * \param pNode Pointer to node to search for
 * \return The node's index, or -1 if it cannot be found on the list.
 * \note The first node has index 1.
 */
LIBCOM_API int  ellFind (ELLLIST *pList, ELLNODE *pNode);

typedef int (*pListCmp)(const ELLNODE* A, const ELLNODE* B);
/**
 * \brief Stable (MergeSort) of a given list.
 * \param pList Pointer to list to be sorted
 * \param pListCmp Compare function to be used
 * \note The comparison function cmp(A,B) is expected
 * to return -1 for A<B, 0 for A==B, and 1 for A>B.
 *
 * \note Use of mergesort algorithm based on analysis by
 * http://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
 */
LIBCOM_API void ellSortStable(ELLLIST *pList, pListCmp pListCmp);
/**
 * \brief Free all the nodes in a list.
 *
 * This routine empties a list, calling freeFunc() for every node on it.
 * \param pList List from which to free all nodes
 * \param freeFunc The free() routine to be called
 * \note The nodes in the list are free()'d on the assumption that the node
 * structures were malloc()'d one-at-a-time and that the ELLNODE structure is
 * the first member of the parent structure.
 */
LIBCOM_API void ellFree2 (ELLLIST *pList, FREEFUNC freeFunc);
/**
 * \brief Verifies that the list is consistent
 * \param pList List to be verified
 */
LIBCOM_API void ellVerify (ELLLIST *pList);

#ifdef __cplusplus
}
#endif

#endif /* INC_ellLib_H */
