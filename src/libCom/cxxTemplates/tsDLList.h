/*
 *      $Id$
 *
 *	type safe doubly linked list templates
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
 */

#ifndef tsDLListH_include
#define tsDLListH_include

//
// tsDLList<T>
//
template <class T>
class tsDLList {
private:
	//
	// clear()
	//
	void clear()
	{
		this->pFirst = 0;
		this->pLast = 0;
		this->itemCount = 0u;
	}
public:
	tsDLList () 
	{
		this->clear();
	}

	//
	// adds addList to this (and removes all items from addList)
	//
	tsDLList (tsDLList<T> &addList)  
	{
		*this = addList;
		addList.clear();	
	}

	//
	// first()
	//
	T *first() const
	{
		return (T *) this->pFirst; 
	}

	//
	// last()
	//
	T *last() const
	{
		return (T *) this->pLast; 
	}

	//
	// count()
	//
	unsigned count() const
	{
		return this->itemCount; 
	}

	//
	// add() - 
	// adds addList to this (and removes all items from addList)
	//
	void add (tsDLList<T> &addList)
	{
		//
		// NOOP if addList is empty
		//
		if (addList.itemCount==0u) {
			return;
		}

		if (this->itemCount==0u) {
			//
			// this is empty so just init from 
			// addList 
			//
			*this = addList;
		}
		else {
			//
			// add addList to the end of this
			//
			this->pLast->tsDLNode<T>::pNext = addList.pFirst;
			addList.pFirst->tsDLNode<T>::pPrev = addList.pLast;
			this->pLast = addList.pLast;
			this->itemCount += addList.itemCount;
		}

		//
		// leave addList empty
		//
		addList.clear();
	}


	//
	// add() - an item
	//
	void add (T &item)
	{
		item.tsDLNode<T>::pNext = 0;
		item.tsDLNode<T>::pPrev = this->pLast;

		if (this->itemCount) {
			this->pLast->tsDLNode<T>::pNext = &item;
		}
		else {
			this->pFirst = &item;
		}

		this->pLast = &item;

		this->itemCount++;
	}

	//
	// insert()
	// pItemBefore==0 => make item be the first item on the list
	// pItemBefore!=0 => place item in the list immediately after
	//			pItemBefore
	//
	void insert (T &item, T *pItemBefore=0)
	{
		if (pItemBefore) {
			item.tsDLNode<T>::pPrev = pItemBefore;
			item.tsDLNode<T>::pNext = 
				pItemBefore->tsDLNode<T>::pNext;
			pItemBefore->tsDLNode<T>::pNext = &item;
		}
		else {
			item.tsDLNode<T>::pPrev = 0;
			item.tsDLNode<T>::pNext = this->pFirst;
			this->pFirst = &item;
		}

		if (item.tsDLNode<T>::pNext) {
			item.tsDLNode<T>::pNext->tsDLNode<T>::pPrev = &item;
		}
		else {
			this->pLast = &item;
		}

		this->itemCount++;
	}

	//
	// remove ()
	//
	void remove (T &item)
	{
		if (this->pLast == &item) {
			this->pLast = item.tsDLNode<T>::pPrev;
		}
		else {
			item.tsDLNode<T>::pNext->tsDLNode<T>::pPrev = 
				item.tsDLNode<T>::pPrev; 
		}

		if (this->pFirst == &item) {
			this->pFirst = item.tsDLNode<T>::pNext;
		}
		else {
			item.tsDLNode<T>::pPrev->tsDLNode<T>::pNext =
				item.tsDLNode<T>::pNext;
		}
		
		this->itemCount--;
	}

	//
	// get ()
	//
	T * get()
	{
		T *pItem = this->pFirst;

		if (pItem) {
			this->remove (*pItem);
		}
		
		return pItem;
	}

private:
	T		*pFirst;
	T		*pLast;
	unsigned	itemCount;
};

//
// tsDLNode<T>
//
template <class T>
class tsDLNode {
friend class tsDLList<T>;
friend class tsDLIter<T>;
public:
	tsDLNode() : pNext(0), pPrev(0) {}
	//
	// when someone copies int a class deriving from this
	// do _not_ change the node pointers
	//
	void operator = (tsDLNode<T> &) {}
	
	T *getPrev() {return this->pPrev;}
	T *getNext() {return this->pNext;}
private:
	T	*pNext;
	T	*pPrev;
};

//
// tsDLIter<T>
//
template <class T>
class tsDLIter {
public:
	tsDLIter(tsDLList<T> &listIn) : 
		list(listIn), pCurrent(0) {}

	void reset ()
	{
		this->pCurrent = 0;
	}

	void reset (tsDLList<T> &listIn)
	{
		this->reset();
		this->list = listIn;
	}

	void operator = (tsDLList<T> &listIn) 
	{
		this->reset(listIn);
	}

	T * operator () () 
	{
		T *pCur = this->pCurrent;
		if (pCur==0) {
			pCur = this->list.first();
		}
		else {
			pCur = pCur->tsDLNode<T>::pNext;
		}
		this->pCurrent = pCur;
		return pCur;
	}
private:
	tsDLList<T>	&list;
	T      		*pCurrent;
};

#endif // tsDLListH_include

