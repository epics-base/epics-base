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
 *
 */

//
// tsSLList<>
//
template <class T>
class tsSLList : public tsSLNode<T> {
public:
	tsSLList () {}

	//
	// first()
	//
	T *first() const
	{
		return this->tsSLNode<T>::pNext; 
	}

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
	// (itemBefore might be the list header object and therefore
	// will not always be of type T)
	//
	void remove (T &item, tsSLNode<T> &itemBefore)
	{
		itemBefore.tsSLNode<T>::pNext = item.tsSLNode<T>::pNext;
	}

	//
	// get ()
	//
	T * get()
	{
		T *pItem = this->tsSLNode<T>::pNext;
		if (pItem) {
			this->remove(*pItem, *this);
		}
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
	// when someone copies int a class deriving from this
	// do _not_ change the node pointers
	//
	void operator = (tsSLNode<T> &) {}

	T *next() {return this->pNext;}
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

	T * operator () () 
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

	tsSLNode<T> * prev () const
	{
		return this->pPrevious;
	}

private:
	T      		*pCurrent;
	tsSLNode<T> 	*pPrevious;
	tsSLList<T>	&list;
};

