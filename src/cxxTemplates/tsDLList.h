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
 */

#ifndef tsDLListH_include
#define tsDLListH_include

template <class T> class tsDLList;
template <class T> class tsDLIter;
template <class T> class tsDLFwdIter;
template <class T> class tsDLBwdIter;
template <class T> class tsDLIterBD;

//
// class tsDLNode<T>
//
// a node in a doubly linked list
//
// NOTE: class T must derive from tsDLNode<T>
//
template <class T>
class tsDLNode {
    friend class tsDLList<T>;
    friend class tsDLIter<T>;
    friend class tsDLFwdIter<T>;
    friend class tsDLBwdIter<T>;
    friend class tsDLIterBD<T>;
public:
	tsDLNode();
	void operator = (const tsDLNode<T> &) const;
private:
	T	*pNext;
	T	*pPrev;
//protected:
//	T	*getNext(void) const;
//	T	*getPrev(void) const;
};

//
// class tsDLList<T>
//
// a doubly linked list
//
// NOTE: class T must derive from tsDLNode<T>
//
template <class T>
class tsDLList {
    friend class tsDLIter<T>;
    friend class tsDLFwdIter<T>;
    friend class tsDLBwdIter<T>;
public:

	tsDLList (); // create empty list

	unsigned count () const; // number of items on list

	void add (T &item); // add item to end of list

    // all Ts in addList added to end of list
    // (addList left empty)
	void add (tsDLList<T> &addList);

	void push (T &item); // add item to beginning of list

	// remove item from list
	void remove (T &item);

	T * get (); // removes first item on list
	T * pop (); // same as get ()

	// insert item in the list immediately after itemBefore
	void insertAfter (T &item, T &itemBefore);

	// insert item in the list immediately before itemAfter)
	void insertBefore (T &item, T &itemAfter);
	
	//
	// returns -1 if the item isnt on the list and the node 
    // number (beginning with zero if it is)
	//
	int find (T &item) const;

	T *first (void) const; // ptr to first item on list
	T *last (void) const; // ptr to last item on list

private:
	T           *pFirst;
	T           *pLast;
	unsigned    itemCount;

	//
	// create empty list 
    // (throw away any knowledge of current list)
	//
	void clear (); 

	//
	// copying one list item into another and
	// ending up with to list headers pointing
	// at the same list is always a questionable
	// thing to do.
	//
	// therefore, this is intentionally private
    // and _not_ implemented.
	//
	tsDLList (const tsDLList &);
};

//
// tsDLIter<T>
//
// doubly linked list iterator
//
// Notes:
// 1) 
//  This iterator does not allow for removal
//	of an item in order to avoid problems 
//	resulting when we remove an item (and 
//	then dont know whether to make pCurrent 
//	point at the item before or after the 
//	item removed
//
template <class T>
class tsDLIter {
public:
	tsDLIter (tsDLList<T> & listIn);
	void reset ();
	void reset (tsDLList<T> &listIn);
	void operator = (tsDLList<T> &listIn);
	T * next ();
	T * prev ();
	T * first();
	T * last();
	T * operator () ();

protected:
	T       *pCurrent;
	tsDLList<T> *pList;
};

//
// class tsDLFwdIter<T>
//
// Notes:
// 1) No direct access to pCurrent is provided since
// 	this might allow for confusion when an item
//	is removed (and pCurrent ends up pointing at
//	an item that has been seen before)
//
// 2) This iterator only moves forward in order to 
//	avoid problems resulting when we remove an
//	item (and then dont know whether to make 
//	pCurrent point at the item before or after
// 	the item removed
//
template <class T>
class tsDLFwdIter: private tsDLIter<T> {
public:
	tsDLFwdIter (tsDLList<T> &listIn);
	void reset ();
	void reset (tsDLList<T> &listIn);
	void operator = (tsDLList<T> &listIn);
	T * operator () ();
	T * next ();
	T * first();

	//
	// remove ()
	// (and move current to be the item
	// pointed to by pPrev - the item seen
	// by the iterator before the current one -
	// this guarantee that the list will be
	// accessed sequentially even if an item
	// is removed)
	//
	void remove ();
};

