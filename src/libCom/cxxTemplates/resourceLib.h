/*
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
 *
 *	NOTES:
 *	.01 Storage for identifier must persist until an item is deleted
 * 	.02 class T must derive from class ID and tsSLNode<T>
 */

#ifndef INCresourceLibh
#define INCresourceLibh 

#include <tsSLList.h>
#include <math.h>

typedef int 		resLibStatus;
typedef	unsigned 	resTableIndex;

const unsigned resTableIndexBitWidth = (sizeof(resTableIndex)*CHAR_BIT);

template <class T, class ID>
class resTable {
public:
	resTable() :
		pTable(0), hashIdMask(0), hashIdNBits(0), nInUse(0) {}

	int init(unsigned nHashTableEntries) 
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

	~resTable() 
	{
		assert (this->nInUse == 0u);

		if (this->pTable) {
			delete [] this->pTable;
		}
	}

	void destroyAllEntries()
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
        			this->nInUse--;
				pItem = pNextItem;
			}
			pList++;
		}
	}

	void show (unsigned level)
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


public:
	int add (T &res)
	{
		//
		// T must derive from ID
		//
		tsSLList<T> &list = this->pTable[this->hash(res)];
		tsSLNode<T> *pPrev;

		pPrev = this->find(list, res);
		if (pPrev) {
			return -1;
		}
		list.add(res);
		this->nInUse++;
		return 0;
	}

	T *remove (const ID &idIn)
	{
		tsSLList<T> &list = this->pTable[this->hash(idIn)];
		tsSLNode<T> *pPrev;
		T *pItem;

		pPrev = this->find(list, idIn);
		if (!pPrev) {
			return 0;
		}
		this->nInUse--;
		pItem = pPrev->next();
		list.remove(*pItem, *pPrev);
		return pItem;
	}


	T *lookup (const ID &idIn)
	{
		tsSLList<T> &list = this->pTable[this->hash(idIn)];
		tsSLNode<T> *pPrev;

		pPrev = this->find(list, idIn);
		if (pPrev) {
			return pPrev->next();
		}
		return NULL;
	}

private:
	tsSLList<T>	*pTable;
	unsigned	hashIdMask;
	unsigned	hashIdNBits;
        unsigned       	nInUse;

	resTableIndex hash(const ID & idIn)
	{
		resTableIndex hashid;
		hashid = idIn.resourceHash(this->hashIdNBits);
		return hashid & this->hashIdMask;
	}

	//
	// beware - this returns a pointer to the node just prior to the
	// the item found on the list
	//
	tsSLNode<T> *find(tsSLList<T> &list, const ID &idIn)
	{
		tsSLIter<T>	iter(list);
		T		*pItem;
		ID		*pId;

		while ( (pItem = iter()) ) {
			pId = pItem;
			if (*pId == idIn) {
				return iter.prev(); 
			}
		}
		return NULL;
	}
};

//
// Some ID classes that work with the above template
//


//
// unsigned identifier
//
class uintId {
public:
	uintId(unsigned idIn=~0u) : id(idIn) {}

        resTableIndex resourceHash(unsigned nBitsId) const
        {
                unsigned        src = this->id;
                resTableIndex 	hashid;
 
                hashid = src;
                src = src >> nBitsId;
                while (src) {
                        hashid = hashid ^ src;
                        src = src >> nBitsId;
                }
                //
                // the result here is always masked to the
                // proper size after it is returned to the resource class
                //
                return hashid;
        }
 
        int operator == (const uintId &idIn)
        {
                return this->id == idIn.id;
        }

	const unsigned getId() const
	{
		return id;
	}
protected:
        unsigned id;
};

//
// resource with unsigned chronological identifier
//
template <class ITEM>
class uintRes : public uintId, public tsSLNode<ITEM> {
friend class uintResTable<ITEM>;
};

//
// special resource table which uses 
// unsigned integer keys allocated in chronological sequence
//
template <class ITEM>
class uintResTable : public resTable<ITEM, uintId> {
public:
	uintResTable() : allocId(1u) {} // hashing is faster close to zero

	//
	// NOTE: This detects (and avoids) the case where 
	// the PV id wraps around and we attempt to have two 
	// resources with the same id.
	//
	void installItem(ITEM &item)
	{
		int resTblStatus;
		do {
			item.uintId::id = this->allocId++;
			resTblStatus = this->add(item);
		}
		while (resTblStatus);
	}
private:
	unsigned 	allocId;
};

//
// pointer identifier
//
class ptrId {
public:
        ptrId (void const * const idIn) : id(idIn) {}
 
        resTableIndex resourceHash(unsigned nBitsId) const
        {
		//
		// This makes the assumption that
		// a pointer will fit inside of a long
		// (this assumption may not port to all
		// CPU architectures)
		//
		unsigned long src = (unsigned long) this->id;
                resTableIndex hashid;
 
                hashid = src;
                src = src >> nBitsId;
                while (src) {
                        hashid = hashid ^ src;
                        src = src >> nBitsId;
                }
                //
                // the result here is always masked to the
                // proper size after it is returned to the resource class
                //
                return hashid;
        }
 
        int operator == (const ptrId &idIn)
        {
                return this->id == idIn.id;
        }
private:
        void const * const id;
};

//
// character string identifier
//
class stringId {
public:
        stringId (char const * const idIn) : id(idIn) {}
 
        resTableIndex resourceHash(unsigned nBitsId) const
        {
		const char *pStr = this->id;
                resTableIndex hashid;
		unsigned i;
 
                hashid = 0u;
		for (i=0u; pStr[i]; i++) {
			hashid += pStr[i] * (i+1u);
		}

		hashid = hashid % (1u<<nBitsId);

                return hashid;
        }
 
        int operator == (const stringId &idIn)
        {
                return strcmp(this->id,idIn.id)==0;
        }

	const char * resourceName()
	{
		return id;
	}

	void show (unsigned)
	{
		printf ("resource id = %s\n", id);
	}
private:
        char const * const id;
};

#endif // INCresourceLibh

