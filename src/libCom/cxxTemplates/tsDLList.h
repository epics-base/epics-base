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
 * Revision 1.3  1996/07/25 18:01:41  jhill
 * use pointer (not ref) for list in iter class
 *
 * Revision 1.2  1996/07/24 22:12:03  jhill
 * added remove() to iter class + made node's prev/next private
 *
 * Revision 1.1.1.1  1996/06/20 22:15:55  jhill
 * installed  ca server templates
 *
 *
 */

#ifndef tsDLListH_include
#define tsDLListH_include

//
// tsDLList<T>
//
template <class T>
class tsDLList {
friend class tsDLIter<T>;
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
	// installs items in addList on the list 
	// (and removes all items from addList)
	//
	tsDLList (tsDLList<T> &addList)  
	{
		*this = addList;
		addList.clear();	
	}

	//
	// count()
	// (returns the number of items on the list)
	//
	unsigned count() const
	{
		return this->itemCount; 
	}

	//
	// add() - 
	// adds addList to the end of the list
	// (and removes all items from addList)
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
	// add() 
	// (add an item to the end of the list)
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

	//
	// insertAfter()
	// (place item in the list immediately after itemBefore)
	//
	void insertAfter (T &item, T &itemBefore)
	{
		item.tsDLNode<T>::pPrev = &itemBefore;
		item.tsDLNode<T>::pNext = itemBefore.tsDLNode<T>::pNext;
		itemBefore.tsDLNode<T>::pNext = &item;

		if (item.tsDLNode<T>::pNext) {
			item.tsDLNode<T>::pNext->tsDLNode<T>::pPrev = &item;
		}
		else {
			this->pLast = &item;
		}

		this->itemCount++;
	}

	//
	// insertBefore ()
	// (place item in the list immediately before itemAfter)
	//
	void insertBefore (T &item, T &itemAfter)
	{
		item.tsDLNode<T>::pNext = &itemAfter;
		item.tsDLNode<T>::pPrev = itemAfter.tsDLNode<T>::pPrev;
		itemAfter.tsDLNode<T>::pPrev = &item;

		if (item.tsDLNode<T>::pPrev) {
			item.tsDLNode<T>::pPrev->tsDLNode<T>::pNext = &item;
		}
		else {
			this->pFirst = &item;
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
	// pop ()
	// (returns the first item on the list)
	T * pop()
	{
		return this->get();
	}

	//
	// push ()
	// (add an item at the beginning of the list)
	//
	void push (T &item)
	{
		item.tsDLNode<T>::pPrev = 0;
		item.tsDLNode<T>::pNext = this->pFirst;

		if (this->itemCount) {
			this->pFirst->tsDLNode<T>::pPrev = &item;
		}
		else {
			this->pLast = &item;
		}

		this->pFirst = &item;

		this->itemCount++;
	}
	
	//
	// find 
	// returns -1 if the item isnt on the list
	// and the node number (beginning with zero if
	// it is)
	//
	int find(T &item);

	T *first(void) const { return pFirst; }

protected:
	T		*getFirst(void) const { return pFirst; }
	T		*getLast(void) const { return pLast; }
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
	// when someone copies in a class deriving from this
	// do _not_ change the node pointers
	//
	void operator = (tsDLNode<T> &) {}

protected:
	T	*getNext(void) { return pNext; }
	T	*getPrev(void) { return pPrev; }
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
		pList(&listIn), pCurrent(0) {}

	void reset ()
	{
		this->pCurrent = 0;
	}

	void reset (tsDLList<T> &listIn)
	{
		this->reset();
		this->pList = &listIn;
	}

	void operator = (tsDLList<T> &listIn) 
	{
		this->reset(listIn);
	}

	T * current () 
	{
		return this->pCurrent;
	}

	T * prev () 
	{
		T *pCur = this->pCurrent;
		if (pCur==0) {
			pCur = this->pList->pLast;
		}
		else {
			pCur = pCur->tsDLNode<T>::pPrev;
		}
		this->pCurrent = pCur;
		return pCur;
	}

	T * next () 
	{
		T *pCur = this->pCurrent;
		if (pCur==0) {
			pCur = this->pList->pFirst;
		}
		else {
			pCur = pCur->tsDLNode<T>::pNext;
		}
		this->pCurrent = pCur;
		return pCur;
	}

	T * first()
	{
		this->pCurrent = this->pList->pFirst;
		return this->pCurrent;
	}

	T * last()
	{
		this->pCurrent = this->pList->pLast;
		return this->pCurrent;
	}

	T * operator () () 
	{
		return this->next();
	}

	//
	// remove ()
	// remove current item 
	// (and move current to be the next item
	// in the list)
	//
	void remove ()
	{
		T *pCur = this->pCurrent;
		if (pCur) {
			//
			// move current to the next item
			//
			this->next();
			//
			// delete current item
			//
			this->pList->remove(*pCur);
		}
	}
protected:
	tsDLList<T> *getList() { return pList; }
private:
	tsDLList<T>	*pList;
	T      		*pCurrent;
};

//
// find 
// returns -1 if the item isnt on the list
// and the node number (beginning with zero if
// it is)
//
template <class T>
inline int tsDLList<T>::find(T &item)
{
	tsDLIter<T>	iter(*this);
	tsDLNode<T>	*pItem;
	int		itemNo=0;

	while ( (pItem = iter()) ) {
		if (pItem == &item) {
			return itemNo;
		}
		itemNo++;
	}
	return -1;
}

#endif // tsDLListH_include

