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
 *      type safe singly linked list templates
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef tsSLListh
#define tsSLListh

#ifndef assert // allow use of epicsAssert.h
#include <assert.h>
#endif

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
public:
    tsSLNode ();
    tsSLNode < T > & operator = ( const tsSLNode < T > & );
private:
    void removeNextItem (); // removes the item after this node
    T *pNext;
    tsSLNode ( const tsSLNode < T > & );
    friend class tsSLList < T >;
    friend class tsSLIter < T >;
    friend class tsSLIterConst < T >;
};


//
// tsSLList<T>
// NOTE: class T must derive from tsSLNode<T>
//
template < class T >
class tsSLList : public tsSLNode < T > {
public:
    tsSLList (); // creates an empty list
    tsSLList ( tsSLList & ); 
    void insert ( T &item, tsSLNode < T > &itemBefore ); // insert after item before
    void add ( T &item ); // add to the beginning of the list
    T * get (); // remove from the beginning of the list
    T * pop (); // same as get
    void push ( T &item ); // same as add
    T * first () const;
    void remove ( T &itemBefore );
    tsSLIterConst <T> firstIter () const;
    tsSLIter <T> firstIter ();
    static tsSLIterConst <T> invalidConstIter ();
    static tsSLIter <T> invalidIter ();
private:
    const tsSLList < T > & operator = ( const tsSLList < T > & );
};

//
// tsSLIterConst<T>
//
template < class T >
class tsSLIterConst {
public:
    tsSLIterConst ();
    bool valid () const;
    bool operator == (const tsSLIterConst<T> &rhs) const;
    bool operator != (const tsSLIterConst<T> &rhs) const;
    tsSLIterConst<T> & operator = (const tsSLIterConst<T> &);
    const T & operator * () const;
    const T * operator -> () const;
    tsSLIterConst<T> & operator ++ ();
    tsSLIterConst<T> operator ++ (int);
    const T * pointer () const;
protected:
    const T * pEntry;
    tsSLIterConst ( const T *pInitialEntry );
    friend class tsSLList < T >;
};

//
// tsSLIter<T>
//
template < class T >
class tsSLIter {
public:
    tsSLIter ();
    bool valid () const;
    bool operator == (const tsSLIter<T> &rhs) const;
    bool operator != (const tsSLIter<T> &rhs) const;
    tsSLIter<T> & operator = (const tsSLIter<T> &);
    T & operator * () const;
    T * operator -> () const;
    tsSLIter <T> & operator ++ ();
    tsSLIter <T> operator ++ (int);
    T * pointer () const;
private:
    T *pEntry;
    tsSLIter ( T *pInitialEntry );
    friend class tsSLList < T >;
};

//////////////////////////////////////////
//
// tsSLNode<T> inline member functions
//
//////////////////////////////////////////

//
// tsSLNode<T>::tsSLNode ()
//
template < class T >
inline tsSLNode < T > :: tsSLNode () : pNext ( 0 ) {}

//
// tsSLNode<T>::tsSLNode ( const tsSLNode < T > & )
// private - not to be used - implemented to eliminate warnings
//
template < class T >
inline tsSLNode < T > :: tsSLNode ( const tsSLNode < T > & )
{
}

