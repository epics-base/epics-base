/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      $Id$
 *
 *	 tsSLList - type safe singly linked list templates
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef assert // allow use of epicsAssert.h
#include <assert.h>
#endif

//
// the hp compiler complains about parameterized friend
// class that has not been declared without this?
//
template <class T> class tsSLList;
template <class T> class tsSLIter;
template <class T> class tsSLIterRm;

//
// tsSLNode<>
// NOTE: T must derive from tsSLNode<T>
//
template <class T>
class tsSLNode {
friend class tsSLList<T>;
friend class tsSLIter<T>;
friend class tsSLIterRm<T>;
public:
	tsSLNode() : pNext(0) {}

	//
	// when someone copies into a class deriving from this
	// do _not_ change the node pointers
	//
	void operator = (const tsSLNode<T> &) {}

private:
	T	*pNext;

	//
	// removeNextItem ()
	//
	// removes the item after this node
	//
	void removeNextItem ()
	{
		T *pItem = this->pNext;
		if (pItem) {
			tsSLNode<T> *pNode = pItem;
			this->pNext = pNode->pNext;
		}
	}
};


//
// tsSLList<>
// NOTE: T must derive from tsSLNode<T>
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
		tsSLNode<T> &node = item;
		node.pNext = itemBefore.pNext;
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
	// get ()
	//
	T * get()
	{
		tsSLNode<T> *pThisNode = this;
		T *pItem = pThisNode->pNext;
		pThisNode->removeNextItem();
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
// tsSLIter<T>
//
template <class T>
class tsSLIter {
public:
	tsSLIter(const tsSLList<T> &listIn) :
	  pCurrent(0), list(listIn) {};

	//
	// move iterator forward
	//
	// NULL test here is inefficient, but it appears that some architectures
	// (intel) dont like to cast a NULL pointer from a tsSLNode<T> to a T even if
	// tsSLNode<T> is always a base class of a T.
	//
	T * next ();

	//
	// move iterator forward
	//
	T * operator () () 
	{
		return this->next();
	}

private:
	T *pCurrent;
	const tsSLList<T> &list;
};

//
// tsSLIterRm<T>
// (A tsSLIter<T> that allows removing a node)
//
// adds remove method (and does not construct
// with const list)
//
// tsSLIter isnt a base class because this
// requires striping const from pCurrent which could get
// us in trouble with a high quality
// optimizing compiler
//
// Notes:
// 1) No direct access to pCurrent is provided since
//      this might allow for confusion when an item
//      is removed (and pCurrent ends up pointing at
//      an item that has been seen before)
//
//
template <class T>
class tsSLIterRm {
public:
	tsSLIterRm(tsSLList<T> &listIn) :
	  pPrevious(0), pCurrent(0), list(listIn) {};

	//
	// move iterator forward
	//
	// NULL test here is inefficient, but it appears that some architectures
	// (intel) dont like to cast a NULL pointer from a tsSLNode<T> to a T even if
	// tsSLNode<T> is always a base class of a T.
	//
	T * next ();

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
	// This cant be called twice in a row without moving 
	// the iterator to the next item. If there is 
	// no current item this function assert fails.
	//
	void remove ();

private:
	T *pPrevious;
	T *pCurrent;
	tsSLList<T> &list;
};

//
// tsSLIter<T>::next () 
//
// move iterator forward
//
// NULL test here is inefficient, but it appears that some architectures
// (intel) dont like to cast a NULL pointer from a tsSLNode<T> to a T even if
// tsSLNode<T> is always a base class of a T.
//
template <class T>
T * tsSLIter<T>::next () 
{
	if (this->pCurrent!=0) {
		tsSLNode<T> *pCurNode = this->pCurrent;
		this->pCurrent = pCurNode->pNext;
	}
	else {
		const tsSLNode<T> &first = this->list;
		//
		// assume that we are starting (or restarting) at the 
		// beginning of the list
		//
		this->pCurrent = first.pNext;
	}
	return this->pCurrent;
}

//
// tsSLIterRm<T>::next ()
//
// move iterator forward
//
// NULL test here is inefficient, but it appears that some architectures
// (intel) dont like to cast a NULL pointer from a tsSLNode<T> to a T even if
// tsSLNode<T> is always a base class of a T.
//
template <class T>
T * tsSLIterRm<T>::next () 
{
	if (this->pCurrent!=0) {
		tsSLNode<T> *pCurNode = this->pCurrent;
		this->pPrevious = this->pCurrent;
		this->pCurrent = pCurNode->pNext;
	}
	else {
		const tsSLNode<T> &first = this->list;
		//
		// assume that we are starting (or restarting) at the 
		// beginning of the list
		//
		this->pCurrent = first.pNext;
		this->pPrevious = 0;
	}
	return this->pCurrent;
}

//
// tsSLIterRm<T>::remove ()
//
// remove current node
// (and move current to be the previos item -
// the item seen by the iterator before the 
// current one - this guarantee that the list 
// will be accessed sequentially even if an item
// is removed)
//
// This cant be called twice in a row without moving 
// the iterator to the next item. If there is 
// no current item this function assert fails.
//
template <class T>
void tsSLIterRm<T>::remove ()
{
	assert (this->pCurrent!=0);

	tsSLNode<T> *pPrevNode;
	tsSLNode<T> *pCurNode = this->pCurrent;

	if (this->pPrevious==0) {
		pPrevNode = &this->list;
		//
		// this assert fails if it is an attempt to 
		// delete twice without moving the iterator
		//
		assert (pPrevNode->pNext == this->pCurrent);
	}
	else {
		pPrevNode = this->pPrevious;
	}

	pPrevNode->pNext = pCurNode->pNext;
	this->pCurrent = this->pPrevious;
	this->pPrevious = 0; 
}
