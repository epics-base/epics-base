/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author: Jeffrey O. Hill
 *              hill@atdiv.lanl.gov
 *              (505) 665 1831
 *      Date:   9-93
 *
 *  NOTES:
 *  .01 Storage for identifier must persist until an item is deleted
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include "epicsAssert.h"
#include "freeList.h"   /* bucketLib uses freeListLib inside the DLL */
#include "bucketLib.h"

/*
 * these data type dependent routines are
 * provided in the bucketLib.c
 */
typedef BUCKETID bucketHash(BUCKET *pb, const void *pId);
typedef ITEM **bucketCompare(ITEM **ppi, const void *pId);

static bucketCompare   bucketUnsignedCompare;
static bucketCompare   bucketPointerCompare;
static bucketCompare   bucketStringCompare;
static bucketHash      bucketUnsignedHash;
static bucketHash      bucketPointerHash;
static bucketHash      bucketStringHash;

typedef struct {
    bucketHash      *pHash;
    bucketCompare   *pCompare;
    buckTypeOfId    type;
}bucketSET;

static bucketSET BSET[] = {
    {bucketUnsignedHash, bucketUnsignedCompare, bidtUnsigned},
    {bucketPointerHash, bucketPointerCompare, bidtPointer},
    {bucketStringHash, bucketStringCompare, bidtString}
};

static int bucketAddItem(BUCKET *prb, bucketSET *pBSET,
            const void *pId, const void *pApp);
static void *bucketLookupItem(BUCKET *pb, bucketSET *pBSET, const void *pId);



/*
 * bucket id bit width
 */
#define BUCKETID_BIT_WIDTH  (sizeof(BUCKETID)*CHAR_BIT)

/*
 * Maximum bucket size
 */
#define BUCKET_MAX_WIDTH    12


/*
 * bucketUnsignedCompare()
 */
static ITEM **bucketUnsignedCompare (ITEM **ppi, const void *pId)
{
    unsigned    id;
    unsigned    *pItemId;
    ITEM        *pi;

    id = * (unsigned *) pId;
    while ( (pi = *ppi) ) {
        if (bidtUnsigned == pi->type) {
            pItemId = (unsigned *) pi->pId;
            if (id == *pItemId) {
                return ppi;
            }
        }
        ppi = &pi->pItem;
    }
    return NULL;
}


/*
 * bucketPointerCompare()
 */
static ITEM **bucketPointerCompare (ITEM **ppi, const void *pId)
{
    void        *ptr;
    void        **pItemId;
    ITEM        *pi;

    ptr = * (void **) pId;
    while ( (pi = *ppi) ) {
        if (bidtPointer == pi->type ) {
            pItemId = (void **) pi->pId;
            if (ptr == *pItemId) {
                return ppi;
            }
        }
        ppi = &pi->pItem;
    }
    return NULL;
}


/*
 * bucketStringCompare ()
 */
static ITEM **bucketStringCompare (ITEM **ppi, const void *pId)
{
    const char  *pStr = pId;
    ITEM        *pi;
    int         status;

    while ( (pi = *ppi) ) {
        if (bidtString == pi->type) {
            status = strcmp (pStr, (char *)pi->pId);
            if (status == '\0') {
                return ppi;
            }
        }
        ppi = &pi->pItem;
    }
    return NULL;
}


/*
 * bucketUnsignedHash ()
 */
static BUCKETID bucketUnsignedHash (BUCKET *pb, const void *pId)
{
    const unsigned  *pUId = (const unsigned *) pId;
    unsigned    src;
    BUCKETID    hashid;

    src = *pUId;
    hashid = src;
    src = src >> pb->hashIdNBits;
    while (src) {
        hashid = hashid ^ src;
        src = src >> pb->hashIdNBits;
    }
    hashid = hashid & pb->hashIdMask;

    return hashid;
}


/*
 * bucketPointerHash ()
 */
static BUCKETID bucketPointerHash (BUCKET *pb, const void *pId)
{
    void * const    *ppId = (void * const *) pId;
    size_t      src;
    BUCKETID    hashid;

    /*
     * This makes the assumption that size_t
     * can be used to hold a pointer value
     * (this assumption may not port to all
     * CPU architectures)
     */
    src = (size_t) *ppId;
    hashid = src;
    src = src >> pb->hashIdNBits;
    while(src){
        hashid = hashid ^ src;
        src = src >> pb->hashIdNBits;
    }
    hashid = hashid & pb->hashIdMask;

    return hashid;
}


/*
 * bucketStringHash ()
 */
