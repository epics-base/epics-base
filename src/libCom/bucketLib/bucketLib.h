/*
 *	$Id$
 *
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 *      Date:  9-93 
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
 * 	.01 091493 joh	added nEntries to struct bucket
 * 	.02 121693 joh	added bucketFree() 
 * 	.03 052395 joh	use std EPICS status 
 *	$Log$
 *	Revision 1.3  1997/04/10 19:45:35  jhill
 *	API changes and include with  not <>
 *
 *	Revision 1.2  1996/06/19 19:44:53  jhill
 *	C++ support
 *
 *
 *	NOTES:
 *	.01 Storage for identifier must persist until an item is deleted
 */

#ifndef INCbucketLibh
#define INCbucketLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "errMdef.h"
#include "epicsTypes.h"

typedef	unsigned 	BUCKETID;

typedef enum {bidtUnsigned, bidtPointer, bidtString} buckTypeOfId;

typedef struct item{
	struct item	*pItem;
	READONLY void	*pId;
	READONLY void   *pApp;
	buckTypeOfId	type;
}ITEM;

typedef struct bucket{
	ITEM		**pTable;
	void		*freeListPVT;
	unsigned	hashIdMask;
	unsigned	hashIdNBits;
        unsigned        nInUse;
}BUCKET;


#if defined(__STDC__) || defined(__cplusplus)
BUCKET	*bucketCreate (unsigned nHashTableEntries);
int	bucketFree (BUCKET *prb);
int	bucketShow (BUCKET *pb);

/*
 * !! Identifier must exist (and remain constant) at the specified address until
 * the item is deleted from the bucket !!
 */
int     bucketAddItemUnsignedId (BUCKET *prb, 
		READONLY unsigned *pId, READONLY void *pApp);
int     bucketAddItemPointerId (BUCKET *prb, 
		void * READONLY *pId, READONLY void *pApp);
int     bucketAddItemStringId (BUCKET *prb, 
		READONLY char *pId, READONLY void *pApp);

int     bucketRemoveItemUnsignedId (BUCKET *prb, READONLY unsigned *pId);
int     bucketRemoveItemPointerId (BUCKET *prb, void * READONLY *pId);
int     bucketRemoveItemStringId (BUCKET *prb, READONLY char *pId);

void    *bucketLookupItemUnsignedId (BUCKET *prb, READONLY unsigned *pId);
void    *bucketLookupItemPointerId (BUCKET *prb, void * READONLY *pId);
void    *bucketLookupItemStringId (BUCKET *prb, READONLY char *pId);

#else /*__STDC__*/
BUCKET	*bucketCreate ();
int	bucketFree ();
int	bucketShow ();
int     bucketAddItemUnsignedId ();
int     bucketAddItemPointerId ();
int     bucketAddItemStringId ();
int     bucketRemoveItemUnsignedId ();
int     bucketRemoveItemPointerId ();
int     bucketRemoveItemStringId ();
void    *bucketLookupItemUnsignedId ();
void    *bucketLookupItemPointerId ();
void    *bucketLookupItemStringId ();
#endif /*__STDC__*/ 

/*
 * Status returned by bucketLib functions
 */
#define BUCKET_SUCCESS		S_bucket_success
#define S_bucket_success	0
#define S_bucket_noMemory	(M_bucket | 1) 	/*Memory allocation failed*/
#define S_bucket_idInUse	(M_bucket | 2) 	/*Identifier already in use*/
#define S_bucket_uknId		(M_bucket | 3) 	/*Unknown identifier*/

#ifdef __cplusplus
}
#endif

#endif /*INCbucketLibh*/

