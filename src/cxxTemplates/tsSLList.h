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
 * Revision 1.2  1996/07/24 22:12:04  jhill
 * added remove() to iter class + made node's prev/next private
 *
 * Revision 1.1.1.1  1996/06/20 22:15:55  jhill
 * installed  ca server templates
 *
 *
 */

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

template <class T>
class tsSLIter {
public:
	tsSLIter(tsSLList<T> &listIn) :
		pList(&listIn), pCurrent(0), pPrevious(0) {}

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

	T * current () 
	{
		return this->pCurrent;
	}

	T * next () 
	{
		if (this->pCurrent) {
			this->pPrevious = this->pCurrent;
		}
		else {
			this->pPrevious = this->pList;
		}
		this->pCurrent = this->pPrevious->pNext; 
		return this->pCurrent;
	}

	// this should move current?
	//tsSLNode<T> * prev () const
	//{
	//	return this->pPrevious;
	//}

	T * operator () () 
	{
		return this->next();
	}

	//
	// remove current node
	//
	void remove ()
	{
		if (this->pCurrent) {
			this->pCurrent = 
				this->pCurrent->tsSLNode<T>::pNext; 
			this->pPrevious->pNext = this->pCurrent;
		}
	}
private:
	T      		*pCurrent;
	tsSLNode<T> 	*pPrevious;
	tsSLList<T>	*pList;
};