//
// tsDLBwdIter<T>
//
// Notes:
// 1) No direct access to pCurrent is provided since
// 	this might allow for confusion when an item
//	is removed (and pCurrent ends up pointing at
//	an item that has been seen before)
//
// 2) This iterator only moves backward in order to 
//	avoid problems resulting when we remove an
//	item (and then dont know whether to make 
//	pCurrent point at the item before or after
// 	the item removed
//
template <class T>
class tsDLBwdIter : private tsDLIter<T> {
public:
	tsDLBwdIter (tsDLList<T> &listIn);
	void reset ();
	void reset (tsDLList<T> &listIn);
	void operator = (tsDLList<T> &listIn);
	T * operator () ();
	T * prev ();
	T * last();
	//
	// remove ()
	// remove current item 
	// (and move current to be the item
	// pointed to by pNext - the item seen
	// by the iterator before the current one -
	// this guarantee that the list will be
	// accessed sequentially even if an item
	// is removed)
	//
	void remove ();
};


//
// class tsDLIterBD<T>
//
// bi-directional doubly linked list iterator
//
template <class T>
class tsDLIterBD {
public:
	tsDLIterBD (T *pInitialEntry);

	tsDLIterBD<T> operator = (T *pNewEntry);

	bool operator == (const tsDLIterBD<T> &rhs) const;
	bool operator != (const tsDLIterBD<T> &rhs) const;

	T & operator * () const;
	T * operator -> () const;
	operator T* () const;

	tsDLIterBD<T> operator ++ (); // prefix ++
	tsDLIterBD<T> operator ++ (int); // postfix ++
	tsDLIterBD<T> operator -- (); // prefix --
	tsDLIterBD<T> operator -- (int); // postfix -- 

#	if defined(_MSC_VER) && _MSC_VER < 1200
		tsDLIterBD (const class tsDLIterBD<T> &copyIn);
#	endif

    //
    // end of the list constant
    //
    static const tsDLIterBD<T> eol ();

private:
	T *pEntry;
};

///////////////////////////////////
// tsDLNode<T> member functions
///////////////////////////////////

//
// tsDLNode<T>::tsDLNode ()
//
template <class T>
inline tsDLNode<T>::tsDLNode() : pNext(0), pPrev(0) {}

//
// tsDLNode<T>::operator = ()
//
// when someone copies in a class deriving from this
// do _not_ change the node pointers
//
template <class T>
inline void tsDLNode<T>::operator = (const tsDLNode<T> &) const {}

//template <class T>
//T * tsDLNode<T>::getNext (void) const
//{ 
//    return pNext; 
//}

//template <class T>
//T * tsDLNode<T>::getPrev (void) const
//{ 
//    return pPrev; 
//}

//////////////////////////////////////
// tsDLList<T> member functions
//////////////////////////////////////

//
// tsDLList<T>::tsDLList ()
//
template <class T>
inline tsDLList<T>::tsDLList () 
{
	this->clear ();
}

//
// tsDLList<T>::count ()
//
// (returns the number of items on the list)
//
template <class T>
inline unsigned tsDLList<T>::count () const
{
	return this->itemCount; 
}

//
// tsDLList<T>::first ()
//
template <class T>
inline T * tsDLList<T>::first (void) const 
{
    return this->pFirst; 
}

//
// tsDLList<T>::last ()
//
template <class T>
inline T *tsDLList<T>::last (void) const 
{ 
    return this->pLast; 
}

//
// tsDLList<T>::clear ()
//
template <class T>
inline void tsDLList<T>::clear ()
{
	this->pFirst = 0;
	this->pLast = 0;
	this->itemCount = 0u;
}

//
// tsDLList<T>::remove ()
//
template <class T>
void tsDLList<T>::remove (T &item)
{
	tsDLNode<T> &theNode = item;

	if (this->pLast == &item) {
		this->pLast = theNode.pPrev;
	}
	else {
		tsDLNode<T> &nextNode = *theNode.pNext;
		nextNode.pPrev = theNode.pPrev; 
	}

	if (this->pFirst == &item) {
		this->pFirst = theNode.pNext;
	}
	else {
		tsDLNode<T> &prevNode = *theNode.pPrev;
		prevNode.pNext = theNode.pNext;
	}
	
	this->itemCount--;
}

