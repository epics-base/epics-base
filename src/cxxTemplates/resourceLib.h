/*
 *      $Id$
 *
 *      General hash table templates for fast indexing of resources
 *      of any base resource type and any resource identifier type. Fast 
 *      indexing is implemented with a hash lookup. The identifier type 
 *      implements the hash algorithm (or derives from one of the supplied 
 *      identifier types which provide a hashing routine). 
 *
 *      Unsigned integer and string identifier classes are supplied here.
 *
 *      Author  Jeffrey O. Hill 
 *				(string hash alg by Marty Kraimer and Peter K. Pearson)
 *
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
 *	NOTES:
 *	.01 Storage for identifier must persist until an item is deleted
 * 	.02 class T must derive from class ID and tsSLNode<T>
 *	
 */

#ifndef INCresourceLibh
#define INCresourceLibh 

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "tsSLList.h"
#include "shareLib.h"

typedef size_t resTableIndex;
static const unsigned indexWidth = sizeof(resTableIndex)*CHAR_BIT;

template <class T, class ID> class resTableIter;

//
// class resTable <T, ID>
//
// This class stores resource entires of type T which can be efficiently 
// located with a hash key of type ID.
//
//
// NOTES: 
// 1)   class T _must_ derive from class ID and also from class tsSLNode<T>
//
// 2)   Classes of type T installed into this resTable must implement a
//      "void destroy ()" method which is called by ~resTable() for each
//      resource entry in the resTable. The destroy() method should at a minimum
//      remove the resource from the resTable, and might also choose to (at your 
//      own discretion) "delete" the item itself.
//
// 3)   If the "resTable::show (unsigned level)" member function is called then 
//      class T must also implemt a "show (unsigned level)" member function which
//      dumps increasing diagnostics information with increasing "level" to
//      standard out.
//
// 4)   Classes of type ID must implement the following memeber functions:
//
//          // equivalence test
//          bool operator == (const ID &);
//
//          // ID to hash index convert (see examples below)
//          index hash (unsigned nBitsHashIndex) const; 
//
// 5)   Classes of type ID must provide the following public compile time
//      determined static member constants. They determine the minimum and maximum 
//      number of elements in the hash table. Knowing these parameters at 
//      compile time improves performance. If minIndexBitWidth == maxIndexBitWidth
//      then the hash table size is determined at compile time
//
//          static const unsigned maxIndexBitWidth = nnnn;
//          static const unsigned minIndexBitWidth = nnnn;
//
//          max number of hash table elements = 1 << maxIndexBitWidth
//          min number of hash table elements = 1 << minIndexBitWidth;
//
// 6)   Storage for identifier of type ID must persist until the item of type 
//      T is deleted from the resTable
//
template <class T, class ID>
class resTable {
    friend class resTableIter<T,ID>;
public:

    //
    // exceptions thrown
    //
    class epicsShareClass dynamicMemoryAllocationFailed {};
    class epicsShareClass entryDidntRespondToDestroyVirtualFunction {};
    class epicsShareClass sizeExceedsMaxIndexWidth {};

	resTable (unsigned nHashTableEntries);

	virtual ~resTable();

	void destroyAllEntries(); // destroy all entries

	//
	// Call (pT->show) (level) for each entry
	// where pT is a pointer to type T. Show
    // returns "void". Show dumps increasing
    // diagnostics to std out with increasing 
    // magnitude of the its level argument.
	//
	void show (unsigned level) const;

    //
    // add entry
    //
    // returns -1 if the id already exits in the table
    // and zero if successful
    //
	int add (T &res);

	T *remove (const ID &idIn); // remove entry

	T *lookup (const ID &idIn) const; // locate entry

#ifdef _MSC_VER
	//
	// required by MS vis c++ 5.0 (but not by 4.0)
	//
	typedef void (T::*pSetMFArg_t)();
#	define pSetMFArg(ARG) pSetMFArg_t ARG
#else
	//
	// required by gnu g++ 2.7.2
	//
#	define pSetMFArg(ARG) void (T:: * ARG)()
#endif

	//
	// Call (pT->*pCB) () for each entry
	//
	// where pT is a pointer to type T and pCB is
	// a pointer to a memmber function of T with 
	// no parameters that returns void
	//
	void traverse (pSetMFArg(pCB)) const;

private:
    tsSLList<T>     *pTable;
    unsigned        hashIdMask;
    unsigned        hashIdNBits;
    unsigned        nInUse;

