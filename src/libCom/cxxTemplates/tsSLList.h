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

#ifndef assert // allow use of epicsAssert.h
#include <assert.h>
#endif

#include "locationException.h"


//
// the hp compiler complains about parameterized friend
// class that has not been declared without this?
//
template < class T > class tsSLList;
template < class T > class tsSLIter;
template < class T > class tsSLIterConst;

//
// tsSLNode<T>
// NOTE: class T must derive from tsSLNode<T>
//
template <class T>
class tsSLNode {
friend class tsSLList < T >;
friend class tsSLIter < T >;
friend class tsSLIterConst < T >;
public:
    tsSLNode ();
    const tsSLNode < T > & operator = ( const tsSLNode < T > & ) const;
private:
    tsSLNode ( const tsSLNode < T > & );
    void removeNextItem (); // removes the item after this node
    T *pNext;
};


//
// tsSLList<T>
// NOTE: class T must derive from tsSLNode<T>
//
template < class T >
class tsSLList : public tsSLNode < T > {
public:
    tsSLList (); // creates an empty list
    void insert ( T &item, tsSLNode < T > &itemBefore ); // insert after item before
    void add ( T &item ); // add to the beginning of the list
    T * get (); // remove from the beginning of the list
    T * pop (); // same as get
    void push ( T &item ); // same as add
    T * first () const;
    void remove ( T &itemBefore );
private:
    tsSLList ( const tsSLList & ); // intentionally _not_ implemented
};

//
// tsSLIterConst<T>
//
template < class T >
class tsSLIterConst {
public:
    tsSLIterConst ( const T *pInitialEntry );

    tsSLIterConst<T> & operator = ( const T *pNewEntry );

    tsSLIterConst<T> itemAfter ();

    bool operator == (const tsSLIterConst<T> &rhs) const;
    bool operator != (const tsSLIterConst<T> &rhs) const;

    const T & operator * () const;
    const T * operator -> () const;

    tsSLIterConst<T> & operator ++ (); // prefix ++
    tsSLIterConst<T> operator ++ (int); // postfix ++

#   if defined(_MSC_VER) && _MSC_VER < 1200
        tsSLIterConst (const class tsSLIterConst<T> &copyIn);
#   endif

    bool valid () const;

    //
    // end of the list constant
    //
    static const tsSLIterConst<T> eol ();

protected:
    union {
        const T *pConstEntry;
        T *pEntry;
    };
};

//
// tsSLIter<T>
//
template < class T >
class tsSLIter : private tsSLIterConst<T> {
public:
    tsSLIter ( T *pInitialEntry );

    tsSLIter <T> & operator = ( T *pNewEntry );

    tsSLIter <T> itemAfter ();

    bool operator == (const tsSLIter<T> &rhs) const;
    bool operator != (const tsSLIter<T> &rhs) const;

    T & operator * () const;
    T * operator -> () const;

    tsSLIter <T> & operator ++ (); // prefix ++
    tsSLIter <T> operator ++ (int); // postfix ++

#   if defined(_MSC_VER) && _MSC_VER < 1200
        tsSLIter (class tsSLIter<T> &copyIn);
#   endif

    bool valid () const;

    //
    // end of the list constant
    //
    static const tsSLIter <T> eol ();
};

//////////////////////////////////////////
//
// tsSLNode<T> inline member functions
//
//////////////////////////////////////////

//
// tsSLNode<T>::tsSLNode
//
template < class T >
tsSLNode < T > ::tsSLNode () : pNext ( 0 ) {}

//
// tsSLNode<T>::operator =
//
// when someone copies into a class deriving from this
// do _not_ change the node pointers
//
template < class T >
inline const tsSLNode < T > &  tsSLNode < T >::operator = ( const tsSLNode < T > & ) const 
{
    return *this;
}

//
// removeNextItem ()
//
// removes the item after this node
//
template <class T>
inline void tsSLNode<T>::removeNextItem ()
{
    T *pItem = this->pNext;
    if ( pItem ) {
        tsSLNode < T > *pNode = pItem;
        this->pNext = pNode->pNext;
    }
}

//////////////////////////////////////////
//
// tsSLList<T> inline member functions
//
//////////////////////////////////////////

//
// tsSLList<T>::tsSLList()
// create an empty list
//
template < class T >
inline tsSLList < T > :: tsSLList ()
{
}

//
// tsSLList<T>::insert()
// (itemBefore might be the list header object and therefore
// will not always be of type T)
//
template < class T >
inline void tsSLList < T > :: insert ( T &item, tsSLNode < T > &itemBefore )
{
    tsSLNode < T > &node = item;
    node.pNext = itemBefore.pNext;
    itemBefore.pNext = &item;
}

//
// tsSLList<T>::add ()
//
template < class T >
inline void tsSLList < T > :: add ( T &item )
{
    this->insert ( item, *this );
}

//
// tsSLList<T>::get ()
//
template < class T >
inline T * tsSLList < T > :: get ()
{
    tsSLNode < T > *pThisNode = this;
    T *pItem = pThisNode->pNext;
    pThisNode->removeNextItem ();
    return pItem;
}

//
// tsSLList<T>::pop ()
//
template < class T >
inline T * tsSLList < T > :: pop ()
{
    return this->get ();
}

