/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/**
 * \file bucketLib.h
 * \author Jeffrey O. Hill
 * \brief A hash table. Do not use for new code.
 *
 * \details
 * A hash table for which keys may be unsigned integers, pointers, or
 * strings. This API is used by the IOC's Channel Access Server, but
 * it should not be used by other code.
 *
 * \note Storage for identifiers must persist until an item is deleted
 */

#ifndef INCbucketLibh
#define INCbucketLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "errMdef.h"
#include "epicsTypes.h"
#include "libComAPI.h"

/** \brief Internal: bucket identifier */
typedef unsigned        BUCKETID;

/** \brief Internal: bucket key type */
typedef enum {bidtUnsigned, bidtPointer, bidtString} buckTypeOfId;

/** \brief Internal: bucket item structure */
typedef struct item{
    struct item     *pItem;
    const void      *pId;
    const void      *pApp;
    buckTypeOfId    type;
}ITEM;

/** \brief Internal: Hash table structure */
typedef struct bucket{
    ITEM            **pTable;
    void            *freeListPVT;
    unsigned        hashIdMask;
    unsigned        hashIdNBits;
    unsigned        nInUse;
}BUCKET;
/**
 * \brief Creates a new hash table
 * \param nHashTableEntries Table size
 * \return Pointer to the newly created hash table, or NULL.
 */
LIBCOM_API BUCKET * epicsStdCall bucketCreate (unsigned nHashTableEntries);
/**
 * \brief Release memory used by a hash table
 * \param *prb Pointer to the hash table
 * \return S_bucket_success
 * \note All items must be deleted from the hash table before calling this.
 */
LIBCOM_API int epicsStdCall bucketFree (BUCKET *prb);
/**
 * \brief Display information about a hash table
 * \param *prb Pointer to the hash table
 * \return S_bucket_success
 */
LIBCOM_API int epicsStdCall bucketShow (BUCKET *prb);

/**
 * \brief Add an item identified by an unsigned int to the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \param *pApp Pointer to the payload
 * \return Status value
 */
LIBCOM_API int epicsStdCall bucketAddItemUnsignedId (BUCKET *prb,
        const unsigned *pId, const void *pApp);
/**
 * \brief Add an item identified by a pointer to the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \param *pApp Pointer to the payload
 * \return Status value
 */
LIBCOM_API int epicsStdCall bucketAddItemPointerId (BUCKET *prb,
        void * const *pId, const void *pApp);
/**
 * \brief Add an item identified by a string to the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \param *pApp Pointer to the payload
 * \return Status value
 */
LIBCOM_API int epicsStdCall bucketAddItemStringId (BUCKET *prb,
        const char *pId, const void *pApp);
/**
 * \brief Remove an item identified by a string from the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \return Status value
 */
LIBCOM_API int epicsStdCall bucketRemoveItemUnsignedId (BUCKET *prb, const unsigned *pId);
/**
 * \brief Remove an item identified by a pointer from the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \return Status value
 */
LIBCOM_API int epicsStdCall bucketRemoveItemPointerId (BUCKET *prb, void * const *pId);
/**
 * \brief Remove an item identified by a string from the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \return Status value
 */
LIBCOM_API int epicsStdCall bucketRemoveItemStringId (BUCKET *prb, const char *pId);
/**
 * \brief Find an item identified by an unsigned int in the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \return Item's payload pointer, or NULL if not found
 */
LIBCOM_API void * epicsStdCall bucketLookupItemUnsignedId (BUCKET *prb, const unsigned *pId);
/**
 * \brief Find an item identified by a pointer in the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \return Item's payload pointer, or NULL if not found
 */
LIBCOM_API void * epicsStdCall bucketLookupItemPointerId (BUCKET *prb, void * const *pId);
/**
 * \brief Find an item identified by a string in the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \return Item's payload pointer, or NULL if not found
 */
LIBCOM_API void * epicsStdCall bucketLookupItemStringId (BUCKET *prb, const char *pId);

/**
 * \brief Find and delete an item identified by an unsigned int from the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \return Item's payload pointer, or NULL if not found
 */
LIBCOM_API void * epicsStdCall bucketLookupAndRemoveItemUnsignedId (BUCKET *prb, const unsigned *pId);
/**
 * \brief Find and delete an item identified by a pointer from the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \return Item's payload pointer, or NULL if not found
 */
LIBCOM_API void * epicsStdCall bucketLookupAndRemoveItemPointerId (BUCKET *prb, void * const *pId);
/**
 * \brief Find and delete an item identified by a string from the table
 * \param *prb Pointer to the hash table
 * \param *pId Pointer to the identifier
 * \return Item's payload pointer, or NULL if not found
 */
LIBCOM_API void * epicsStdCall bucketLookupAndRemoveItemStringId (BUCKET *prb, const char *pId);


 /**
 * \name Status values returned by some bucketLib functions
 * @{
 */
/**
 * \brief A synonym for S_bucket_success
 */
#define BUCKET_SUCCESS      S_bucket_success
/**
 * \brief Success, must be 0.
 */
#define S_bucket_success    0
/**
 * \brief Memory allocation failed
 */
#define S_bucket_noMemory   (M_bucket | 1)  /*Memory allocation failed*/
/**
 * \brief Identifier already in use
 */
#define S_bucket_idInUse    (M_bucket | 2)  /*Identifier already in use*/
/**
 * \brief Unknown identifier
 */
#define S_bucket_uknId      (M_bucket | 3)  /*Unknown identifier*/
/** @} */

#ifdef __cplusplus
}
#endif

#endif /*INCbucketLibh*/
