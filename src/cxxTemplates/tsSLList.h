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
		pCurrent(0), pPrevious(0), list(listIn) {}

	void operator = (tsSLList<T> &listIn) 
	{
		list = listIn;
		pCurrent = 0;
		pPrevious = 0;
	}

	T * current () 
	{
		return this->pCurrent;
	}

	T * next () 
	{
		tsSLNode<T> *pPrev = this->pCurrent;
		T *pCur;
		if (pPrev==0) {
			pPrev = &this->list;
		}
		this->pPrevious = pPrev;
		pCur = pPrev->pNext;
		this->pCurrent = pCur; 
		return pCur;
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
	tsSLList<T>	&list;
};