	resTableIndex hash (const ID & idIn) const;

	T *find (tsSLList<T> &list, const ID &idIn) const;

	T *findDelete (tsSLList<T> &list, const ID &idIn);
};

//
// Some ID classes that work with the above template
//

//
// class resTableIter
//
// an iterator for the resource table class
//
template <class T, class ID>
class resTableIter {
public:
    resTableIter (const resTable<T,ID> &tableIn);
	T * next ();
	T * operator () ();
private:
    tsSLIter<T>             iter;
    unsigned                index;
	const resTable<T,ID>    *pTable;
};

//
// class intId
//
// signed or unsigned integer identifier (class T must be
// a signed or unsigned interger type)
//
// this class works as type ID in resTable <class T, class ID>
//
// 1<<MIN_INDEX_WIDTH specifies the minimum number of
// elements in the hash table within resTable <class T, class ID>
//
// MAX_ID_WIDTH specifies the maximum number of ls bits in an 
// integer identifier which might be set at any time. Set this 
// parameter to zero if unsure of the correct minimum hash table 
// size.
//
// MIN_INDEX_WIDTH and MAX_ID_WIDTH are specified here at
// compile time so that the hash index can be produced 
// efficently. Hash indexes are produced more efficiently 
// when (MAX_ID_WIDTH - MIN_INDEX_WIDTH) is minimized.
//
template <class T, unsigned MIN_INDEX_WIDTH = 4, unsigned MAX_ID_WIDTH = sizeof(T)*CHAR_BIT>
class epicsShareClass intId {
public:
	intId (const T &idIn);
	bool operator == (const intId &idIn) const;
	resTableIndex hash (unsigned nBitsIndex) const;
	const T getId() const;

    static resTableIndex hashEngine (const T &id); // can be used by other classes

    static const unsigned maxIndexBitWidth;
    static const unsigned minIndexBitWidth;

protected:
	T id;
};

//
// class chronIntIdResTable <ITEM>
//
// a specialized resTable which uses unsigned integer keys which are
// allocated in chronological sequence
// 
// NOTE: ITEM must public inherit from chronIntIdRes <ITEM>
//
typedef intId<unsigned, 8> chronIntId;
template <class ITEM>
class chronIntIdResTable : public resTable<ITEM, chronIntId> {
public:
    chronIntIdResTable (unsigned nHashTableEntries);
    virtual ~chronIntIdResTable ();
    void add (ITEM &item);
private:
    unsigned allocId;
};

//
// class chronIntIdRes<ITEM>
//
// resource with unsigned chronological identifier
//
template <class ITEM>
class chronIntIdRes : public chronIntId, public tsSLNode<ITEM> {
    friend class chronIntIdResTable<ITEM>;
public:
    chronIntIdRes ();
private:
    void setId (unsigned newId);
};

//
// class stringId
//
// character string identifier
//
class epicsShareClass stringId {
public:

    //
    // exceptions
    //
    class epicsShareClass dynamicMemoryAllocationFailed {};

    enum allocationType {copyString, refString};

	stringId (const char * idIn, allocationType typeIn=copyString);

    ~ stringId();

	resTableIndex hash (unsigned nBitsIndex) const;
 
	bool operator == (const stringId &idIn) const;

	const char * resourceName() const; // return the pointer to the string

	void show (unsigned level) const;

    static const unsigned maxIndexBitWidth;

    static const unsigned minIndexBitWidth;

private:
	const char * pStr;
	const allocationType allocType;
	static const unsigned char fastHashPermutedIndexSpace[256];
};

/////////////////////////////////////////////////
// resTable<class T, class ID> member functions
/////////////////////////////////////////////////

//
// resTable::resTable (unsigned nHashTableEntries)
//
template <class T, class ID>
resTable<T,ID>::resTable (unsigned nHashTableEntries) :
	nInUse (0) 
{
    unsigned nbits, mask;

	//
	// count the number of bits in the hash index
	//
	for (nbits=0; nbits<indexWidth; nbits++) {
		mask = (1<<nbits) - 1;
		if ( ((nHashTableEntries-1) & ~mask) == 0){
			break;
		}
	}

    if ( nbits > ID::maxIndexBitWidth ) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else            
            throw sizeExceedsMaxIndexWidth ();
#       endif
    }

    //
    // it improves performance to round up to a 
    // minimum table size
    //
    if (nbits<ID::minIndexBitWidth) {
        nbits = ID::minIndexBitWidth;
        mask = (1<<nbits) - 1;
	}

	this->hashIdNBits = nbits;
    this->hashIdMask = mask;
	this->nInUse = 0u;
	this->pTable = new tsSLList<T> [1<<nbits];
	if (this->pTable==0) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else            
            throw dynamicMemoryAllocationFailed ();
