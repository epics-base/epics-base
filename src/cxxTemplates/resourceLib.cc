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
 * Revision 1.3  1997/04/10 19:43:08  jhill
 * API changes
 *
 * Revision 1.2  1996/12/06 22:26:34  jhill
 * added auto cleanup of installed classes to destroy
 *
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
		T *pItem;
		T *pNextItem;

		{
			tsSLIter<T> iter(*pList);
			pItem = iter();
			while (pItem) {
				pNextItem = iter();
				pItem->destroy();
				pItem = pNextItem;
			}
		}

		//
		// Check to see if a defective class is
		// installed that does not remove itself
		// from the table when it is destroyed.
		//
		{
			tsSLIterRm<T> iter(*pList);
			while ( (pItem=iter()) ) {
				fprintf(stderr, 
"Warning: Defective class still in resTable<T,ID> after it was destroyed\n");
				//
				// remove defective class
				//
				iter.remove();
				this->nInUse--;
			}
		}

		pList++;
	}
}

//
// resTable<T,ID>::show
//
template <class T, class ID>
void resTable<T,ID>::show (unsigned level) const
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

//
// resTable<T,ID>::traverse
//
// pCB is a pointer to a member function
//
template <class T, class ID>
void resTable<T,ID>::traverse (void (T::*pCB)())
{
	tsSLList<T> 	*pList;

	pList = this->pTable;
	while (pList < &this->pTable[this->hashIdMask+1]) {
		tsSLIter<T> iter(*pList);
		T *pItem;

		while ( (pItem = iter()) ) {
			(pItem->*pCB) ();
		}
		pList++;
	}
}

//
// this needs to be instanciated only once (normally in libCom)
//
#ifdef instantiateStringIdFastHash 
//
// The hash algorithm is a modification of the algorithm described in
// Fast Hashing of Variable Length Text Strings, Peter K. Pearson,
// Communications of the ACM, June 1990
// The modifications were designed by Marty Kraimer
//
epicsShareExtern const unsigned char stringIdFastHash[256] = {
 39,159,180,252, 71,  6, 13,164,232, 35,226,155, 98,120,154, 69,
157, 24,137, 29,147, 78,121, 85,112,  8,248,130, 55,117,190,160,
176,131,228, 64,211,106, 38, 27,140, 30, 88,210,227,104, 84, 77,
 75,107,169,138,195,184, 70, 90, 61,166,  7,244,165,108,219, 51,
  9,139,209, 40, 31,202, 58,179,116, 33,207,146, 76, 60,242,124,
254,197, 80,167,153,145,129,233,132, 48,246, 86,156,177, 36,187,
 45,  1, 96, 18, 19, 62,185,234, 99, 16,218, 95,128,224,123,253,
 42,109,  4,247, 72,  5,151,136,  0,152,148,127,204,133, 17, 14,
182,217, 54,199,119,174, 82, 57,215, 41,114,208,206,110,239, 23,
189, 15,  3, 22,188, 79,113,172, 28,  2,222, 21,251,225,237,105,
102, 32, 56,181,126, 83,230, 53,158, 52, 59,213,118,100, 67,142,
220,170,144,115,205, 26,125,168,249, 66,175, 97,255, 92,229, 91,
214,236,178,243, 46, 44,201,250,135,186,150,221,163,216,162, 43,
 11,101, 34, 37,194, 25, 50, 12, 87,198,173,240,193,171,143,231,
111,141,191,103, 74,245,223, 20,161,235,122, 63, 89,149, 73,238,
134, 68, 93,183,241, 81,196, 49,192, 65,212, 94,203, 10,200, 47
};
#endif // instantiateStringIdFastHash 

#endif // INCresourceLibcc

