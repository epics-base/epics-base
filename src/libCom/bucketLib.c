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
 */


#ifdef vxWorks
#include <stdioLib.h>
#else /* vxWorks */
#include <stdio.h>
#endif /* vxWorks */

#include <assert.h>
#include <stdlib.h>

#include <bucketLib.h>

#ifndef NULL
#define NULL 0
#endif /* NULL */
#ifndef NBBY
#define NBBY 8
#endif /* NBBY */
#ifndef TRUE
#define TRUE 1
#endif /* TRUE */
#ifndef FALSE
#define FALSE 0
#endif /* FALSE */

#define BUCKET_IX_WIDTH		12
#define BUCKET_IX_N		(1<<BUCKET_IX_WIDTH)
#define BUCKET_IX_MASK		(BUCKET_IX_N-1)	

#ifdef DEBUG
main()
{
	BUCKETID	id;
	int		s;
	BUCKET		*pb;
	char		*pValSave;
	char		*pVal;
	unsigned	i;

	pb = bucketCreate(NBBY*sizeof(BUCKETID));
	if(!pb){
		return BUCKET_FAILURE;
	}

	id = 444;
	pValSave = "fred";
	s = bucketAddItem(pb, id, pValSave);
	if(s != BUCKET_SUCCESS){
		return BUCKET_FAILURE;
	}

	printf("Begin\n");
	for(i=0; i<500000; i++){
		pVal = bucketLookupItem(pb, id);
		if(pVal != pValSave){
			printf("failure\n");
			break;
		}
		pVal = bucketLookupItem(pb, id);
		if(pVal != pValSave){
			printf("failure\n");
			break;
		}
		pVal = bucketLookupItem(pb, id);
		if(pVal != pValSave){
			printf("failure\n");
			break;
		}
		pVal = bucketLookupItem(pb, id);
		if(pVal != pValSave){
			printf("failure\n");
			break;
		}
		pVal = bucketLookupItem(pb, id);
		if(pVal != pValSave){
			printf("failure\n");
			break;
		}
		pVal = bucketLookupItem(pb, id);
		if(pVal != pValSave){
			printf("failure\n");
			break;
		}
		pVal = bucketLookupItem(pb, id);
		if(pVal != pValSave){
			printf("failure\n");
			break;
		}
		pVal = bucketLookupItem(pb, id);
		if(pVal != pValSave){
			printf("failure\n");
			break;
		}
		pVal = bucketLookupItem(pb, id);
		if(pVal != pValSave){
			printf("failure\n");
			break;
		}
		pVal = bucketLookupItem(pb, id);
		if(pVal != pValSave){
			printf("failure\n");
			break;
		}
	}
	printf("End\n");

	return BUCKET_SUCCESS;
}
#endif


/*
 * bucketCreate()
 */
#ifdef __STDC__
BUCKET	*bucketCreate(unsigned indexWidth)
#else 
BUCKET	*bucketCreate(indexWidth)
unsigned indexWidth;
#endif
{
	BUCKET		*pb;

	/*
	 * indexWidth must be specified at least one
	 * bit less than the bit size of type BUCKETID
	 */
	if(indexWidth>sizeof(BUCKETID)*NBBY){
		printf("%s at %d: Requested index width=%d is to large. max=%d\n" ,
			__FILE__,
			__LINE__,
			indexWidth,
			sizeof(BUCKETID)*NBBY-1);
		return NULL;
	}

	pb = (BUCKET *) calloc(1, sizeof(*pb));
	if(!pb){
		return pb;
	}

	if(indexWidth>BUCKET_IX_WIDTH){
		pb->indexShift = indexWidth - BUCKET_IX_WIDTH;
	}
	else{
		pb->indexShift = 0;
	}
	pb->nextIndexMask = (1<<pb->indexShift)-1;
	pb->nEntries = 1<<(indexWidth-pb->indexShift);
	if(indexWidth == sizeof(BUCKETID)*NBBY){
		pb->indexMask = 0; 
		pb->indexMask = ~pb->indexMask;
	}
	else{
		pb->indexMask = (1<<indexWidth)-1; 
	}

	pb->pTable = (ITEMPTR *) calloc(
			pb->nEntries, 
			sizeof(ITEMPTR));
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
	/*
	 * deleting a bucket with entries in use
	 * will cause memory leaks and is not allowed
	 */
	if(prb->nInUse){
		return BUCKET_FAILURE;
	}

	free(prb->pTable);
	free(prb);

	return BUCKET_SUCCESS;
}


