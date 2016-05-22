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
 *      type safe doubly linked list templates
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef tsDLListH_include
#define tsDLListH_include

template <class T> class tsDLList;
template <class T> class tsDLIterConst;
template <class T> class tsDLIter;

//
// class tsDLNode<T>
//
// a node in a doubly linked list containing entries of type T
// ( class T must publicly derive from class tsDLNode<T> )
//
template < class T >
class tsDLNode {
public:
    tsDLNode ();
    tsDLNode ( const tsDLNode<T> & );
    const tsDLNode <T> & operator = ( const tsDLNode<T> & );
private:
    T * pNext;
    T * pPrev;
    friend class tsDLList<T>;
    friend class tsDLIter<T>;
    friend class tsDLIterConst<T>;
};

//
// class tsDLList<T>
//
// a doubly linked list containing entries of type T
// ( class T must publicly derive from class tsDLNode<T> )
//
template < class T >
class tsDLList {
public:
    tsDLList (); // create empty list
    unsigned count () const; // number of items on list
    void add ( T & item ); // add item to end of list
    void add ( tsDLList<T> & addList ); // add to end of list - addList left empty
    void push ( T & item ); // add item to beginning of list
    void push ( tsDLList<T> & pushList ); // add to beg of list - pushList left empty
    void remove ( T & item ); // remove item from list
    void removeAll ( tsDLList <T> & destination );
    T * get (); // removes first item on list
    T * pop (); // same as get ()
    void insertAfter ( T & item, T & itemBefore ); // insert item immediately after itemBefore
    void insertBefore ( T & item, T & itemAfter ); // insert item immediately before itemAfter
    int find ( const T & item ) const; // returns -1 if not present, node number if present
    T * first ( void ) const; // ptr to first item on list
    T * last ( void ) const; // ptr to last item on list
    tsDLIterConst <T> firstIter () const;
    tsDLIter <T> firstIter ();
    tsDLIterConst <T> lastIter () const;
    tsDLIter <T> lastIter ();
    static tsDLIterConst <T> invalidConstIter ();
    static tsDLIter <T> invalidIter ();
private:
    T * pFirst;
    T * pLast;
    unsigned itemCount;
    void clear (); 
    tsDLList ( const tsDLList & ); // not allowed
    const tsDLList <T> & operator = ( const tsDLList <T> & ); // not allowed
};

//
// class tsDLIterConst<T>
//
// bi-directional iterator for a const doubly linked list 
//
template <class T>
class tsDLIterConst {
public:
    tsDLIterConst ();
    bool valid () const;
    bool operator == ( const tsDLIterConst<T> & rhs ) const;
    bool operator != ( const tsDLIterConst<T> & rhs ) const;
    tsDLIterConst<T> & operator = ( const tsDLIterConst<T> & );
    const T & operator * () const;
    const T * operator -> () const;
    tsDLIterConst<T> & operator ++ (); 
    tsDLIterConst<T> operator ++ (int); 
    tsDLIterConst<T> & operator -- (); 
    tsDLIterConst<T> operator -- (int); 
    const T * pointer () const;
private:
    const T * pEntry;
    tsDLIterConst ( const T * pInitialEntry );
    friend class tsDLList <T>;
};

