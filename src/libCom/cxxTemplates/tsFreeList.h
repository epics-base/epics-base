
/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include "osiMutex.h"

#if defined(_MSC_VER) && _MSC_VER <= 1200
#   pragma warning( disable : 4291 )  
#endif

template < class T >
class tsFreeListItem {
public:
    union {
        tsFreeListItem < T > *pNext;
        char pad[ sizeof (T) ];
    };
};

template < class T, unsigned N = 0x400 >
class tsFreeListChunk {
public:
    tsFreeListChunk < T, N > *pNext;
    tsFreeListItem < T > items [N];
};

template < class T, unsigned N = 0x400 >
class tsFreeList : private osiMutex {
public:
    tsFreeList ();
    ~tsFreeList ();
    void * allocate ( size_t size );
    void release ( void *p, size_t size );
private:
    tsFreeListItem < T > *pFreeList;
    tsFreeListChunk < T, N > *pChunkList;
};

template < class T, unsigned N >
inline tsFreeList < T, N >::tsFreeList () : 
    pFreeList (0), pChunkList (0) {}

template < class T, unsigned N >
tsFreeList < T, N >::~tsFreeList ()
{
    tsFreeListChunk < T, N > *pChunk;

    this->lock ();

    while ( ( pChunk = this->pChunkList ) ) {
        this->pChunkList = this->pChunkList->pNext;
        delete pChunk;
    }

    this->unlock();
}

template < class T, unsigned N >
void * tsFreeList < T, N >::allocate (size_t size)
{
    tsFreeListItem < T > *p;

    if (size != sizeof (*p) ) {
        return ::operator new (size);
    }

    this->lock ();

    p = this->pFreeList;

    if ( p ) {
        this->pFreeList = p->pNext;
    }
    else {
        unsigned i;

        tsFreeListChunk<T,N> *pChunk = new ( tsFreeListChunk < T, N > );
        if ( ! pChunk) {
            return 0;
        }

        for ( i=1u; i < N-1; i++ ) {
            pChunk->items[i].pNext = &pChunk->items[i+1];
        }
        pChunk->items[N-1].pNext = 0;
        p = &pChunk->items[0];
        this->pFreeList = &pChunk->items[1u];
        pChunk->pNext = this->pChunkList;
        this->pChunkList = pChunk;
    }

    this->unlock ();

    return static_cast <void *> (p);
}

template < class T, unsigned N >
void tsFreeList < T, N >::release (void *pCadaver, size_t size)
{
    if ( size != sizeof (tsFreeListItem<T>) ) {
        ::operator delete (pCadaver);
    }
    else {
        if ( pCadaver ) {
            this->lock ();
            tsFreeListItem<T> *p = 
                static_cast < tsFreeListItem < T > *> (pCadaver);
            p->pNext = this->pFreeList;
            this->pFreeList = p;
            this->unlock ();
        }
    }
}