//
// tsSLList<T>::push ()
//
template <class T>
inline void tsSLList < T > :: push ( T &item )
{
    this->add (item);
}

template <class T>
inline T * tsSLList < T > :: first () const
{
    const tsSLNode < T > *pThisNode = this;
    return pThisNode->pNext;
}

template <class T>
void tsSLList < T > :: remove ( T &itemBefore )
{
    tsSLNode < T > *pBeforeNode = &itemBefore;
    tsSLNode < T > *pAfterNode =  pBeforeNode->pNext;
    pBeforeNode->pNext = pAfterNode->pNext;
}

//////////////////////////////////////////
//
// tsSLIterConst<T> inline member functions
//
//////////////////////////////////////////

template < class T >
inline tsSLIterConst<T>::tsSLIterConst ( const T *pInitialEntry ) : 
    pConstEntry ( pInitialEntry )
{
}

template < class T >
inline tsSLIterConst <T> & tsSLIterConst<T>::operator = ( const T *pNewEntry )
{
    this->pConstEntry = pNewEntry;
    return *this;
}

template < class T >
inline tsSLIterConst <T> tsSLIterConst<T>::itemAfter ()
{
    const tsSLNode < T > *pCurNode = this->pConstEntry;
    return pCurNode->pNext;
}

template < class T >
inline bool tsSLIterConst<T>::operator == ( const tsSLIterConst<T> &rhs ) const
{
    return this->pConstEntry == rhs.pConstEntry;
}

template < class T >
inline bool tsSLIterConst<T>::operator != (const tsSLIterConst<T> &rhs) const
{
    return this->pConstEntry != rhs.pConstEntry;
}

template < class T >
inline const T & tsSLIterConst<T>::operator * () const
{
    return *this->pConstEntry;
}

template < class T >
inline const T * tsSLIterConst<T>::operator -> () const
{
    return this->pConstEntry;
}

template < class T >
inline tsSLIterConst<T> & tsSLIterConst<T>::operator ++ () // prefix ++
{
    const tsSLNode < T > *pCurNode = this->pConstEntry;
    this->pConstEntry = pCurNode->pNext;
    return *this;
}

template < class T >
inline tsSLIterConst<T> tsSLIterConst<T>::operator ++ (int) // postfix ++
{
    tsSLIterConst<T> tmp = *this;
    tsSLNode < T > *pCurNode = this->pConstEntry;
    this->pConstEntry = pCurNode->pNext;
    return tmp;
}


#   if defined(_MSC_VER) && _MSC_VER < 1200
template < class T >
inline tsSLIterConst<T>::tsSLIterConst (const class tsSLIterConst<T> &copyIn) :
    pConstEntry ( copyIn.pConstEntry )
{
}
#   endif

template < class T >
inline bool tsSLIterConst<T>::valid () const
{
    return this->pConstEntry ? true : false;
}

//
// end of the list constant
//
template < class T >
inline const tsSLIterConst<T> tsSLIterConst<T>::eol ()
{
    return 0;
}

//////////////////////////////////////////
//
// tsSLIter<T> inline member functions
//
//////////////////////////////////////////

template < class T >
inline tsSLIter<T>::tsSLIter ( T *pInitialEntry ) : 
    tsSLIterConst<T> ( pInitialEntry )
{
}

template < class T >
inline tsSLIter <T> & tsSLIter<T>::operator = ( T *pNewEntry )
{
    tsSLIterConst<T>::operator = ( pNewEntry );
    return *this;
}

template < class T >
inline tsSLIter <T> tsSLIter<T>::itemAfter ()
{
    tsSLNode < T > *pCurNode = this->pEntry;
    return pCurNode->pNext;
}

template < class T >
inline bool tsSLIter<T>::operator == ( const tsSLIter<T> &rhs ) const
{
    return this->pEntry == rhs.pEntry;
}

template < class T >
inline bool tsSLIter<T>::operator != (const tsSLIter<T> &rhs) const
{
    return this->pEntry != rhs.pEntry;
}

template < class T >
inline T & tsSLIter<T>::operator * () const
{
    return *this->pEntry;
}

template < class T >
inline T * tsSLIter<T>::operator -> () const
{
    return this->pEntry;
}

template < class T >
inline tsSLIter<T> & tsSLIter<T>::operator ++ () // prefix ++
{
    this->tsSLIterConst<T>::operator ++ ();
    return *this;
}

template < class T >
inline tsSLIter<T> tsSLIter<T>::operator ++ (int) // postfix ++
{
    tsSLIter<T> tmp = *this;
    this->tsSLIterConst<T>::operator ++ ();
    return tmp;
}


#   if defined(_MSC_VER) && _MSC_VER < 1200
template < class T >
inline tsSLIter<T>::tsSLIter (const class tsSLIter<T> &copyIn) :
    tsSLIterConst<T> ( copyIn )
{
}
#   endif

template < class T >
inline bool tsSLIter<T>::valid () const
{
    return this->pEntry ? true : false;
}

//
// end of the list constant
//
template < class T >
inline const tsSLIter<T> tsSLIter<T>::eol ()
{
    return 0;
}


#endif // tsSLListh