//
// tsDLList<T>::get ()
//
template <class T>
inline T * tsDLList<T>::get()
{
	T *pItem = this->pFirst;

	if (pItem) {
		this->remove (*pItem);
	}
	
	return pItem;
}

//
// tsDLList<T>::pop ()
//
// (returns the first item on the list)
template <class T>
inline T * tsDLList<T>::pop()
{
	return this->get();
}

//
// tsDLList<T>::add ()
// 
// adds addList to the end of the list
// (and removes all items from addList)
//
template <class T>
void tsDLList<T>::add (tsDLList<T> &addList)
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
		tsDLNode<T> *pLastNode = this->pLast;
		tsDLNode<T> *pAddListFirstNode = addList.pFirst;

		//
		// add addList to the end of this
		//
		pLastNode->pNext = addList.pFirst;
		pAddListFirstNode->pPrev = addList.pLast;
		this->pLast = addList.pLast;
		this->itemCount += addList.itemCount;
	}

	//
	// leave addList empty
	//
	addList.clear();
}

//
// tsDLList<T>::add ()
//
// add an item to the end of the list
//
template <class T>
void tsDLList<T>::add (T &item)
{
    tsDLNode<T> &theNode = item;

	theNode.pNext = 0;
	theNode.pPrev = this->pLast;

	if (this->itemCount) {
		tsDLNode<T> &lastNode = *this->pLast;
		lastNode.pNext = &item;
	}
	else {
		this->pFirst = &item;
	}

	this->pLast = &item;

	this->itemCount++;
}

//
// tsDLList<T>::insertAfter ()
//
// place item in the list immediately after itemBefore
//
template <class T>
void tsDLList<T>::insertAfter (T &item, T &itemBefore)
{
	tsDLNode<T> &nodeItem = item;
	tsDLNode<T> &nodeBefore = itemBefore;

	nodeItem.pPrev = &itemBefore;
	nodeItem.pNext = nodeBefore.pNext;
	nodeBefore.pNext = &item;

	if (nodeItem.pNext) {
		tsDLNode<T> *pNextNode = nodeItem.pNext;
		pNextNode->pPrev = &item;
	}
	else {
		this->pLast = &item;
	}

	this->itemCount++;
}

//
// tsDLList<T>::insertBefore ()
//
// place item in the list immediately before itemAfter
//
template <class T>
void tsDLList<T>::insertBefore (T &item, T &itemAfter)
{
	tsDLNode<T> &node = item;
	tsDLNode<T> &nodeAfter = itemAfter;

	node.pNext = &itemAfter;
	node.pPrev = nodeAfter.pPrev;
	nodeAfter.pPrev = &item;

	if (node.pPrev) {
		tsDLNode<T> &prevNode = *node.pPrev;
		prevNode.pNext = &item;
	}
	else {
		this->pFirst = &item;
	}

	this->itemCount++;
}

//
// tsDLList<T>::push ()
//
// add an item at the beginning of the list
//
template <class T>
void tsDLList<T>::push (T &item)
{
	tsDLNode<T> &theNode = item;
	theNode.pPrev = 0;
	theNode.pNext = this->pFirst;

	if (this->itemCount) {
		tsDLNode<T> *pFirstNode = this->pFirst;
		pFirstNode->pPrev = &item;
	}
	else {
		this->pLast = &item;
	}

	this->pFirst = &item;

	this->itemCount++;
}

//
// tsDLList<T>::find () 
// returns -1 if the item isnt on the list
// and the node number (beginning with zero if
// it is)
//
template <class T>
int tsDLList<T>::find (T &item) const
{
	tsDLFwdIter<T> iter (*this);
	tsDLNode<T>	*pItem;
	int		itemNo=0;

	while ( (pItem = iter.next()) ) {
		if (pItem == &item) {
			return itemNo;
		}
		itemNo++;
	}
	return -1;
}

//////////////////////////////////////////
// tsDLIterBD<T> member functions
//////////////////////////////////////////

