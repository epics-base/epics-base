/*
 *      $Id$
 *
 *      type safe singly linked list templates
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
 */

#ifndef tsSLListh
#define tsSLListh

#include <assert.h>

//
// the hp compiler complains about parameterized friend
// class that has not been declared without this?
//
template <class T> class tsSLList;
template <class T> class tsSLIter;
template <class T> class tsSLIterRm;

//
// tsSLNode<T>
// NOTE: class T must derive from tsSLNode<T>
//
template <class T>
class tsSLNode {
friend class tsSLList<T>;
friend class tsSLIter<T>;
friend class tsSLIterRm<T>;
public:

	tsSLNode ();

	void operator = (const tsSLNode<T> &) const;

private:

	void removeNextItem ();	// removes the item after this node

	T	*pNext;
};


//
// tsSLList<T>
// NOTE: class T must derive from tsSLNode<T>
//
template <class T>
class tsSLList : public tsSLNode<T> {
public:
    tsSLList (); // creates an empty list

	void insert (T &item, tsSLNode<T> &itemBefore); // insert after item before

	void add (T &item); // add to the beginning of the list

	T * get (); // remove from the beginning of the list

	T * pop (); // same as get

	void push (T &item); // same as add
private:
    tsSLList (const tsSLList &); // intentionally _not_ implemented
};

//
// tsSLIter<T>
//
template <class T>
class tsSLIter {
public:
	tsSLIter (const tsSLList<T> &listIn);

	T * next (); // move iterator forward

	T * operator () (); // same as next ()

private:
	T *pCurrent;
	const tsSLList<T> *pList; // ptr allows cpy op
};

//
// tsSLIterRm<T>
// (An tsSLIter<T> that allows removing a node)
//
template <class T>
class tsSLIterRm {
public:

	//
	// exceptions
	//
	class noCurrentItemInIterator {};

	tsSLIterRm (tsSLList<T> &listIn);

	T * next (); // move iterator forward

	T * operator () (); // same as next ()

	void remove (); // remove current node

private:
	T *pPrevious;
	T *pCurrent;
	tsSLList<T> *pList; // ptr allows cpy op
};

//////////////////////////////////////////
//
// tsSLNode<T> inline member functions
//
//////////////////////////////////////////

//
// tsSLNode<T>::tsSLNode
//
template <class T>
tsSLNode<T>::tsSLNode() : pNext(0) {}

//
// tsSLNode<T>::operator =
//
// when someone copies into a class deriving from this
// do _not_ change the node pointers
//
template <class T>
inline void tsSLNode<T>::operator = (const tsSLNode<T> &) const {}

//
// removeNextItem ()
//
// removes the item after this node
//
template <class T>
inline void tsSLNode<T>::removeNextItem ()
{
	T *pItem = this->pNext;
	if (pItem) {
		tsSLNode<T> *pNode = pItem;
		this->pNext = pNode->pNext;
	}
}

//////////////////////////////////////////
//
// tsSLList<T> inline memeber functions
//
//////////////////////////////////////////

//
// tsSLList<T>::tsSLList()
// create an empty list
//
template <class T>
inline tsSLList<T>::tsSLList ()
{
}

//
// tsSLList<T>::insert()
// (itemBefore might be the list header object and therefore
// will not always be of type T)
//
template <class T>
inline void tsSLList<T>::insert (T &item, tsSLNode<T> &itemBefore)
{
	tsSLNode<T> &node = item;
	node.pNext = itemBefore.pNext;
	itemBefore.pNext = &item;
}

//
// tsSLList<T>::add ()
//
template <class T>
inline void tsSLList<T>::add (T &item)
{
	this->insert (item, *this);
}

//
// tsSLList<T>::get ()
//
template <class T>
inline T * tsSLList<T>::get()
{
	tsSLNode<T> *pThisNode = this;
	T *pItem = pThisNode->pNext;
	pThisNode->removeNextItem();
	return pItem;
}

//
// tsSLList<T>::pop ()
//
template <class T>
inline T * tsSLList<T>::pop()
{
	return this->get();
}

//
// tsSLList<T>::push ()
//
template <class T>
inline void tsSLList<T>::push(T &item)
{
	this->add(item);
}

//////////////////////////////////////////
//
// tsSLIter<T> inline memeber functions
//
//////////////////////////////////////////

//
// tsSLIter<T>::tsSLIter
//
template <class T>
inline tsSLIter<T>::tsSLIter (const tsSLList<T> &listIn) :
  pCurrent (0), pList (&listIn) {}

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
		const tsSLNode<T> &first = *this->pList;
		//
		// assume that we are starting (or restarting) at the 
		// beginning of the list
		//
		this->pCurrent = first.pNext;
	}
	return this->pCurrent;
}

//
// move iterator forward
//
template <class T>
inline T * tsSLIter<T>::operator () () 
{
	return this->next();
}

//////////////////////////////////////////
//
// tsSLIterRm<T> inline memeber functions
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
//////////////////////////////////////////

//
// tsSLIterRm<T>::tsSLIterRm ()
//
template <class T>
inline tsSLIterRm<T>::tsSLIterRm (tsSLList<T> &listIn) :
  pPrevious (0), pCurrent (0), pList (&listIn) {}


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
		const tsSLNode<T> &first = *this->pList;
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
// move iterator forward
//
template <class T>
inline T * tsSLIterRm<T>::operator () () 
{
	return this->next();
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
// no current item this function throws an exception.
//
template <class T>
void tsSLIterRm<T>::remove ()
{
	if (this->pCurrent==0) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else            
            throw noCurrentItemInIterator ();
#       endif
	}

	tsSLNode<T> *pPrevNode;
	tsSLNode<T> *pCurNode = this->pCurrent;

	if (this->pPrevious==0) {
		pPrevNode = this->pList;
		//
		// fail if it is an attempt to 
		// delete twice without moving the iterator
		//
		if (pPrevNode->pNext != this->pCurrent) {
#           ifdef noExceptionsFromCXX
                assert (0);
#           else            
                throw noCurrentItemInIterator ();
#           endif
		}
	}
	else {
		pPrevNode = this->pPrevious;
	}

	pPrevNode->pNext = pCurNode->pNext;
	this->pCurrent = this->pPrevious;
	this->pPrevious = 0; 
}

#endif // tsSLListh
