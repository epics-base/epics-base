/*
 *      Author: Jeffrey O. Hill
 *              hill@atdiv.lanl.gov
 *              (505) 665 1831
 *      Date:  	9-93 
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
 *      Modification Log:
 *      -----------------
 * 	.01 091493 joh	fixed overzealous parameter check
 *	.02 121693 joh	added bucketFree()
 *
 * 	NOTES:
 * 	.01 Storage for identifier must persist until an item is deleted	
 */


#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <bucketLib.h>


#ifndef NBBY
#define NBBY 8
#endif /* NBBY */
#ifndef TRUE
#define TRUE 1
#endif /* TRUE */
#ifndef FALSE
#define FALSE 0
#endif /* FALSE */
#ifndef LOCAL
#define LOCAL static
#endif /* LOCAL */
#ifndef max
#define max(A,B) ((A)>(B)?(A):(B))
#endif /* max */

/*
 * these data type dependent routines are
 * provided in the bucketLib.c
 */
typedef BUCKETID bucketHash(BUCKET *pb, const void *pId);
typedef ITEM **bucketCompare(ITEM **ppi, const void *pId);

LOCAL bucketCompare   bucketUnsignedCompare;
LOCAL bucketCompare   bucketPointerCompare;
LOCAL bucketCompare   bucketStringCompare;
LOCAL bucketHash      bucketUnsignedHash;
LOCAL bucketHash      bucketPointerHash;
LOCAL bucketHash      bucketStringHash;

typedef struct {
        bucketHash      *pHash;
        bucketCompare   *pCompare;
	buckTypeOfId	type;
}bucketSET;

LOCAL bucketSET BSET[] = {
	{bucketUnsignedHash, bucketUnsignedCompare, bidtUnsigned},
	{bucketPointerHash, bucketPointerCompare, bidtPointer},
	{bucketStringHash, bucketStringCompare, bidtString}
};

LOCAL int bucketAddItem(BUCKET *prb, bucketSET *pBSET, const void *pId, void *pApp);
LOCAL int bucketRemoveItem (BUCKET *prb, bucketSET *pBSET, const void *pId);
LOCAL void *bucketLookupItem(BUCKET *pb, bucketSET *pBSET, const void *pId);



/*
 * bucket id bit width
 */
#define BUCKETID_BIT_WIDTH 	(sizeof(BUCKETID)*NBBY)

/*
 * Maximum bucket size
 */
#define BUCKET_MAX_WIDTH	12	

#ifdef DEBUG
#error This is out of date
main()
{
	BUCKETID	id1;
	BUCKETID	id2;
	char		*pValSave1;
	char		*pValSave2;
	int		s;
	BUCKET		*pb;
	char		*pVal;
	unsigned	i;
	clock_t		start, finish;
	double 		duration;
	const int	LOOPS = 500000;

	pb = bucketCreate(8);
	if(!pb){
		return BUCKET_FAILURE;
	}

	id1 = 0x1000a432;
	pValSave1 = "fred";
	s = bucketAddItemUnsignedId(pb, &id1, pValSave1);
	assert(s == BUCKET_SUCCESS);

	pValSave2 = "jane";
	s = bucketAddItemStringId(pb, pValSave2, pValSave2);
	assert(s == BUCKET_SUCCESS);

	start = clock();
	for(i=0; i<LOOPS; i++){
		pVal = bucketLookupItem(pb, id1);
		assert(pVal == pValSave1);
		pVal = bucketLookupItem(pb, id1);
		assert(pVal == pValSave1);
		pVal = bucketLookupItem(pb, id1);
		assert(pVal == pValSave1);
		pVal = bucketLookupItem(pb, id1);
		assert(pVal == pValSave1);
		pVal = bucketLookupItem(pb, id1);
		assert(pVal == pValSave1);
		pVal = bucketLookupItem(pb, id1);
		assert(pVal == pValSave1);
		pVal = bucketLookupItem(pb, id1);
		assert(pVal == pValSave1);
		pVal = bucketLookupItem(pb, id1);
		assert(pVal == pValSave1);
		pVal = bucketLookupItem(pb, id1);
		assert(pVal == pValSave1);
		pVal = bucketLookupItem(pb, id2);
		assert(pVal == pValSave2);
	}
	finish = clock();

	duration = finish-start;
	duration = duration/CLOCKS_PER_SEC;
	printf("It took %15.10f total sec\n", duration);
	duration = duration/LOOPS;
	duration = duration/10;
	duration = duration * 1e6;
	printf("It took %15.10f u sec per hash lookup\n", duration);

	bucketShow(pb);

	return BUCKET_SUCCESS;
}
#endif


