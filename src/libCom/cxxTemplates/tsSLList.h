/*
 *      $Id$
 *
 *	 tsSLList - type safe singly linked list templates
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
 * Revision 1.4  1996/09/04 19:57:07  jhill
 * string id resource now copies id
 *
 * Revision 1.3  1996/07/25 18:01:42  jhill
 * use pointer (not ref) for list in iter class
 *
 * Revision 1.2  1996/07/24 22:12:04  jhill
 * added remove() to iter class + made node's prev/next private
 *
 * Revision 1.1.1.1  1996/06/20 22:15:55  jhill
 * installed  ca server templates
 *
 *
 */

#ifndef assert // allows epicsAssert.h 
#include <assert.h>
#endif

//
// tsSLList<>
//
template <class T>
class tsSLList : public tsSLNode<T> {
public:

	//
	// insert()
	// (itemBefore might be the list header object and therefore
	// will not always be of type T)
	//
	void insert (T &item, tsSLNode<T> &itemBefore)
	{
		item.tsSLNode<T>::pNext = itemBefore.pNext;
		itemBefore.pNext = &item;
	}

	//
	// add()
	//
	void add (T &item)
	{
		this->insert (item, *this);
	}

	//
	// remove ()
	// **** removes item after "itemBefore" ****
	// (itemBefore might be the list header object and therefore
	// will not always be of type T)
	//
	void remove (tsSLNode<T> &itemBefore)
	{
		T *pItem = itemBefore.tsSLNode<T>::pNext;
		if (pItem) {
			itemBefore.tsSLNode<T>::pNext = pItem->tsSLNode<T>::pNext;
		}
	}

	//
	// get ()
	//
	T * get()
	{
		T *pItem = this->tsSLNode<T>::pNext;
		this->remove(*this);
		return pItem;
	}

	T * pop()
	{
		return get();
	}

	void push(T &item)
	{
		this->add(item);
	}
};

//
// tsSLNode<>
//
template <class T>
class tsSLNode {
friend class tsSLList<T>;
friend class tsSLIter<T>;
public:
	tsSLNode() : pNext(0) {}

	//
	// when someone copies into a class deriving from this
	// do _not_ change the node pointers
	//
	void operator = (tsSLNode<T> &) {}

private:
	T	*pNext;
};

//
// tsDLFwdIter<T>
//
// Notes:
// 1) No direct access to pCurrent is provided since
//      this might allow for confusion when an item
//      is removed (and pCurrent ends up pointing at
//      an item that has been seen before)
//
template <class T>
class tsSLIter {
public:
	tsSLIter(tsSLList<T> &listIn) :
		pCurrent(0), pPrevious(0), pList(&listIn) {}

	void reset()
	{
		this->pCurrent = 0;
		this->pPrevious = 0;
	}

	void reset (tsSLList<T> &listIn)
	{
		this->pList = &listIn;
		this->reset();
	}

	void operator = (tsSLList<T> &listIn) 
	{
		this->reset(listIn);
	}

	//
	// move iterator forward
	//
	T * next () 
	{
		T *pNewCur;
		if (this->pCurrent) {
			this->pPrevious = this->pCurrent;
		}
		else {
			this->pPrevious = this->pList;
		}
		this->pCurrent = pNewCur = this->pPrevious->pNext;
		return pNewCur;
	}

	//
	// move iterator forward
	//
	T * operator () () 
	{
		return this->next();
	}

	//
	// remove current node
        // (and move current to be the previos item -
	// the item seen by the iterator before the 
	// current one - this guarantee that the list 
	// will be accessed sequentially even if an item
        // is removed)
	//
	// **** NOTE ****
	// This may be called once for each cycle of the 
	// iterator. Attempts to call this twice without
	// moving the iterator forward inbetween the two
	// calls will assert fail
	//
	void remove ()
	{
		if (this->pCurrent) {
			assert(this->pPrevious);
			this->pPrevious->pNext = this->pCurrent->pNext;
			this->pCurrent = this->pPrevious;
			this->pPrevious = 0; 
		}
	}
private:
	tsSLNode<T>	*pCurrent;
	tsSLNode<T> 	*pPrevious;
	tsSLList<T>	*pList;
};

