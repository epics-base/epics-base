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
 * Revision 1.12  1998/06/16 03:03:07  jhill
 * eliminated dangling ptr warning from bounds checker
 *
 * Revision 1.11  1998/05/05 18:06:58  jhill
 * rearranged to allow compilation by g++ 2.8.1
 *
 * Revision 1.10  1998/02/18 22:53:13  jhill
 * fixed gnu warning
 *
 * Revision 1.9  1998/02/05 23:28:21  jhill
 * fixed hp sompiler warnings
 *
 * Revision 1.8  1997/06/13 09:21:53  jhill
 * fixed compiler compatibility problems
 *
 * Revision 1.7  1997/04/10 19:43:10  jhill
 * API changes
 *
 * Revision 1.6  1997/01/22 21:14:21  jhill
 * fixed class decl order for VMS
 *
 * Revision 1.5  1996/11/02 01:07:20  jhill
 * many improvements
 *
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
	// remove ()
	// **** removes item after "itemBefore" ****
	// (itemBefore might be the list header object and therefore
	// will not always be of type T)
	//
	void remove (tsSLNode<T> &itemBefore)
	{
		T *pItem = itemBefore.pNext;
		if (pItem) {
			tsSLNode<T> *pNode = pItem;
			itemBefore.pNext = pNode->pNext;
		}
	}

	//
	// get ()
	//
	T * get()
	{
		tsSLNode<T> *pThisNode = this;
		T *pItem = pThisNode->pNext;
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
