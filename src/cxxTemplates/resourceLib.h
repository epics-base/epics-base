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

#include <tsSLList.h>

typedef int 		resLibStatus;
typedef	unsigned 	resTableIndex;

#define resTableIndexBitWidth (sizeof(resTableIndex)*CHAR_BIT)

//
// class T must derive class ID
//
template <class T, class ID>
class resTable {
public:
	enum resTableDelFlag {rtdfDelete, rtdfNoDelete};

	resTable() :
		pTable(0), hashIdMask(0), hashIdNBits(0), nInUse(0) {}

	int init(unsigned nHashTableEntries);

	~resTable() 
	{
		assert (this->nInUse == 0u);

		if (this->pTable) {
			delete [] this->pTable;
		}
	}

	void destroyAllEntries();

	void show (unsigned level);

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

	T *remove (const ID &idIn)
	{
		tsSLList<T> &list = this->pTable[this->hash(idIn)];
		return this->find(list, idIn, rtdfDelete);
	}


	T *lookup (const ID &idIn)
	{
		tsSLList<T> &list = this->pTable[this->hash(idIn)];
		return this->find(list, idIn);
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
	// find
	// searches from where the iterator points to the
	// end of the list for idIn
	//
	// iterator points to the item found upon return
	// (or NULL if nothing matching was found)
	//
	T *find (tsSLList<T> &list, const ID &idIn, 
		resTableDelFlag df=rtdfNoDelete)
	{
		tsSLIter<T> 	iter(list);
		T		*pItem;
		ID		*pId;

		while ( (pItem = iter()) ) {
			pId = pItem;
			if (*pId == idIn) {
				if (df==rtdfDelete) {
					iter.remove();
					this->nInUse--;
				}
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
// resource with unsigned chronological identifier
//
template <class ITEM>
class uintRes : public uintId, public tsSLNode<ITEM> {
friend class uintResTable<ITEM>;
public:
	uintRes(unsigned idIn=UINT_MAX) : uintId(idIn) {}
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
        stringId (char const * const idIn) :
		pStr(new char [strlen(idIn)+1u])
	{
		if (this->pStr!=NULL) {
			strcpy(this->pStr, idIn);
		}
	}

	~ stringId()
	{
		if (this->pStr!=NULL) {
			delete [] this->pStr;
		}
	}
 
        resTableIndex resourceHash(unsigned nBitsId) const
        {
                resTableIndex hashid;
		unsigned i;
 
		if (this->pStr==NULL) {
			return 0u;
		}

                hashid = 0u;
		for (i=0u; this->pStr[i]; i++) {
			hashid += this->pStr[i] * (i+1u);
		}

		hashid = hashid % (1u<<nBitsId);

                return hashid;
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

	const char * resourceName()
	{
		return this->pStr;
	}

	void show (unsigned)
	{
		printf ("resource id = %s\n", this->pStr);
	}
private:
        char * const pStr;
};

#if defined(__SUNPRO_CC) || defined(EXPAND_TEMPLATES_HERE)
# 	include "resourceLib.cc"
#endif

#endif // INCresourceLibh

