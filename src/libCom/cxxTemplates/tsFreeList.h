
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

#include <stdio.h>
#include <typeinfo>
#include "osiMutex.h"

#if defined ( _MSC_VER ) && _MSC_VER <= 1200
#   pragma warning ( disable : 4291 )  
#endif


template < class T, unsigned DEBUG_LEVEL >
union tsFreeListItem {
public:
    tsFreeListItem < T, DEBUG_LEVEL > *pNext;
    char pad[ sizeof (T) ];
};

template < class T, unsigned N = 0x400, unsigned DEBUG_LEVEL = 0u >
struct tsFreeListChunk {
    tsFreeListChunk < T, N, DEBUG_LEVEL > *pNext;
    tsFreeListItem < T, DEBUG_LEVEL > items [N];
};

template < class T, unsigned N = 0x400, unsigned DEBUG_LEVEL = 0u >
class tsFreeList : private osiMutex {
public:
    tsFreeList ();
    ~tsFreeList ();
    void * allocate ( size_t size );
    void release ( void *p, size_t size );
private:
    tsFreeListItem < T, DEBUG_LEVEL > *pFreeList;
    tsFreeListChunk < T, N, DEBUG_LEVEL > *pChunkList;
    tsFreeListItem < T, DEBUG_LEVEL > * allocateFromNewChunk ();
};

template < class T, unsigned N, unsigned DEBUG_LEVEL >
inline tsFreeList < T, N, DEBUG_LEVEL >::tsFreeList () : 
    pFreeList (0), pChunkList (0) {}

template < class T, unsigned N, unsigned DEBUG_LEVEL >
tsFreeList < T, N, DEBUG_LEVEL >::~tsFreeList ()
{
    tsFreeListChunk < T, N, DEBUG_LEVEL > *pChunk;
    unsigned nChunks = 0u;

    this->lock ();

    while ( ( pChunk = this->pChunkList ) ) {
        this->pChunkList = this->pChunkList->pNext;
        delete pChunk;
        nChunks++;
    }

    this->unlock();

    if ( DEBUG_LEVEL > 0u ) {
        fprintf ( stderr, "free list destructor for class %s returned %u objects to pool\n", 
            typeid ( T ).name (), N * nChunks );
    }
}

template < class T, unsigned N, unsigned DEBUG_LEVEL >
inline void * tsFreeList < T, N, DEBUG_LEVEL >::allocate ( size_t size )
{
    tsFreeListItem < T, DEBUG_LEVEL > *p;

    if ( size != sizeof ( T ) || N == 0u ) {
        return ::operator new ( size );
    }

    this->lock ();

    p = this->pFreeList;
    if ( p ) {
        this->pFreeList = p->pNext;
    }
    else {
        p = allocateFromNewChunk ();
    }

    this->unlock ();

    return static_cast < void * > ( p );
}

template < class T, unsigned N, unsigned DEBUG_LEVEL >
tsFreeListItem < T, DEBUG_LEVEL > * tsFreeList < T, N, DEBUG_LEVEL >::allocateFromNewChunk ()
{
    tsFreeListChunk < T, N, DEBUG_LEVEL > *pChunk = new ( tsFreeListChunk < T, N, DEBUG_LEVEL > );
    if ( ! pChunk) {
        return 0;
    }

    for ( unsigned i=1u; i < N-1; i++ ) {
        pChunk->items[i].pNext = &pChunk->items[i+1];
    }
    pChunk->items[N-1].pNext = 0;
    if ( N > 1 ) {
        this->pFreeList = &pChunk->items[1u];
    }
    pChunk->pNext = this->pChunkList;
    this->pChunkList = pChunk;

    return &pChunk->items[0];
}

template < class T, unsigned N, unsigned DEBUG_LEVEL >
inline void tsFreeList < T, N, DEBUG_LEVEL >::release ( void *pCadaver, size_t size )
{
    if ( size != sizeof ( T ) || N == 0u ) {
        ::operator delete ( pCadaver );
    }
    else if ( pCadaver ) {
        this->lock ();
        tsFreeListItem < T, DEBUG_LEVEL > *p = 
            static_cast < tsFreeListItem < T, DEBUG_LEVEL > *> ( pCadaver );
        p->pNext = this->pFreeList;
        this->pFreeList = p;
        this->unlock ();
    }
}