static BUCKETID bucketStringHash (BUCKET *pb, const void *pId)
{
    const char  *pStr = (const char *) pId;
    BUCKETID    hashid;
    unsigned    i;

    hashid = 0;
    i = 1;
    while(*pStr){
        hashid += *pStr * i;
        pStr++;
        i++;
    }

    hashid = hashid % (pb->hashIdMask+1);

    return hashid;
}



/*
 * bucketCreate()
 */
LIBCOM_API BUCKET * epicsStdCall bucketCreate (unsigned nHashTableEntries)
{
    BUCKETID    mask;
    unsigned    nbits;
    BUCKET      *pb;

    /*
     * no absurd sized buckets
     */
    if (nHashTableEntries<=1) {
        fprintf (stderr, "Tiny bucket create failed\n");
        return NULL;
    }

    /*
     * count the number of bits in the bucket id
     */
    if ( BUCKETID_BIT_WIDTH > 0 ) {
        for (nbits=0; nbits<BUCKETID_BIT_WIDTH; nbits++) {
            mask = (1<<nbits) - 1;
            if ( ((nHashTableEntries-1) & ~mask) == 0){
                break;
            }
        }
    }
    else {
        mask = 0;
        nbits = 0;
    }

    /*
     * indexWidth must be specified at least one
     * bit less than the bit size of type BUCKETID
     */
    if (nbits>=BUCKETID_BIT_WIDTH) {
        fprintf (
            stderr,
        "%s at %d: Requested index width=%d to large. max=%ld\n",
            __FILE__,
            __LINE__,
            nbits,
            (long)(BUCKETID_BIT_WIDTH-1));
        return NULL;
    }

    pb = (BUCKET *) calloc(1, sizeof(*pb));
    if (!pb) {
        return pb;
    }

    pb->hashIdMask = mask;
    pb->hashIdNBits = nbits;
    freeListInitPvt(&pb->freeListPVT, sizeof(ITEM), 1024);

    pb->pTable = (ITEM **) calloc (mask+1, sizeof(*pb->pTable));
    if (!pb->pTable) {
        freeListCleanup(pb->freeListPVT);
        free (pb);
        return NULL;
    }
    return pb;
}


/*
 * bucketFree()
 */
LIBCOM_API int epicsStdCall bucketFree (BUCKET *prb)
{
    /*
     * deleting a bucket with entries in use
     * will cause memory leaks and is not allowed
     */
    assert (prb->nInUse==0);

    /*
     * free the free list
     */
    freeListCleanup(prb->freeListPVT);
    free (prb->pTable);
    free (prb);

    return S_bucket_success;
}


/*
 * bucketAddItem()
 */
LIBCOM_API int epicsStdCall 
    bucketAddItemUnsignedId(BUCKET *prb, const unsigned *pId, const void *pApp)
{
    return bucketAddItem(prb, &BSET[bidtUnsigned], pId, pApp);
}
LIBCOM_API int epicsStdCall 
    bucketAddItemPointerId(BUCKET *prb, void * const *pId, const void *pApp)
{
    return bucketAddItem(prb, &BSET[bidtPointer], pId, pApp);
}
LIBCOM_API int epicsStdCall 
    bucketAddItemStringId(BUCKET *prb, const char *pId, const void *pApp)
{
    return bucketAddItem(prb, &BSET[bidtString], pId, pApp);
}
static int bucketAddItem(BUCKET *prb, bucketSET *pBSET, const void *pId, const void *pApp)
{
    BUCKETID    hashid;
    ITEM        **ppi;
    ITEM        **ppiExists;
    ITEM        *pi;

    /*
     * try to get it off the free list first. If
     * that fails then malloc()
     */
    pi = (ITEM *) freeListMalloc(prb->freeListPVT);
    if (!pi) {
        return S_bucket_noMemory;
    }

    /*
     * create the hash index
     */
    hashid = (*pBSET->pHash) (prb, pId);

    pi->pApp = pApp;
    pi->pId = pId;
    pi->type = pBSET->type;
    assert ((hashid & ~prb->hashIdMask) == 0);
    ppi = &prb->pTable[hashid];
    /*
     * Don't reuse a resource id !
     */
    ppiExists = (*pBSET->pCompare) (ppi, pId);
    if (ppiExists) {
        freeListFree(prb->freeListPVT,pi);
        return S_bucket_idInUse;
    }
    pi->pItem = *ppi;
    prb->pTable[hashid] = pi;
    prb->nInUse++;

    return S_bucket_success;
}

/*
 * bucketLookupAndRemoveItem ()
 */