//
// tsSLNode<T>::operator =
//
// when someone copies into a class deriving from this
// do _not_ change the node pointers
//
template < class T >
inline tsSLNode < T > &  tsSLNode < T >::operator = 
    ( const tsSLNode < T > & )
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
// tsSLList<T>::tsSLList( tsSLList & )
//
template < class T >
inline tsSLList < T > :: tsSLList ( tsSLList &listIn )
{
    this->pNext = listIn.pNext;
    listIn.pNext = 0;
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
inline void tsSLList < T > :: remove ( T &itemBefore )
{
    tsSLNode < T > *pBeforeNode = &itemBefore;
    tsSLNode < T > *pAfterNode =  pBeforeNode->pNext;
    pBeforeNode->pNext = pAfterNode->pNext;
}

template <class T>
inline tsSLIterConst <T> tsSLList < T > :: firstIter () const
{
    const tsSLNode < T > *pThisNode = this;
    return tsSLIterConst <T> ( pThisNode->pNext );
}

template <class T>
inline tsSLIter <T> tsSLList < T > :: firstIter ()
{
    tsSLNode < T > *pThisNode = this;
    return tsSLIter <T> ( pThisNode->pNext );
}

template <class T>
inline tsSLIterConst <T> tsSLList < T > :: invalidConstIter ()
{
    return tsSLIterConst <T> ( 0 );
}

template <class T>
inline tsSLIter <T> tsSLList < T > :: invalidIter ()
{
    return tsSLIter <T> ( 0 );
}

//////////////////////////////////////////
//
// tsSLIterConst<T> inline member functions
//
//////////////////////////////////////////

template < class T >
inline tsSLIterConst<T>::tsSLIterConst ( const T *pInitialEntry ) : 
    pEntry ( pInitialEntry )
{
}

template < class T >
inline tsSLIterConst<T>::tsSLIterConst () : 
    pEntry ( 0 )
{
}

template < class T >
inline bool tsSLIterConst<T>::valid () const
{
    return this->pEntry != 0;
}

template < class T >
inline bool tsSLIterConst<T>::operator == ( const tsSLIterConst<T> &rhs ) const
{
    return this->pEntry == rhs.pConstEntry;
}

template < class T >
inline bool tsSLIterConst<T>::operator != (const tsSLIterConst<T> &rhs) const
{
    return this->pEntry != rhs.pConstEntry;
}

template < class T >
inline tsSLIterConst<T> & tsSLIterConst<T>::operator = ( const tsSLIterConst<T> & rhs ) 
{
    this->pEntry = rhs.pEntry;
    return *this;
}

template < class T >
inline const T & tsSLIterConst<T>::operator * () const
{
    return *this->pEntry;
}

template < class T >
inline const T * tsSLIterConst<T>::operator -> () const
{
    return this->pEntry;
}

template < class T >
inline tsSLIterConst<T> & tsSLIterConst<T>::operator ++ () // prefix ++
{
    const tsSLNode < T > *pCurNode = this->pEntry;
    this->pEntry = pCurNode->pNext;
    return *this;
}

template < class T >
inline tsSLIterConst<T> tsSLIterConst<T>::operator ++ ( int ) // postfix ++
{
    const tsSLIterConst<T> tmp = *this;
    const tsSLNode < T > *pCurNode = this->pEntry;
    this->pEntry = pCurNode->pNext;
    return tmp;
}

template <class T>
inline const T * tsSLIterConst < T > :: pointer () const
{
    return this->pEntry;
}

//////////////////////////////////////////
//
// tsSLIter<T> inline member functions
//
//////////////////////////////////////////

template < class T >
inline tsSLIter<T>::tsSLIter ( T *pInitialEntry ) : 
    pEntry ( pInitialEntry )
{
}

template < class T >
inline tsSLIter<T>::tsSLIter () : 
    pEntry ( 0 )
{
}

template < class T >
inline bool tsSLIter<T>::valid () const
{
    return this->pEntry != 0;
}

template < class T >
inline bool tsSLIter<T>::operator == ( const tsSLIter<T> &rhs ) const
{
    return this->pEntry == rhs.pEntry;
}

template < class T >
inline bool tsSLIter<T>::operator != ( const tsSLIter<T> &rhs ) const
{
    return this->pEntry != rhs.pEntry;
}

template < class T >
inline tsSLIter<T> & tsSLIter<T>::operator = ( const tsSLIter<T> & rhs ) 
{
    this->pEntry = rhs.pEntry;
    return *this;
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
    const tsSLNode < T > *pCurNode = this->pEntry;
    this->pEntry = pCurNode->pNext;
    return *this;
}

template < class T >
inline tsSLIter<T> tsSLIter<T>::operator ++ ( int ) // postfix ++
{
    const tsSLIter<T> tmp = *this;
    const tsSLNode < T > *pCurNode = this->pEntry;
    this->pEntry = pCurNode->pNext;
    return tmp;
}

template <class T>
inline T * tsSLIter < T > :: pointer () const
{
    return this->pEntry;
}

#endif // tsSLListh