/*
 * bucketUnsignedCompare()
 */
LOCAL ITEM **bucketUnsignedCompare (ITEM **ppi, const void *pId)
{
	unsigned	id;	
	unsigned	*pItemId;
	ITEM		*pi;

	id = * (unsigned *) pId;
	while (pi = *ppi) {
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
ITEM **bucketPointerCompare (ITEM **ppi, const void *pId)
{
	void		*ptr;	
	void		**pItemId;
	ITEM		*pi;

	ptr = * (void **) pId;
	while (pi = *ppi) {
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
ITEM **bucketStringCompare (ITEM **ppi, const void *pId)
{
	const char	*pStr = pId;	
	ITEM		*pi;
	int		status;

	while (pi = *ppi) {
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
BUCKETID bucketUnsignedHash (BUCKET *pb, const void *pId)
{
	const unsigned	*pUId = pId;	
	unsigned 	src;
	BUCKETID	hashid;

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
 *
 */
BUCKETID bucketPointerHash (BUCKET *pb, const void *pId)
{
	void * const	*ppId = pId;	
	unsigned long	src;
	BUCKETID	hashid;

	/*
	 * This makes the assumption that
	 * a pointer will fit inside of a long
	 * (this assumption may not port to all
	 * CPU architectures)
	 */
	src = (unsigned long) *ppId; 
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
BUCKETID bucketStringHash (BUCKET *pb, const void *pId)
{
	const char	*pStr = pId;	
	BUCKETID	hashid;
	unsigned	i;

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
BUCKET  *bucketCreate (unsigned nHashTableEntries)
{
	BUCKETID	mask;
	unsigned	nbits;
	BUCKET		*pb;

	if (nHashTableEntries==0) {
		return NULL;
	}

	/*
	 * count the number of bits in the bucket id
	 */
	for (nbits=0; nbits<BUCKETID_BIT_WIDTH; nbits++) {
		mask = (1<<nbits) - 1;
		if ( ((nHashTableEntries-1) & ~mask) == 0){
			break;
		}
	}
	/*
	 * indexWidth must be specified at least one
	 * bit less than the bit size of type BUCKETID
	 */
	if (nbits>=BUCKETID_BIT_WIDTH) {
		printf("%s at %d: Requested index width=%d to large. max=%d\n",
			__FILE__,
			__LINE__,
			nbits,
			BUCKETID_BIT_WIDTH-1);
		return NULL;
	}

	pb = (BUCKET *) calloc(1, sizeof(*pb));
	if (!pb) {
		return pb;
	}

	pb->hashIdMask = mask;
	pb->hashIdNBits = nbits;

	pb->pTable = (ITEM **) calloc (mask+1, sizeof(*pb->pTable));
	if(!pb->pTable){
		free(pb);
		return NULL;
	}
	return pb;
}


/*
 * bucketFree()
 */
#ifdef __STDC__
int	bucketFree(BUCKET *prb)
#else
int	bucketFree(prb)
BUCKET	*prb;
#endif
{
	ITEM *pi;
	ITEM *pni;

	/*
	 * deleting a bucket with entries in use
	 * will cause memory leaks and is not allowed
	 */
	assert(prb->nInUse==0);

	/*
	 * free the free list
	 */
	pi = prb->pFreeItems;
	while (pi) {
		pni = pi->pItem;
		free (pi);
		pi = pni;
	}
	free(prb->pTable);
	free(prb);

	return BUCKET_SUCCESS;
}


/*
 * bucketAddItem()
 */
int	bucketAddItemUnsignedId(BUCKET *prb, const unsigned *pId, void *pApp)
{
	return bucketAddItem(prb, &BSET[bidtUnsigned], pId, pApp);
}
int	bucketAddItemPointerId(BUCKET *prb, void * const *pId, void *pApp)
{
	return bucketAddItem(prb, &BSET[bidtPointer], pId, pApp);
}
int	bucketAddItemStringId(BUCKET *prb, const char *pId, void *pApp)
{
	return bucketAddItem(prb, &BSET[bidtString], pId, pApp);
}
LOCAL int bucketAddItem(BUCKET *prb, bucketSET *pBSET, const void *pId, void *pApp)
{
	BUCKETID	hashid;
	ITEM		*pi;

	/*
	 * try to get it off the free list first. If
	 * that fails then malloc()
	 */
	pi = prb->pFreeItems;
	if (pi) {
		prb->pFreeItems = pi->pItem;
	}
	else {
		pi = (ITEM *) malloc(sizeof(ITEM));
		if(!pi){
			return BUCKET_FAILURE;
		}
	}

	/*
	 * create the hash index 
	 */
	hashid = (*pBSET->pHash) (prb, pId);

	pi->pApp = pApp;
	pi->pId = pId;
	pi->type = pBSET->type;
	assert((hashid & ~prb->hashIdMask) == 0);
	pi->pItem = prb->pTable[hashid];
	prb->pTable[hashid] = pi;
	prb->nInUse++;

	return BUCKET_SUCCESS;
}


/*
 * bucketRemoveItem()
 */
int	bucketRemoveItemUnsignedId (BUCKET *prb, const unsigned *pId)
{
	return bucketRemoveItem(prb, &BSET[bidtUnsigned], pId);
}
int	bucketRemoveItemPointerId (BUCKET *prb, void * const *pId)
{
	return bucketRemoveItem(prb, &BSET[bidtPointer], pId);
}
int	bucketRemoveItemStringId (BUCKET *prb, const char *pId)
{
	return bucketRemoveItem(prb, &BSET[bidtString], pId);
}
LOCAL int bucketRemoveItem (BUCKET *prb, bucketSET *pBSET, const void *pId)
{
	BUCKETID	hashid;
	ITEM		**ppi;
	ITEM		*pi;

	/*
	 * create the hash index
	 */
	hashid = (*pBSET->pHash) (prb, pId);

	assert((hashid & ~prb->hashIdMask) == 0);
	ppi = &prb->pTable[hashid];
	ppi = (*pBSET->pCompare) (ppi, pId);
	if(!ppi){
		return BUCKET_FAILURE;
	}
	prb->nInUse--;
	pi = *ppi;
	*ppi = pi->pItem;

	/*
	 * stuff it on the free list
	 */
	pi->pItem = prb->pFreeItems;
	prb->pFreeItems = pi;

	return BUCKET_SUCCESS;
}



/*
 * bucketLookupItem()
 */
void 	*bucketLookupItemUnsignedId (BUCKET *prb, const unsigned *pId)
{
	return bucketLookupItem(prb, &BSET[bidtUnsigned], pId);
}
void	*bucketLookupItemPointerId (BUCKET *prb, void * const *pId)
{
	return bucketLookupItem(prb, &BSET[bidtPointer], pId);
}
void	*bucketLookupItemStringId (BUCKET *prb, const char *pId)
{
	return bucketLookupItem(prb, &BSET[bidtString], pId);
}
LOCAL void *bucketLookupItem(BUCKET *pb, bucketSET *pBSET, const void *pId)
{
	BUCKETID	hashid;
	ITEM		**ppi;

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
		return (*ppi)->pApp;
	}
	return NULL;
}



/*
 * bucketShow()
 */
#ifdef __STDC__
int	bucketShow(BUCKET *pb)
#else
int	bucketShow(pb)
BUCKET *pb;
#endif
{
	ITEM 		**ppi;
	ITEM 		*pi;
	ITEM 		*pni;
	unsigned	nElem;
	double		X;
	double		XX;
	double		mean;
	double		stdDev;
	unsigned	count;
	unsigned	maxEntries;
	unsigned	freeListCount;

	/*
	 * count bytes on the free list
	 */
	pi = pb->pFreeItems;
	freeListCount = 0;
	while (pi) {
		freeListCount++;
		pni = pi->pItem;
		pi = pni;
	}

	printf(	"Bucket entries in use = %d bytes in use = %d\n",
		pb->nInUse,
		sizeof(*pb)+(pb->hashIdMask+1)*
			sizeof(ITEM *)+pb->nInUse*sizeof(ITEM));

	printf(	"Free list bytes in use = %d\n",
		freeListCount*sizeof(ITEM));

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
		maxEntries = max (count, maxEntries);
		ppi++;
	}

	mean = X/nElem;
	stdDev = sqrt(XX/nElem - mean*mean);
	printf( "Bucket entries/hash id - mean = %f std dev = %f max = %d\n",
		mean,
		stdDev,
		maxEntries);

	return BUCKET_SUCCESS;
}