static void *bucketLookupAndRemoveItem (BUCKET *prb, bucketSET *pBSET, const void *pId)
{
    BUCKETID    hashid;
    ITEM        **ppi;
    ITEM        *pi;
    void        *pApp;

    /*
     * create the hash index
     */
    hashid = (*pBSET->pHash) (prb, pId);

    assert((hashid & ~prb->hashIdMask) == 0);
    ppi = &prb->pTable[hashid];
    ppi = (*pBSET->pCompare) (ppi, pId);
    if(!ppi){
        return NULL;
    }
    prb->nInUse--;
    pi = *ppi;
    *ppi = pi->pItem;

    pApp = (void *) pi->pApp;

    /*
     * stuff it on the free list
     */
    freeListFree(prb->freeListPVT, pi);

    return pApp;
}
LIBCOM_API void * epicsStdCall bucketLookupAndRemoveItemUnsignedId (BUCKET *prb, const unsigned *pId)
{
    return bucketLookupAndRemoveItem(prb, &BSET[bidtUnsigned], pId);
}
LIBCOM_API void * epicsStdCall bucketLookupAndRemoveItemPointerId (BUCKET *prb, void * const *pId)
{
    return bucketLookupAndRemoveItem(prb, &BSET[bidtPointer], pId);
}
LIBCOM_API void * epicsStdCall bucketLookupAndRemoveItemStringId (BUCKET *prb, const char *pId)
{
    return bucketLookupAndRemoveItem(prb, &BSET[bidtString], pId);
}


/*
 * bucketRemoveItem()
 */
LIBCOM_API int epicsStdCall 
    bucketRemoveItemUnsignedId (BUCKET *prb, const unsigned *pId)
{
    return bucketLookupAndRemoveItem(prb, &BSET[bidtUnsigned], pId)?S_bucket_success:S_bucket_uknId;
}
LIBCOM_API int epicsStdCall 
    bucketRemoveItemPointerId (BUCKET *prb, void * const *pId)
{
    return bucketLookupAndRemoveItem(prb, &BSET[bidtPointer], pId)?S_bucket_success:S_bucket_uknId;
}
LIBCOM_API int epicsStdCall 
    bucketRemoveItemStringId (BUCKET *prb, const char *pId)
{
    return bucketLookupAndRemoveItem(prb, &BSET[bidtString], pId)?S_bucket_success:S_bucket_uknId;
}


/*
 * bucketLookupItem()
 */
LIBCOM_API void * epicsStdCall
    bucketLookupItemUnsignedId (BUCKET *prb, const unsigned *pId)
{
    return bucketLookupItem(prb, &BSET[bidtUnsigned], pId);
}
LIBCOM_API void * epicsStdCall
    bucketLookupItemPointerId (BUCKET *prb, void * const *pId)
{
    return bucketLookupItem(prb, &BSET[bidtPointer], pId);
}
LIBCOM_API void * epicsStdCall
    bucketLookupItemStringId (BUCKET *prb, const char *pId)
{
    return bucketLookupItem(prb, &BSET[bidtString], pId);
}
static void *bucketLookupItem (BUCKET *pb, bucketSET *pBSET, const void *pId)
{
    BUCKETID    hashid;
    ITEM        **ppi;

    /*
     * create the hash index
     */
    hashid = (*pBSET->pHash) (pb, pId);
    assert((hashid & ~pb->hashIdMask) == 0);

    /*
     * at the bottom level just
     * linear search for it.
     */
    ppi = (*pBSET->pCompare) (&pb->pTable[hashid], pId);
    if(ppi){
        return (void *) (*ppi)->pApp;
    }
    return NULL;
}



/*
 * bucketShow()
 */
LIBCOM_API int epicsStdCall bucketShow(BUCKET *pb)
{
    ITEM        **ppi;
    ITEM        *pi;
    unsigned    nElem;
    double      X;
    double      XX;
    double      mean;
    double      stdDev;
    unsigned    count;
    unsigned    maxEntries;

    printf( "    Bucket entries in use = %d bytes in use = %ld\n",
        pb->nInUse,
        (long) (sizeof(*pb)+(pb->hashIdMask+1)*
            sizeof(ITEM *)+pb->nInUse*sizeof(ITEM)));

    ppi = pb->pTable;
    nElem = pb->hashIdMask+1;
    X = 0.0;
    XX = 0.0;
    maxEntries = 0;
    while (ppi < &pb->pTable[nElem]) {
        pi = *ppi;
        count = 0;
        while (pi) {
            count++;
            pi = pi->pItem;
        }
        X += count;
        XX += count*count;
        if (count > maxEntries) maxEntries = count;
        ppi++;
    }

    mean = X/nElem;
    stdDev = sqrt(XX/nElem - mean*mean);
    printf( "    Bucket entries/hash id - mean = %f std dev = %f max = %d\n",
        mean,
        stdDev,
        maxEntries);

    return S_bucket_success;
}


