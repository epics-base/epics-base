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
 * Revision 1.8  1997/04/10 19:43:09  jhill
 * API changes
 *
 * Revision 1.7  1996/12/06 22:26:36  jhill
 * added auto cleanup of installed classes to destroy
 *
 * Revision 1.6  1996/11/02 01:07:17  jhill
 * many improvements
 *
 * Revision 1.5  1996/09/04 19:57:06  jhill
 * string id resource now copies id
 *
 * Revision 1.4  1996/08/05 19:31:59  jhill
 * fixed removes use of iter.cur()
 *
 * Revision 1.3  1996/07/25 17:58:16  jhill
 * fixed missing ref in list decl
 *
 * Revision 1.2  1996/07/24 22:12:02  jhill
 * added remove() to iter class + made node's prev/next private
 *
 * Revision 1.1.1.1  1996/06/20 22:15:55  jhill
 * installed  ca server templates
 *
 *
 *	NOTES:
 *	.01 Storage for identifier must persist until an item is deleted
 * 	.02 class T must derive from class ID and tsSLNode<T>
 */

#ifndef INCresourceLibh
#define INCresourceLibh 

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "tsSLList.h"
#include "shareLib.h"

typedef int 		resLibStatus;
typedef	unsigned 	resTableIndex;

#define resTableIndexBitWidth (sizeof(resTableIndex)*CHAR_BIT)

//
// class T must derive from class ID and also from class tsSLNode<T>
//
// NOTE: Classes installed into this table should have
// a virtual destructor so that the delete in ~resTable() will
// work correctly.
//
template <class T, class ID>
class resTable {
public:
	resTable() :
		pTable(0), hashIdMask(0), hashIdNBits(0), nInUse(0) {}

	int init(unsigned nHashTableEntries);

	~resTable() 
	{
		if (this->pTable) {
			this->destroyAllEntries();
			assert (this->nInUse == 0u);
			delete [] this->pTable;
		}
	}

	//
	// destroy all res in the table
	//
	void destroyAllEntries();

	//
	// call T::show(level) for each res in the table
	//
	void show (unsigned level) const;

	//
	// add a res to the table
	//
	int add (T &res)
	{
		//
		// T must derive from ID
		//
		tsSLList<T> &list = this->pTable[this->hash(res)];

		if (this->find(list, res) != 0) {
			return -1;
		}
		list.add(res);
		this->nInUse++;
		return 0;
	}

	//
	// remove a res from the table
	//
	T *remove (const ID &idIn)
	{
		tsSLList<T> &list = this->pTable[this->hash(idIn)];
		return this->findDelete(list, idIn);
	}


	//
	// find an res in the table
	//
	T *lookup (const ID &idIn)
	{
		tsSLList<T> &list = this->pTable[this->hash(idIn)];
		return this->find(list, idIn);
	}

