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
public:
    tsDLNode();
    tsDLNode <T> operator = (const tsDLNode<T> &) const;
private:
    T *pNext;
    T *pPrev;
    friend class tsDLList<T>;
    friend class tsDLIterBD<T>;
    friend class tsDLIterConstBD<T>;
    friend class tsDLIter<T>; // deprecated
    friend class tsDLFwdIter<T>; // deprecated
    friend class tsDLBwdIter<T>; // deprecated
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
public:
    tsDLList (); // create empty list
    unsigned count () const; // number of items on list
    void add ( T & item ); // add item to end of list
    void add ( tsDLList<T> & addList ); // add to end of list - addList left empty
    void push ( T & item ); // add item to beginning of list
    void remove ( T & item ); // remove item from list
    T * get (); // removes first item on list
    T * pop (); // same as get ()
    void insertAfter ( T & item, T & itemBefore ); // insert item immediately after itemBefore
    void insertBefore ( T & item, T & itemAfter ); // insert item immediately before itemAfter
    int find ( const T & item ) const; // returns -1 if not present, node number if present
    T * first ( void ) const; // ptr to first item on list
    T * last ( void ) const; // ptr to last item on list
    tsDLIterConstBD <T> firstIter () const;
    tsDLIterBD <T> firstIter ();
    tsDLIterConstBD <T> lastIter () const;
    tsDLIterBD <T> lastIter ();
private:
    T * pFirst;
    T * pLast;
    unsigned itemCount;
    void clear (); 
    tsDLList ( const tsDLList & ); // not allowed
    const tsDLList <T> & operator = ( const tsDLList <T> & ); // not allowed
    friend class tsDLIter<T>; // deprecated
    friend class tsDLFwdIter<T>; // deprecated
    friend class tsDLBwdIter<T>; // deprecated
};

//
// class tsDLIterConstBD<T>
//
// bi-directional const doubly linked list iterator
//
template <class T>
class tsDLIterConstBD {
public:
    bool valid () const;
    bool operator == (const tsDLIterConstBD<T> &rhs) const;
    bool operator != (const tsDLIterConstBD<T> &rhs) const;
    const T & operator * () const;
    const T * operator -> () const;
    tsDLIterConstBD<T> & operator ++ (); 
    tsDLIterConstBD<T> operator ++ (int); 
    tsDLIterConstBD<T> & operator -- (); 
    tsDLIterConstBD<T> operator -- (int); 
    const T * pointer () const;
private:
    const T * pEntry;
    tsDLIterConstBD ( const T * pInitialEntry );
    friend class tsDLList <T>;
};