#       endif
	}
}

//
// resTable::remove ()
//
// remove a res from the resTable
//
template <class T, class ID>
inline T * resTable<T,ID>::remove (const ID &idIn)
{
	tsSLList<T> &list = this->pTable[this->hash(idIn)];
	return this->findDelete(list, idIn);
}

//
// resTable::lookup ()
//
// find an res in the resTable
//
template <class T, class ID>
inline T * resTable<T,ID>::lookup (const ID &idIn) const
{
	tsSLList<T> &list = this->pTable[this->hash(idIn)];
	return this->find(list, idIn);
}

//
// resTable::hash ()
//
template <class T, class ID>
inline resTableIndex resTable<T,ID>::hash (const ID & idIn) const
{
	return idIn.hash (this->hashIdNBits) & this->hashIdMask;
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
		// from the resTable when it is destroyed.
		//
		{
			tsSLIterRm<T> iter(*pList);
			while ( (pItem=iter()) ) {
				fprintf (stderr, 
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
	tsSLList<T> *pList;
	double X;
	double XX;
	double mean;
	double stdDev;
	unsigned maxEntries;

	printf("resTable with %d resources installed\n", this->nInUse);

	if (level >=1u) {
		pList = this->pTable;
		X = 0.0;
		XX = 0.0;
		maxEntries = 0u;
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
			if (count>0u) {
				X += count;
				XX += count*count;
				if (count>maxEntries) {
					maxEntries = count;
				}
			}
			pList++;
		}
	 
		mean = X/(this->hashIdMask+1);
		stdDev = sqrt(XX/(this->hashIdMask+1) - mean*mean);
		printf( 
	"entries/occupied resTable entry: mean = %f std dev = %f max = %d\n",
			mean, stdDev, maxEntries);
	}
}

//
// resTable<T,ID>::traverse
//
template <class T, class ID>
void resTable<T,ID>::traverse (pSetMFArg(pCB)) const
{
	tsSLList<T> *pList;

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
// add a res to the resTable
//
// (bad status on failure)
//
template <class T, class ID>
int resTable<T,ID>::add (T &res)
{
	//
	// T must derive from ID
	//
	tsSLList<T> &list = this->pTable[this->hash(res)];

	if ( this->find (list, res) != 0 ) {
		return -1;
	}

	list.add (res);
	this->nInUse++;

    return 0;
}

//
// find
// searches from where the iterator points to the
// end of the list for idIn
//
// iterator points to the item found upon return
// (or NULL if nothing matching was found)
//
template <class T, class ID>
T *resTable<T,ID>::find (tsSLList<T> &list, const ID &idIn) const
{
	tsSLIter<T> iter(list);
	T *pItem;
	ID *pId;

	while ( (pItem = iter()) ) {
		pId = pItem;
		if (*pId == idIn) {
			break;
		}
	}
	return pItem;
}

//
// findDelete
// searches from where the iterator points to the
// end of the list for idIn
//
// iterator points to the item found upon return
// (or NULL if nothing matching was found)
//
// removes the item if it finds it
//
template <class T, class ID>
T *resTable<T,ID>::findDelete (tsSLList<T> &list, const ID &idIn)
{
	tsSLIterRm<T> iter(list);
	T *pItem;
	ID *pId;

	while ( (pItem = iter()) ) {
		pId = pItem;
		if (*pId == idIn) {
			iter.remove();
			this->nInUse--;
			break;
		}
	}
	return pItem;
}

//
// ~resTable<T,ID>::resTable()
//
template <class T, class ID>
resTable<T,ID>::~resTable() 
{
	if (this->pTable) {
		this->destroyAllEntries();
		if (this->nInUse != 0u) {
#           ifdef noExceptionsFromCXX
                assert (0);
#           else            
                throw entryDidntRespondToDestroyVirtualFunction ();
#           endif
		}
		delete [] this->pTable;
	}
}

//////////////////////////////////////////////
// resTableIter<T,ID> member functions
//////////////////////////////////////////////

//
// resTableIter<T,ID>::resTableIter ()
//
template <class T, class ID>
inline resTableIter<T,ID>::resTableIter (const resTable<T,ID> &tableIn) : 
    iter (tableIn.pTable[0]), index (1), pTable (&tableIn) {} 

//
// resTableIter<T,ID>::next ()
//
template <class T, class ID>
inline T * resTableIter<T,ID>::next ()
{
    T *pNext = this->iter.next();
    if (pNext) {
        return pNext;
    }
    if ( this->index >= (1u<<this->table.hashIdNBits) ) {
        return 0;
    }
    ;
    this->iter = tsSLIter<T> (this->pTable->pTable[this->index++]);
    return this->iter.next ();
}

//
// resTableIter<T,ID>::operator () ()
//
template <class T, class ID>
inline T * resTableIter<T,ID>::operator () ()
{
    return this->next ();
}

//////////////////////////////////////////////
// chronIntIdResTable<ITEM> member functions
//////////////////////////////////////////////

//
// chronIntIdResTable<ITEM>::chronIntIdResTable()
//
template <class ITEM>
inline chronIntIdResTable<ITEM>::chronIntIdResTable (unsigned nHashTableEntries) : 
    resTable<ITEM, chronIntId> (nHashTableEntries),
    allocId(1u) {} // hashing is faster close to zero

//
// chronIntIdResTable<ITEM>::~chronIntIdResTable()
// (not inline because it is virtual)
//
template <class ITEM>
chronIntIdResTable<ITEM>::~chronIntIdResTable() {}

//
// chronIntIdResTable<ITEM>::add()
//
// NOTE: This detects (and avoids) the case where 
// the PV id wraps around and we attempt to have two 
// resources with the same id.
//
template <class ITEM>
inline void chronIntIdResTable<ITEM>::add (ITEM &item)
{
    int status;
    do {
	    item.chronIntIdRes<ITEM>::setId (allocId++);
        status = this->resTable<ITEM,chronIntId>::add (item);
    }
    while (status);
}

/////////////////////////////////////////////////
// chronIntIdRes<ITEM> member functions
/////////////////////////////////////////////////

//
// chronIntIdRes<ITEM>::chronIntIdRes
//
template <class ITEM>
inline chronIntIdRes<ITEM>::chronIntIdRes () : chronIntId (UINT_MAX) {}

//
// id<ITEM>::setId ()
//
// workaround for bug in DEC compiler
//
template <class ITEM>
inline void chronIntIdRes<ITEM>::setId (unsigned newId) 
{
    this->id = newId;
}

/////////////////////////////////////////////////
// intId member functions
/////////////////////////////////////////////////

//
// intId::intId ()
//
template <class T, unsigned MIN_INDEX_WIDTH, unsigned MAX_ID_WIDTH>
inline intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH>::intId (const T &idIn) 
    : id (idIn) {}

//
// intId::operator == ()
//
template <class T, unsigned MIN_INDEX_WIDTH, unsigned MAX_ID_WIDTH>
inline bool intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH>::operator == 
        (const intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH> &idIn) const
{
	return this->id == idIn.id;
}

//
// intId::getId ()
//
template <class T, unsigned MIN_INDEX_WIDTH, unsigned MAX_ID_WIDTH>
inline const T intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH>::getId () const
{
	return this->id;
}

//
// const unsigned intId::minIndexBitWidth
//
template <class T, unsigned MIN_INDEX_WIDTH, unsigned MAX_ID_WIDTH>
const unsigned intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH>::minIndexBitWidth = MIN_INDEX_WIDTH;


//
//  const unsigned intId::maxIndexBitWidth
//
template <class T, unsigned MIN_INDEX_WIDTH, unsigned MAX_ID_WIDTH>
const unsigned intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH>::maxIndexBitWidth = indexWidth;

//
// intId::hashEngine()
//
// converts any integer into a hash table index
//
//
template <class T, unsigned MIN_INDEX_WIDTH, unsigned MAX_ID_WIDTH>
inline resTableIndex intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH>::hashEngine (const T &id)
{
	resTableIndex hashid = static_cast<resTableIndex>(id);

	//
    // On most compilers the optimizer will unroll this loop so this
    // is actually a very small inline function
    //
	// Experiments using the microsoft compiler show that this isnt 
	// slower than switching on the architecture size and urolling the
	// loop explicitly (that solution has resulted in portability
	// problems in the past).
	//
    unsigned width = MAX_ID_WIDTH;
    do {
        width >>= 1u;
		hashid ^= hashid>>width;
    } while (width>MIN_INDEX_WIDTH);

	//
	// the result here is always masked to the
	// proper size after it is returned to the "resTable" class
	//
	return hashid;
}


//
// intId::hash()
//
template <class T, unsigned MIN_INDEX_WIDTH, unsigned MAX_ID_WIDTH>
inline resTableIndex intId<T, MIN_INDEX_WIDTH, MAX_ID_WIDTH>::hash (unsigned /* nBitsIndex */) const
{
    return this->hashEngine (this->id);
}

////////////////////////////////////////////////////
// stringId member functions
////////////////////////////////////////////////////

//
// stringId::operator == ()
//
inline bool stringId::operator == 
        (const stringId &idIn) const
{
	if (this->pStr!=NULL && idIn.pStr!=NULL) {
		return strcmp(this->pStr,idIn.pStr)==0;
	}
	else {
		return false; // not equal
	}
}

//
// stringId::resourceName ()
//
inline const char * stringId::resourceName () const
{
	return this->pStr;
}

#ifdef instantiateRecourceLib

//
// stringId::stringId()
//
stringId::stringId (const char * idIn, allocationType typeIn) :
	allocType (typeIn)
{
	if (typeIn==copyString) {
		unsigned nChars = strlen (idIn) + 1u;

		this->pStr = new char [nChars];
		if (this->pStr!=0) {
			memcpy ((void *)this->pStr, idIn, nChars);
		}
		else {
#           ifdef noExceptionsFromCXX
                assert (0);
#           else            
                throw dynamicMemoryAllocationFailed ();
#           endif
		}
	}
	else {
		this->pStr = idIn;
	}
}

//
// const unsigned stringId::minIndexBitWidth
//
// this limit is based on limitations in the hash
// function below
//
const unsigned stringId::minIndexBitWidth = 8;

//
// const unsigned stringId::maxIndexBitWidth
//
// see comments related to this limit in the hash
// function below
//
const unsigned stringId::maxIndexBitWidth = 16;

//
// stringId::show ()
//
void stringId::show (unsigned level) const
{
	if (level>2u) {
		printf ("resource id = %s\n", this->pStr);
	}
}

//
// stringId::~stringId()
//
//
// this needs to be instanciated only once (normally in libCom)
//
stringId::~stringId()
{
	if (this->allocType==copyString) {
		if (this->pStr!=NULL) {
			//
			// the microsoft and solaris compilers will 
			// not allow a pointer to "const char"
			// to be deleted 
			//
			// the HP-UX compiler gives us a warning on
			// each cast away of const, but in this case
			// it cant be avoided. 
			//
			// The DEC compiler complains that const isnt 
			// really significant in a cast if it is present.
			//
			// I hope that deleting a pointer to "char"
			// is the same as deleting a pointer to 
			// "const char" on all compilers
			//
			delete [] const_cast<char *>(this->pStr);
		}
	}
}

//
// stringId::hash()
//
// This hash algorithm is a modification of the algorithm described in 
// Fast Hashing of Variable Length Text Strings, Peter K. Pearson, 
// Communications of the ACM, June 1990. The initial modifications 
// were designed by Marty Kraimer. Some additional minor optimizations
// by Jeff Hill.
//
resTableIndex stringId::hash(unsigned nBitsIndex) const
{
    const unsigned char *pUStr = 
        reinterpret_cast<const unsigned char *>(this->pStr);

	if (pUStr==NULL) {
		return 0u;
	}

	unsigned h0 = 0u;
	unsigned h1 = 0u;
	unsigned c;

    while (true) {

        c = *(pUStr++);
        if (c==0) {
            break;
        }
        h0 = fastHashPermutedIndexSpace[h0 ^ c];

        c = *(pUStr++);
        if (c==0) {
            break;
        }
        h1 = fastHashPermutedIndexSpace[h1 ^ c];
	}

    h1 = h1 << (nBitsIndex-8u);
    h0 = h1 ^ h0;

	return h0;
}

//
// The hash algorithm is a modification of the algorithm described in
// Fast Hashing of Variable Length Text Strings, Peter K. Pearson,
// Communications of the ACM, June 1990
// The modifications were designed by Marty Kraimer
//
const unsigned char stringId::fastHashPermutedIndexSpace[256] = {
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

#endif // if instantiateRecourceLib is defined

#endif // INCresourceLibh