template <class T>
inline tsDLIterBD<T>::tsDLIterBD (T * pInitialEntry) : 
	pEntry(pInitialEntry) {}

//
// This is apparently required by some compiler, but
// only causes trouble with MS Visual C 6.0. This 
// should not be required by any compiler. I am assuming 
// that this "some compiler" is a past version of MS 
// Visual C.
//	
#	if defined(_MSC_VER) && _MSC_VER < 1200
	template <class T>
	tsDLIterBD<T>::tsDLIterBD (const class tsDLIterBD<T> &copyIn) :
		pEntry(copyIn.pEntry) {}
#	endif

template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator = (T *pNewEntry)
{
	this->pEntry = pNewEntry;
	return *this;
}

template <class T>
inline bool tsDLIterBD<T>::operator == (const tsDLIterBD<T> &rhs) const
{
	return (this->pEntry == rhs.pEntry);
}

template <class T>
inline bool tsDLIterBD<T>::operator != (const tsDLIterBD<T> &rhs) const
{
	return (this->pEntry != rhs.pEntry);
}

template <class T>
inline T & tsDLIterBD<T>::operator * () const
{
	return *this->pEntry;
}

template <class T>
inline T * tsDLIterBD<T>::operator -> () const
{
	return this->pEntry;
}

template <class T>
inline tsDLIterBD<T>::operator T* () const
{
	return this->pEntry;
}

//
// prefix ++
//
template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator ++ () 
{
	tsDLNode<T> &node = *this->pEntry;
	this->pEntry = node.pNext;
    return *this;
}

//
// postfix ++
//
template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator ++ (int) 
{
	tsDLIterBD<T> tmp = *this;
	tsDLNode<T> &node = *this->pEntry;
	this->pEntry = node.pNext;
	return tmp;
}

//
// prefix -- 
//
template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator -- () 
{
	tsDLNode<T> &entryNode = *this->pEntry;
	this->pEntry = entryNode.pPrev;
    return *this;
}

//
// postfix -- 
//
template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator -- (int) 
{
	tsDLIterBD<T> tmp = *this;
	tsDLNode<T> &entryNode = *this->pEntry;
	this->pEntry = entryNode.pPrev;
	return tmp;
}

//
// tsDLIterBD<T>::eol
//
template <class T>
inline const tsDLIterBD<T> tsDLIterBD<T>::eol ()
{
    return tsDLIterBD<T>(0);
}

//////////////////////////////////////////
// tsDLIter<T> member functions
//////////////////////////////////////////

template <class T> 
inline tsDLIter<T>::tsDLIter (tsDLList<T> & listIn) : 
	pCurrent(0), pList(&listIn) {}

template <class T> 
inline void tsDLIter<T>::reset ()
{
	this->pCurrent = 0;
}

template <class T> 
inline void tsDLIter<T>::reset (tsDLList<T> &listIn)
{
	this->reset();
	this->pList = &listIn;
}

template <class T> 
inline void tsDLIter<T>::operator = (tsDLList<T> &listIn)
{
	this->reset(listIn);
}

template <class T> 
inline T * tsDLIter<T>::first()
{
	this->pCurrent = this->pList->pFirst;
	return this->pCurrent;
}

template <class T> 
inline T * tsDLIter<T>::last()
{
	this->pCurrent = this->pList->pLast;
	return this->pCurrent;
}

//
// tsDLIter<T>::next ()
//
template <class T> 
inline T * tsDLIter<T>::next () 
{
	T *pCur = this->pCurrent;
	if (pCur==0) {
		pCur = this->pList->pFirst;
	}
	else {
		tsDLNode<T> *pCurNode = pCur;
		pCur = pCurNode->pNext;
	}
	this->pCurrent = pCur;
	return pCur;
}

//
// tsDLIter<T>::prev ()
//
template <class T>
inline T * tsDLIter<T>::prev ()
{
	T *pCur = this->pCurrent;
	if (pCur==0) {
		pCur = this->pList->pLast;
	}
	else {
		tsDLNode<T> *pCurNode = pCur;
		pCur = pCurNode->pPrev;
	}
	this->pCurrent = pCur;
	return pCur;
}