//
// class tsDLIterBD<T>
//
// bi-directional doubly linked list iterator
//
template <class T>
class tsDLIterBD {
public:
    bool valid () const;
    bool operator == ( const tsDLIterBD<T> & rhs ) const;
    bool operator != ( const tsDLIterBD<T> & rhs ) const;
    T & operator * () const;
    T * operator -> () const;
    tsDLIterBD<T> & operator ++ (); 
    tsDLIterBD<T> operator ++ ( int ); 
    tsDLIterBD<T> & operator -- (); 
    tsDLIterBD<T> operator -- ( int );  
    T * pointer () const;
private:
    T * pEntry;
    tsDLIterBD ( T *pInitialEntry );
    friend class tsDLList <T>;
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
// when someone tries to copy another node into a node 
// do _not_ change the node pointers
//
template <class T>
inline tsDLNode<T> tsDLNode<T>::operator = (const tsDLNode<T> &) const 
{ 
    return tsDLNode<T>(); 
}

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
inline void tsDLList<T>::remove ( T &item )
{
    tsDLNode<T> &theNode = item;

    if ( this->pLast == &item ) {
        this->pLast = theNode.pPrev;
    }
    else {
        tsDLNode<T> &nextNode = *theNode.pNext;
        nextNode.pPrev = theNode.pPrev; 
    }

    if ( this->pFirst == &item ) {
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

    if ( pItem ) {
        this->remove ( *pItem );
    }
    
    return pItem;
}

//
// tsDLList<T>::pop ()
//
// (returns the first item on the list)
template <class T>
inline T * tsDLList<T>::pop ()
{
    return this->get ();
}

//
// tsDLList<T>::add ()
// 
// adds addList to the end of the list
// (and removes all items from addList)
//
template <class T>
inline void tsDLList<T>::add ( tsDLList<T> &addList )
{
    if ( addList.itemCount != 0u ) {
        if ( this->itemCount == 0u ) {
            this->pFirst = addList.pFirst;
        }
        else {
            tsDLNode<T> *pLastNode = this->pLast;
            tsDLNode<T> *pAddListFirstNode = addList.pFirst;
            pLastNode->pNext = addList.pFirst;
            pAddListFirstNode->pPrev = addList.pLast;
        }
        this->pLast = addList.pLast;
        this->itemCount += addList.itemCount;
        addList.clear();
    }
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

    if ( this->itemCount ) {
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

template < class T >
inline tsDLIterConstBD <T> tsDLList < T > :: firstIter () const
{
    return tsDLIterConstBD < T > ( this->pFirst );
}

template < class T >
inline tsDLIterBD <T> tsDLList < T > :: firstIter ()
{
    return tsDLIterBD < T > ( this->pFirst );
}

template < class T >
inline tsDLIterConstBD <T> tsDLList < T > :: lastIter () const
{
    return tsDLIterConstBD < T > ( this->pLast );
}

template < class T >
inline tsDLIterBD <T> tsDLList < T > :: lastIter ()
{
    return tsDLIterBD < T > ( this->pLast );
}

//////////////////////////////////////////
// tsDLIterConstBD<T> member functions
//////////////////////////////////////////
template <class T>
inline tsDLIterConstBD<T>::tsDLIterConstBD ( const T *pInitialEntry ) : 
    pEntry ( pInitialEntry ) {}

template <class T>
inline bool tsDLIterConstBD<T>::valid () const
{
    return this->pEntry != 0;
}

template <class T>
inline bool tsDLIterConstBD<T>::operator == (const tsDLIterConstBD<T> &rhs) const
{
    return this->pEntry == rhs.pEntry;
}

template <class T>
inline bool tsDLIterConstBD<T>::operator != (const tsDLIterConstBD<T> &rhs) const
{
    return this->pEntry != rhs.pEntry;
}

template <class T>
inline const T & tsDLIterConstBD<T>::operator * () const
{
    return *this->pEntry;
}

template <class T>
inline const T * tsDLIterConstBD<T>::operator -> () const
{
    return this->pEntry;
}

//
// prefix ++
//
template <class T>
inline tsDLIterConstBD<T> & tsDLIterConstBD<T>::operator ++ () 
{
    const tsDLNode<T> &node = *this->pEntry;
    this->pEntry = node.pNext;
    return *this;
}

//
// postfix ++
//
template <class T>
inline tsDLIterConstBD<T> tsDLIterConstBD<T>::operator ++ (int) 
{
    const tsDLIterConstBD<T> tmp = *this;
    const tsDLNode<T> &node = *this->pEntry;
    this->pEntry = node.pNext;
    return tmp;
}

//
// prefix -- 
//
template <class T>
inline tsDLIterConstBD<T> & tsDLIterConstBD<T>::operator -- () 
{
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pPrev;
    return *this;
}

//
// postfix -- 
//
template <class T>
inline tsDLIterConstBD<T> tsDLIterConstBD<T>::operator -- (int) 
{
    const tsDLIterConstBD<T> tmp = *this;
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pPrev;
    return tmp;
}

template <class T>
inline const T * tsDLIterConstBD<T>::pointer () const
{
    return this->pEntry;
}

//////////////////////////////////////////
// tsDLIterBD<T> member functions
//////////////////////////////////////////

template <class T>
inline tsDLIterBD<T>::tsDLIterBD ( T * pInitialEntry ) : 
    pEntry ( pInitialEntry ) {}

template <class T>
inline bool tsDLIterBD<T>::valid () const
{
    return this->pEntry != 0;
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
inline tsDLIterBD<T> & tsDLIterBD<T>::operator ++ () // prefix ++
{
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pNext;
    return *this;
}

template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator ++ (int) // postfix ++
{
    const tsDLIterBD<T> tmp = *this;
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pNext;
    return tmp;
}

template <class T>
inline tsDLIterBD<T> & tsDLIterBD<T>::operator -- () // prefix --
{
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pPrev;
    return *this;
}

template <class T>
inline tsDLIterBD<T> tsDLIterBD<T>::operator -- (int) // postfix -- 
{
    const tsDLIterBD<T> tmp = *this;
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pPrev;
    return tmp;
}

template <class T>
inline T * tsDLIterBD<T>::pointer () const
{
    return this->pEntry;
}

#include "tsDLListDeprecated.h"

#endif // tsDLListH_include