	//
	// Call (pT->*pCB) () for each item in the table
	//
	// where pT is a pointer to type T and pCB is
	// a pointer to a memmber function of T with 
	// no parameters and returning void
	//
	void traverse(void (T::*pCB)());

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
	// find
	// searches from where the iterator points to the
	// end of the list for idIn
	//
	// iterator points to the item found upon return
	// (or NULL if nothing matching was found)
	//
	T *find (tsSLList<T> &list, const ID &idIn)
	{
		tsSLIter<T> 	iter(list);
		T		*pItem;
		ID		*pId;

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
	T *findDelete (tsSLList<T> &list, const ID &idIn)
	{
		tsSLIterRm<T> 	iter(list);
		T		*pItem;
		ID		*pId;

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
};

//
// Some ID classes that work with the above template
//


//
// unsigned identifier
//
class uintId {
public:
	uintId(unsigned idIn=UINT_MAX) : id(idIn) {}

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
// special resource table which uses 
// unsigned integer keys allocated in chronological sequence
// 
// NOTE: ITEM must public inherit from uintRes<ITEM>
//
template <class ITEM>
class uintResTable : public resTable<ITEM, uintId> {
public:
	uintResTable() : allocId(1u) {} // hashing is faster close to zero

	inline void installItem(ITEM &item);
private:
	unsigned 	allocId;
};

//
// resource with unsigned chronological identifier
//
template <class ITEM>
class uintRes : public uintId, public tsSLNode<ITEM> {
friend class uintResTable<ITEM>;
public:
	uintRes(unsigned idIn=UINT_MAX) : uintId(idIn) {}
};

//
// uintRes<ITEM>::installItem()
//
// NOTE: This detects (and avoids) the case where 
// the PV id wraps around and we attempt to have two 
// resources with the same id.
//
template <class ITEM>
inline void uintResTable<ITEM>::installItem(ITEM &item)
{
	int resTblStatus;
	do {
		item.uintRes<ITEM>::id = allocId++;
		resTblStatus = this->add(item);
	}
	while (resTblStatus);
}

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
// not in stringId static because of problems with
// shareable libraries under win32
//
epicsShareExtern const unsigned char stringIdFastHash[256];

//
// character string identifier
//
// NOTE: to be robust in situations where the new()
// in the constructor might fail a careful consumer 
// of this class should check to see if the 
// stringId::resourceName() below
// returns a valid (non--NULL) string pointer.
// Eventually an exception will be thrown if
// new fails (when this is portable).
//
class stringId {
public:
	enum allocationType {copyString, refString};

	//
	// allocCopyString()
	//
	static inline char * allocCopyString(const char * const pStr)
	{
		char *pNewStr = new char [strlen(pStr)+1u];
		if (pNewStr) {
			strcpy (pNewStr, pStr);
		}
		return pNewStr;
	}

	//
	// stringId() constructor
	//
	// Use typeIn==refString only if the string passed in will exist
	// and remain constant during the entire lifespan of the stringId
	// object.
	//
        stringId (char const * const idIn, allocationType typeIn=copyString) :
		pStr(typeIn==copyString?allocCopyString(idIn):idIn),
		allocType(typeIn) {}

	~ stringId()
	{
		if (this->allocType==copyString) {
			if (this->pStr!=NULL) {
				delete [] (char *) this->pStr;
			}
		}
	}

	//
	// The hash algorithm is a modification of the algorithm described in 
	// Fast Hashing of Variable Length Text Strings, Peter K. Pearson, 
	// Communications of the ACM, June 1990 
	// The modifications were designed by Marty Kraimer
	//
        resTableIndex resourceHash(unsigned nBitsId) const
        {
		if (this->pStr==NULL) {
			return 0u;
		}

		unsigned h0 = 0u;
		unsigned h1 = 0u;
		unsigned c;
		unsigned i;
		for (i=0u; (c = this->pStr[i]); i++) {
			//
			// odd 
			//
			if (i&1u) {
				h1 = stringIdFastHash[h1 ^ c];
			}
			//
			// even 
			//
			else {
				h0 = stringIdFastHash[h0 ^ c];
			}
		}

		//
		// does not work well for more than 65k entries ?
		// (because some indexes in the table will not be produced)
		//
		if (nBitsId>=8u) {
			h1 = h1 << (nBitsId-8u);
		}
		return h1 ^ h0;
        }
 
        int operator == (const stringId &idIn)
        {
		if (this->pStr!=NULL && idIn.pStr!=NULL) {
                	return strcmp(this->pStr,idIn.pStr)==0;
		}
		else {
			return 0u; // not equal
		}
        }

	//
	// return the pointer to the string
	// (also used to test to see if "new()"
	// failed in the constructor
	//
	const char * resourceName()
	{
		return this->pStr;
	}

	void show (unsigned level) const
	{
		if (level>2u) {
			printf ("resource id = %s\n", this->pStr);
		}
	}
private:
        const char * const pStr;
	allocationType const allocType;
};

#endif // INCresourceLibh