//
// tsDLIter<T>::operator () ()
//
template <class T>
inline T * tsDLIter<T>::operator () () 
{
	return this->next();
}

///////////////////////////////////////////
// tsDLBwdIter<T> member functions
///////////////////////////////////////////

template <class T>
inline tsDLBwdIter<T>::tsDLBwdIter(tsDLList<T> &listIn) : 
	tsDLIter<T>(listIn) {}

template <class T>
inline void tsDLBwdIter<T>::reset ()
{
	this->tsDLIter<T>::reset();
}

template <class T>
inline void tsDLBwdIter<T>::reset (tsDLList<T> &listIn)
{
	this->tsDLIter<T>::reset(listIn);
}

template <class T>
inline void tsDLBwdIter<T>::operator = (tsDLList<T> &listIn)
{
	this->tsDLIter<T>::reset(listIn);
}

template <class T>
inline T * tsDLBwdIter<T>::last()
{
	return this->tsDLIter<T>::last();
}

//
// tsDLBwdIter<T>::remove ()
// 
// remove current item 
// (and move current to be the item
// pointed to by pNext - the item seen
// by the iterator before the current one -
// this guarantee that the list will be
// accessed sequentially even if an item
// is removed)
//
template <class T>
void tsDLBwdIter<T>::remove ()
{
	T *pCur = this->pCurrent;

	if (pCur) {
		tsDLNode<T> *pCurNode = pCur;

		//
		// strip const (we didnt declare the
		// list const in the constructor)
		//
		tsDLList<T> * pMutableList = 
			(tsDLList<T> *) this->pList;

		//
		// Move this->pCurrent to the item after the
		// item being deleted 
		//
		this->pCurrent = pCurNode->pNext;

		//
		// delete current item
		//
		pMutableList->remove(*pCur);
	}
}

//
// tsDLBwdIter<T>::operator () ()
//
template <class T>
inline T * tsDLBwdIter<T>::operator () ()
{
	return this->tsDLIter<T>::prev();
}

//
// tsDLBwdIter<T>::prev () 
//
template <class T>
inline T * tsDLBwdIter<T>::prev () 
{
	return this->tsDLIter<T>::prev();
}

//////////////////////////////////////////
// tsDLFwdIter<T> member functions
//////////////////////////////////////////

template <class T>
inline tsDLFwdIter<T>::tsDLFwdIter (tsDLList<T> &listIn) : 
	tsDLIter<T>(listIn) {}

template <class T>
inline void tsDLFwdIter<T>::reset ()
{
	this->tsDLIter<T>::reset();
}

template <class T>
inline void tsDLFwdIter<T>::reset (tsDLList<T> &listIn)
{
	this->tsDLIter<T>::reset(listIn);
}

template <class T>
inline void tsDLFwdIter<T>::operator = (tsDLList<T> &listIn)
{
	this->tsDLIter<T>::reset(listIn);
}

template <class T>
inline T * tsDLFwdIter<T>::first()
{
	tsDLIter<T> &iterBase = *this;
	return iterBase.first();
}

//
// tsDLFwdIter<T>::remove ()
// (and move current to be the item
// pointed to by pPrev - the item seen
// by the iterator before the current one -
// this guarantee that the list will be
// accessed sequentially even if an item
// is removed)
//
template <class T>
void tsDLFwdIter<T>::remove ()
{
	T *pCur = this->pCurrent;

	if (pCur) {
		tsDLNode<T> *pCurNode = pCur;

		//
		// Move this->pCurrent to the previous item
		//
		this->pCurrent = pCurNode->pPrev;

		//
		// delete current item
		//
		this->pList->remove(*pCur);
	}
}

//
// tsDLFwdIter<T>::next ()
// 
template <class T>
inline T * tsDLFwdIter<T>::next () 
{
	tsDLIter<T> &iterBase = *this;
	return iterBase.next();
}

//
// tsDLFwdIter<T>::operator () ()
// 
template <class T>
inline T * tsDLFwdIter<T>::operator () () 
{
	return this->next();
}

#endif // tsDLListH_include

