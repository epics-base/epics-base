/*
 *      $Id$
 *
 *  type safe doubly linked list templates
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
template <class T> class tsDLIterConstBD;
template <class T> class tsDLIterBD;
template <class T> class tsDLIter; // deprecated
template <class T> class tsDLFwdIter; // deprecated
template <class T> class tsDLBwdIter; // deprecated

//
// class tsDLNode<T>
//
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
    friend class tsDLIterBD<T>;
    friend class tsDLIterConstBD<T>;
    friend class tsDLIter<T>; // deprecated
    friend class tsDLFwdIter<T>; // deprecated
    friend class tsDLBwdIter<T>; // deprecated
public:
    tsDLNode();
    void operator = (const tsDLNode<T> &) const;
private:
    T   *pNext;
    T   *pPrev;
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
    friend class tsDLIter<T>; // deprecated
    friend class tsDLFwdIter<T>; // deprecated
    friend class tsDLBwdIter<T>; // deprecated
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
    int find (const T &item) const;

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
// class tsDLIterConstBD<T>
//
// bi-directional const doubly linked list iterator
//
template <class T>
class tsDLIterConstBD {
public:
    tsDLIterConstBD (const T *pInitialEntry);

    tsDLIterConstBD<T> operator = (const T *pNewEntry);

    tsDLIterConstBD<T> itemAfter ();
    tsDLIterConstBD<T> itemBefore ();

    bool operator == (const tsDLIterConstBD<T> &rhs) const;
    bool operator != (const tsDLIterConstBD<T> &rhs) const;

    const T & operator * () const;
    const T * operator -> () const;

    tsDLIterConstBD<T> operator ++ (); // prefix ++
    tsDLIterConstBD<T> operator ++ (int); // postfix ++
    tsDLIterConstBD<T> operator -- (); // prefix --
    tsDLIterConstBD<T> operator -- (int); // postfix -- 

#   if defined(_MSC_VER) && _MSC_VER < 1200
        tsDLIterConstBD (const class tsDLIterConstBD<T> &copyIn);
#   endif

    bool valid () const;

    //
    // end of the list constant
    //
    static const tsDLIterConstBD<T> eol ();

protected:
    union {
        const T *pConstEntry;
        T *pEntry;
    };
};

//
// class tsDLIterBD<T>
//
// bi-directional doubly linked list iterator
//
template <class T>
class tsDLIterBD : private tsDLIterConstBD<T> {
public:
    tsDLIterBD (T *pInitialEntry);

    tsDLIterBD<T> operator = (T *pNewEntry);

    tsDLIterBD<T> itemAfter ();
    tsDLIterBD<T> itemBefore ();

    bool operator == (const tsDLIterBD<T> &rhs) const;
    bool operator != (const tsDLIterBD<T> &rhs) const;

    T & operator * () const;
    T * operator -> () const;

    tsDLIterBD<T> operator ++ (); // prefix ++

    tsDLIterBD<T> operator ++ (int); // postfix ++

    tsDLIterBD<T> operator -- (); // prefix --

    tsDLIterBD<T> operator -- (int); // postfix -- 

#   if defined(_MSC_VER) && _MSC_VER < 1200
        tsDLIterBD (const class tsDLIterBD<T> &copyIn);
#   endif

    bool valid () const;

    //
    // end of the list constant
    //
    static const tsDLIterBD<T> eol ();
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
inline void tsDLList<T>::remove (T &item)
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
inline void tsDLList<T>::add (tsDLList<T> &addList)
{
    //
    // NOOP if addList is empty
    //
    if ( addList.itemCount == 0u ) {
        return;
    }

    if ( this->itemCount == 0u ) {
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
inline void tsDLList<T>::add (T &item)
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
inline void tsDLList<T>::insertAfter (T &item, T &itemBefore)
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
inline void tsDLList<T>::insertBefore (T &item, T &itemAfter)
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
inline void tsDLList<T>::push (T &item)
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
template < class T >
int tsDLList < T > :: find ( const T &item ) const
{
    tsDLIterConstBD < T > thisItem ( &item );
    tsDLIterConstBD < T > iter ( this->first () );
    int itemNo = 0;

    while ( iter.valid () ) {
        if ( iter == thisItem ) {
            return itemNo;
        }
        itemNo++;
        iter++;
    }
    return -1;
}

//////////////////////////////////////////
// tsDLIterConstBD<T> member functions
//////////////////////////////////////////
template <class T>
inline tsDLIterConstBD<T>::tsDLIterConstBD (const T * pInitialEntry) : 
    pConstEntry (pInitialEntry) {}

//
// This is apparently required by some compiler, but
// only causes trouble with MS Visual C 6.0. This 
// should not be required by any compiler. I am assuming 
// that this "some compiler" is a past version of MS 
// Visual C.
//  
#   if defined(_MSC_VER) && _MSC_VER < 1200
    template <class T>
    inline tsDLIterConstBD<T>::tsDLIterConstBD (const class tsDLIterBD<T> &copyIn) :
        pConstEntry (copyIn.pEntry) {}
#   endif

template <class T>
inline tsDLIterConstBD<T> tsDLIterConstBD<T>::operator = (const T *pNewEntry)
{
    this->pConstEntry = pNewEntry;
    return *this;
}

template <class T>
inline bool tsDLIterConstBD<T>::operator == (const tsDLIterConstBD<T> &rhs) const
{
    return this->pConstEntry == rhs.pConstEntry;
}

template <class T>
inline bool tsDLIterConstBD<T>::operator != (const tsDLIterConstBD<T> &rhs) const
{
    return this->pConstEntry != rhs.pConstEntry;
}

template <class T>
inline const T & tsDLIterConstBD<T>::operator * () const
{
    return *this->pConstEntry;
}

template <class T>
inline const T * tsDLIterConstBD<T>::operator -> () const
{
    return this->pConstEntry;
}

template <class T>
inline tsDLIterConstBD<T> tsDLIterConstBD<T>::itemAfter ()
{
    const tsDLNode<T> &node = *this->pConstEntry;
    return tsDLIterConstBD<T> (node.pNext);
}

template <class T>
inline tsDLIterConstBD<T> tsDLIterConstBD<T>::itemBefore ()
{
    const tsDLNode<T> &node = *this->pConstEntry;
    return tsDLIterConstBD<T> (node.pPrev);
}

//
// prefix ++
//
template <class T>
inline tsDLIterConstBD<T> tsDLIterConstBD<T>::operator ++ () 
{
    const tsDLNode<T> &node = *this->pConstEntry;
    this->pConstEntry = node.pNext;
    return *this;
}

//
// postfix ++
//
template <class T>
inline tsDLIterConstBD<T> tsDLIterConstBD<T>::operator ++ (int) 
{
    tsDLIterConstBD<T> tmp = *this;
    const tsDLNode<T> &node = *this->pConstEntry;
    this->pConstEntry = node.pNext;
    return tmp;
}

//
// prefix -- 
//
template <class T>
inline tsDLIterConstBD<T> tsDLIterConstBD<T>::operator -- () 
{
    const tsDLNode<T> &entryNode = *this->pConstEntry;
    this->pConstEntry = entryNode.pPrev;
    return *this;
}

//
// postfix -- 
//
template <class T>
inline tsDLIterConstBD<T> tsDLIterConstBD<T>::operator -- (int) 
{
    tsDLIterConstBD<T> tmp = *this;
    const tsDLNode<T> &entryNode = *this->pConstEntry;
    this->pConstEntry = entryNode.pPrev;
    return tmp;
}

template <class T>
inline bool tsDLIterConstBD<T>::valid () const
{
    return this->pEntry ? true : false;
}

//
// tsDLIterConstBD<T>::eol
//
template <class T>
inline const tsDLIterConstBD<T> tsDLIterConstBD<T>::eol ()
{
    return tsDLIterConstBD<T>(0);
}

//////////////////////////////////////////
// tsDLIterBD<T> member functions
//////////////////////////////////////////

template <class T>
inline tsDLIterBD<T>::tsDLIterBD (T * pInitialEntry) : 
    tsDLIterConstBD<T> (pInitialEntry) {}

//
// This is apparently required by some compiler, but
// only causes trouble with MS Visual C 6.0. This 
// should not be required by any compiler. I am assuming 
// that this "some compiler" is a past version of MS 
// Visual C.
//  
#   if defined(_MSC_VER) && _MSC_VER < 1200
    template <class T>
    inline tsDLIterBD<T>::tsDLIterBD (const class tsDLIterBD<T> &copyIn) :
        tsDLIterConstBD (copyIn) {}
#   endif

template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator = (T *pNewEntry)
{
    tsDLIterConstBD<T>::operator = (pNewEntry);
    return *this;
}

template <class T>
inline bool tsDLIterBD<T>::operator == (const tsDLIterBD<T> &rhs) const
{
    return this->pEntry == rhs.pEntry;
}

template <class T>
inline bool tsDLIterBD<T>::operator != (const tsDLIterBD<T> &rhs) const
{
    return this->pEntry != rhs.pEntry;
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
inline tsDLIterBD<T> tsDLIterBD<T>::itemAfter ()
{
    tsDLNode<T> &node = *this->pEntry;
    return tsDLIterBD<T> (node.pNext);
}

template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::itemBefore ()
{
    tsDLNode<T> &node = *this->pEntry;
    return tsDLIterBD<T> (node.pPrev);
}

template <class T>
inline bool tsDLIterBD<T>::valid () const
{
    return this->pEntry ? true : false;
}

template <class T>
inline const tsDLIterBD<T> tsDLIterBD<T>::eol ()
{
    return tsDLIterBD<T>(0);
}

template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator ++ () // prefix ++
{
    this->tsDLIterConstBD<T>::operator ++ ();
    return *this;
}

template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator ++ (int) // postfix ++
{
    tsDLIterBD<T> tmp = *this;
    this->tsDLIterConstBD<T>::operator ++ (1);
    return tmp;
}

template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator -- () // prefix --
{
    this->tsDLIterConstBD<T>::operator -- ();
    return *this;
}

template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator -- (int) // postfix -- 
{
    tsDLIterBD<T> tmp = *this;
    this->tsDLIterConstBD<T>::operator -- (1);
    return tmp;
}

#include "tsDLListDeprecated.h"

#endif // tsDLListH_include