//
// class tsDLIter<T>
//
// bi-directional iterator for a doubly linked list 
//
template <class T>
class tsDLIter {
public:
    tsDLIter ();
    bool valid () const;
    bool operator == ( const tsDLIter<T> & rhs ) const;
    bool operator != ( const tsDLIter<T> & rhs ) const;
    tsDLIter<T> & operator = ( const tsDLIter<T> & );
    T & operator * () const;
    T * operator -> () const;
    tsDLIter<T> & operator ++ (); 
    tsDLIter<T> operator ++ ( int ); 
    tsDLIter<T> & operator -- (); 
    tsDLIter<T> operator -- ( int );  
    T * pointer () const;
private:
    T * pEntry;
    tsDLIter ( T *pInitialEntry );
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

template <class T>
inline tsDLNode<T>::tsDLNode ( const tsDLNode<T> & ) :  
    pNext (0), pPrev(0) {}

//
// tsDLNode<T>::operator = ()
//
// when someone tries to copy another node into a node 
// do _not_ change the node pointers
//
template <class T>
inline const tsDLNode<T> & tsDLNode<T>::operator = ( const tsDLNode<T> & ) 
{ 
    return * this; 
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
// tsDLList<T>::removeAll ()
//
template <class T>
inline void tsDLList<T>::removeAll ( 
        tsDLList <T> & destination )
{
    destination.pFirst = this->pFirst;
    destination.pLast = this->pLast;
    destination.itemCount = this->itemCount;
    this->pFirst = 0;
    this->pLast = 0;
    this->itemCount = 0;
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
// adds pushList to the beginning of the list
// (and removes all items from pushList)
//
template <class T>
inline void tsDLList<T>::push ( tsDLList<T> & pushList )
{
    if ( pushList.itemCount != 0u ) {
        if ( this->itemCount ) {
            tsDLNode<T> * pFirstNode = this->pFirst;
            tsDLNode<T> * pAddListLastNode = pushList.pLast;
            pFirstNode->pPrev = pushList.pLast;
            pAddListLastNode->pNext = this->pFirst;
        }
        else {
            this->pLast = pushList.pLast;
        }
        this->pFirst = pushList.pFirst;
        this->itemCount += pushList.itemCount;
        pushList.clear();
    }
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

    if ( this->itemCount ) {
        tsDLNode<T> *pFirstNode = this->pFirst;
        pFirstNode->pPrev = & item;
    }
    else {
        this->pLast = & item;
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
    tsDLIterConst < T > thisItem ( &item );
    tsDLIterConst < T > iter ( this->first () );
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
inline tsDLIterConst <T> tsDLList < T > :: firstIter () const
{
    return tsDLIterConst < T > ( this->pFirst );
}

template < class T >
inline tsDLIter <T> tsDLList < T > :: firstIter ()
{
    return tsDLIter < T > ( this->pFirst );
}

template < class T >
inline tsDLIterConst <T> tsDLList < T > :: lastIter () const
{
    return tsDLIterConst < T > ( this->pLast );
}

template < class T >
inline tsDLIter <T> tsDLList < T > :: lastIter ()
{
    return tsDLIter < T > ( this->pLast );
}

template < class T >
inline tsDLIterConst <T> tsDLList < T > :: invalidConstIter ()
{
    return tsDLIterConst < T > ( 0 );
}

template < class T >
inline tsDLIter <T> tsDLList < T > :: invalidIter ()
{
    return tsDLIter < T > ( 0 );
}

//////////////////////////////////////////
// tsDLIterConst<T> member functions
//////////////////////////////////////////
template <class T>
inline tsDLIterConst<T>::tsDLIterConst ( const T *pInitialEntry ) : 
    pEntry ( pInitialEntry ) {}

template <class T>
inline tsDLIterConst<T>::tsDLIterConst () : 
    pEntry ( 0 ) {}

template <class T>
inline bool tsDLIterConst<T>::valid () const
{
    return this->pEntry != 0;
}

template <class T>
inline bool tsDLIterConst<T>::operator == (const tsDLIterConst<T> &rhs) const
{
    return this->pEntry == rhs.pEntry;
}

template <class T>
inline bool tsDLIterConst<T>::operator != (const tsDLIterConst<T> &rhs) const
{
    return this->pEntry != rhs.pEntry;
}

template <class T>
inline tsDLIterConst<T> & tsDLIterConst<T>::operator = ( const tsDLIterConst<T> & rhs )
{
    this->pEntry = rhs.pEntry;
    return *this;
}

template <class T>
inline const T & tsDLIterConst<T>::operator * () const
{
    return *this->pEntry;
}

template <class T>
inline const T * tsDLIterConst<T>::operator -> () const
{
    return this->pEntry;
}

//
// prefix ++
//
template <class T>
inline tsDLIterConst<T> & tsDLIterConst<T>::operator ++ () 
{
    const tsDLNode<T> &node = *this->pEntry;
    this->pEntry = node.pNext;
    return *this;
}

//
// postfix ++
//
template <class T>
inline tsDLIterConst<T> tsDLIterConst<T>::operator ++ (int) 
{
    const tsDLIterConst<T> tmp = *this;
    const tsDLNode<T> &node = *this->pEntry;
    this->pEntry = node.pNext;
    return tmp;
}

//
// prefix -- 
//
template <class T>
inline tsDLIterConst<T> & tsDLIterConst<T>::operator -- () 
{
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pPrev;
    return *this;
}

//
// postfix -- 
//
template <class T>
inline tsDLIterConst<T> tsDLIterConst<T>::operator -- (int) 
{
    const tsDLIterConst<T> tmp = *this;
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pPrev;
    return tmp;
}

template <class T>
inline const T * tsDLIterConst<T>::pointer () const
{
    return this->pEntry;
}

//////////////////////////////////////////
// tsDLIter<T> member functions
//////////////////////////////////////////

template <class T>
inline tsDLIter<T>::tsDLIter ( T * pInitialEntry ) : 
    pEntry ( pInitialEntry ) {}

template <class T>
inline tsDLIter<T>::tsDLIter () : 
    pEntry ( 0 ) {}

template <class T>
inline bool tsDLIter<T>::valid () const
{
    return this->pEntry != 0;
}

template <class T>
inline bool tsDLIter<T>::operator == (const tsDLIter<T> &rhs) const
{
    return this->pEntry == rhs.pEntry;
}

template <class T>
inline bool tsDLIter<T>::operator != (const tsDLIter<T> &rhs) const
{
    return this->pEntry != rhs.pEntry;
}

template <class T>
inline tsDLIter<T> & tsDLIter<T>::operator = ( const tsDLIter<T> & rhs )
{
    this->pEntry = rhs.pEntry;
    return *this;
}

template <class T>
inline T & tsDLIter<T>::operator * () const
{
    return *this->pEntry;
}

template <class T>
inline T * tsDLIter<T>::operator -> () const
{
    return this->pEntry;
}

template <class T>
inline tsDLIter<T> & tsDLIter<T>::operator ++ () // prefix ++
{
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pNext;
    return *this;
}

template <class T>
inline tsDLIter<T> tsDLIter<T>::operator ++ (int) // postfix ++
{
    const tsDLIter<T> tmp = *this;
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pNext;
    return tmp;
}

template <class T>
inline tsDLIter<T> & tsDLIter<T>::operator -- () // prefix --
{
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pPrev;
    return *this;
}

template <class T>
inline tsDLIter<T> tsDLIter<T>::operator -- (int) // postfix -- 
{
    const tsDLIter<T> tmp = *this;
    const tsDLNode<T> &entryNode = *this->pEntry;
    this->pEntry = entryNode.pPrev;
    return tmp;
}

template <class T>
inline T * tsDLIter<T>::pointer () const
{
    return this->pEntry;
}

#endif // tsDLListH_include

