/*
 *
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
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
 *
 * History
 * $Log$
 * Revision 1.1  1996/11/02 01:07:48  jhill
 * installed
 *
 *
 *	NOTES:
 *	.01 Storage for identifier must persist until an item is deleted
 * 	.02 class T must derive from class ID and tsSLNode<T>
 */

#ifndef INCresourceLibcc
#define INCresourceLibcc

#include <math.h>

//
// resTable<T,ID>::init()
//
template <class T, class ID>
int resTable<T,ID>::init(unsigned nHashTableEntries) 
{
	unsigned	nbits;

	if (nHashTableEntries<1u) {
		return -1;
	}

	//
	// count the number of bits in the hash index
	//
	for (nbits=0; nbits<resTableIndexBitWidth; nbits++) {
		this->hashIdMask = (1<<nbits) - 1;
		if ( ((nHashTableEntries-1) & ~this->hashIdMask) == 0){
			break;
		}
	}
	this->hashIdNBits = nbits;
	this->nInUse = 0u;
	this->pTable = new tsSLList<T> [this->hashIdMask+1u];
	if (!pTable) {
		return -1;
	}
	return 0;
}

//
// resTable<T,ID>::destroyAllEntries()
//
template <class T, class ID>
void resTable<T,ID>::destroyAllEntries()
{
	tsSLList<T> *pList = this->pTable;

	while (pList<&this->pTable[this->hashIdMask+1]) {
		tsSLIter<T> iter(*pList);
		T *pItem;
		T *pNextItem;

		pItem = iter();
		while (pItem) {
			pNextItem = iter();
			delete pItem;
			pItem = pNextItem;
		}
		//
		// Check to see if a defective class is
		// installed that does not remove itself
		// from the table when it is destroyed.
		//
		iter.reset();
		while ( (pItem=iter()) ) {
			fprintf(stderr, 
"Warning: Defective class still in resTable<T,ID> after it was destroyed\n");
			//
			// remove defective class
			//
			iter.remove();
			this->nInUse--;
		}
		pList++;
	}
}

//
// resTable<T,ID>::show
//
template <class T, class ID>
void resTable<T,ID>::show (unsigned level)
{
	tsSLList<T> 	*pList;
	double		X;
	double		XX;
	double		mean;
	double		stdDev;
	unsigned	maxEntries;

	printf("resTable with %d resources installed\n", this->nInUse);

	if (level >=1u) {
		pList = this->pTable;
		X = 0.0;
		XX = 0.0;
		maxEntries = 0;
		while (pList < &this->pTable[this->hashIdMask+1]) {
			unsigned count;
			tsSLIter<T> iter(*pList);
			T *pItem;

			count = 0;
			while ( (pItem = iter()) ) {
				if (level >= 3u) {
					pItem->show (level);
				}
				count++;
			}
			X += count;
			XX += count*count;
			if (count>maxEntries) {
				maxEntries = count;
			}
			pList++;
		}
	 
		mean = X/(this->hashIdMask+1);
		stdDev = sqrt(XX/(this->hashIdMask+1)- mean*mean);
		printf( 
	"entries/table index - mean = %f std dev = %f max = %d\n",
			mean, stdDev, maxEntries);
	}
}

#endif // INCresourceLibcc