/*
 * bucketAddItem()
 */
#ifdef __STDC__
int	bucketAddItem(BUCKET *prb, BUCKETID id, void *pItem)
#else
int	bucketAddItem(prb, id, pItem)
BUCKET  *prb;
BUCKETID id;
void *pItem;
#endif
{
	int	s;
	ITEMPTR	*pi;

	/*
	 * is the id to big ?
	 */
	if(id&~prb->indexMask){
		return BUCKET_FAILURE;
	}

	if(prb->indexShift){
		int	new;
		BUCKET	*pb;

		pi = &prb->pTable[id>>prb->indexShift];
		pb = pi->pBucket;

		if(!pb){
			pb = bucketCreate(prb->indexShift);
			if(!pb){
				return BUCKET_FAILURE;
			}
			pi->pBucket = pb;
			prb->nInUse++;
			new = TRUE;
		}	
		else{
			new = FALSE;
		}

		s = bucketAddItem(
			pb, 
			id&prb->nextIndexMask, 
			pItem);
		/*
		 * if memory cant be allocated at a lower
		 * level dont leak everything allocated for this new
		 * item so far
		 */
		if(s != BUCKET_SUCCESS && new){
			s = bucketFree(pb);
			assert(s == BUCKET_SUCCESS);
			pi->pBucket = NULL;
			prb->nInUse--;
		}
		return s;
	}
	
	pi = &prb->pTable[id];
	if(pi->pItem){
		return BUCKET_FAILURE;
	}

	pi->pItem = pItem;
	prb->nInUse++;

	return BUCKET_SUCCESS;
}


/*
 * bucketRemoveItem()
 */
#ifdef __STDC__
int	bucketRemoveItem (BUCKET *prb, BUCKETID id, void *pItem)
#else
int	bucketRemoveItem (prb, id, pItem)
BUCKET *prb; 
BUCKETID id; 
void *pItem;
#endif
{
	ITEMPTR	*ppi;

	/*
	 * is the id to big ?
	 */
	if(id&~prb->indexMask){
		return BUCKET_FAILURE;
	}

	if(prb->indexShift){
		BUCKET	*pb;
		int	s;

		ppi = &prb->pTable[id>>prb->indexShift];
		pb = ppi->pBucket;

		if(!pb){
			return BUCKET_FAILURE;
		}	

		s = bucketRemoveItem(
			pb, 
			id&prb->nextIndexMask, 
			pItem);
		if(s!=BUCKET_SUCCESS){
			return s;
		}

		if(pb->nInUse==0){
			free(pb->pTable);
			free(pb);
			ppi->pBucket = NULL;
			prb->nInUse--;
		}
		return s;
	}

	ppi = &prb->pTable[id];
	if(ppi->pItem != pItem){
		return BUCKET_FAILURE;
	}

	prb->nInUse--;
	ppi->pItem = NULL;

	return BUCKET_SUCCESS;
}


/*
 * bucketLookupItem()
 */
#ifdef __STDC__
void	*bucketLookupItem(BUCKET *pb, BUCKETID id)
#else
void	*bucketLookupItem(pb, id)
BUCKET *pb; 
BUCKETID id;
#endif
{
	unsigned shift;

	/*
	 * is the id to big ?
	 */
	if(id&~pb->indexMask){
		return NULL;
	}

	while(shift = pb->indexShift){
		BUCKETID nextId;

		nextId = id & pb->nextIndexMask;

		pb = pb->pTable[id>>shift].pBucket;
		if(!pb){
			return pb;
		}	
		id = nextId;	
	}

	return pb->pTable[id].pItem;
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
	ITEMPTR	*pi;

	pi = pb->pTable;

	printf(	"Bucket: mask=%x entries in use=%d bytes in use=%d\n",
		(pb->nEntries-1)<<pb->indexShift,
		pb->nInUse,
		sizeof(*pb)+pb->nEntries*sizeof(*pi));

	if(pb->indexShift){
		for(	pi = pb->pTable;
			pi<&pb->pTable[pb->nEntries];
			pi++){
			if(pi->pBucket){
				bucketShow(pi->pBucket);
			}
		}
	}
	return BUCKET_SUCCESS;
}


