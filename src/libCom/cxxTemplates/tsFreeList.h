
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

#ifndef tsFreeList_h
#define tsFreeList_h

#include <new>

#ifdef EPICS_FREELIST_DEBUG
#   include <string.h>
#endif

//
// To allow your class to be allocated off of a free list
// using the new operator:
// 
// 1) add the following static, private free list data members 
// to your class
//
// static epicsSingleton < tsFreeList < class classXYZ > > pFreeList;
//
// 2) add the following member functions to your class
// 
// inline void * classXYZ::operator new ( size_t size )
// {
//    return classXYZ::pFreeList->allocate ( size );
// }
//
// inline void classXYZ::operator delete ( void *pCadaver, size_t size )
// {
//    classXYZ::pFreeList->release ( pCadaver, size );
// }
//
// NOTES:
//
// 1) A NOOP mutex class may be specified if mutual exclusion isnt required
//
// 2) If you wish to force use of the new operator, then declare your class's 
// destructor as a protected member function.
//
// 3) Setting N to zero causes the free list to be bypassed
//

#include <stdio.h>
#include <string.h>
#include <typeinfo>
#include "epicsMutex.h"
#include "epicsGuard.h"

#ifdef EPICS_FREELIST_DEBUG
#   define tsFreeListDebugBypass 1
#else
#   define tsFreeListDebugBypass 0
#endif

// these versions of the microsoft compiler incorrectly
// warn about a missing delete operator if only the
// newly preferred delete operator with a size argument 
// is present - I expect that they will fix this in the
// next version
#if defined ( _MSC_VER ) && _MSC_VER <= 1200
#   pragma warning ( disable : 4291 )  
#endif

template < class T, unsigned DEBUG_LEVEL > 
    union tsFreeListItem;
template < class T, unsigned N, unsigned DEBUG_LEVEL> 
    struct tsFreeListChunk;

template < class T, unsigned N = 0x400, 
    class MUTEX = epicsMutex, unsigned DEBUG_LEVEL = 0u >
class tsFreeList {
public:
    tsFreeList ();
    ~tsFreeList ();
    void * allocate ( size_t size );
    void release ( void *p, size_t size );
private:
    tsFreeListItem < T, DEBUG_LEVEL > *pFreeList;
    tsFreeListChunk < T, N, DEBUG_LEVEL > *pChunkList;
    MUTEX mutex;
    tsFreeListItem < T, DEBUG_LEVEL > * allocateFromNewChunk ();
};

template < class T, unsigned DEBUG_LEVEL >
union tsFreeListItem {
public:
    char pad [ sizeof ( T ) ];
    tsFreeListItem < T, DEBUG_LEVEL > *pNext;
};

template < class T, unsigned N = 0x400, unsigned DEBUG_LEVEL = 0u >
struct tsFreeListChunk {
    tsFreeListItem < T, DEBUG_LEVEL > items [N];
    tsFreeListChunk < T, N, DEBUG_LEVEL > *pNext;
};

template < class T, unsigned N, class MUTEX, unsigned DEBUG_LEVEL >
inline tsFreeList < T, N, MUTEX, DEBUG_LEVEL > :: tsFreeList () : 
    pFreeList ( 0 ), pChunkList ( 0 ) {}

template < class T, unsigned N, class MUTEX, unsigned DEBUG_LEVEL >
tsFreeList < T, N, MUTEX, DEBUG_LEVEL > :: ~tsFreeList ()
{
    tsFreeListChunk < T, N, DEBUG_LEVEL > *pChunk;
    unsigned nChunks;

    if ( DEBUG_LEVEL > 1u ) {
        nChunks = 0u;
    }

    while ( ( pChunk = this->pChunkList ) ) {
        this->pChunkList = this->pChunkList->pNext;
        delete pChunk;
        if ( DEBUG_LEVEL > 1u ) {
            nChunks++;
        }
    }

    if ( DEBUG_LEVEL > 1u ) {
        fprintf ( stderr, "free list destructor for class %s returned %u objects to pool\n", 
            typeid ( T ).name (), N * nChunks );
    }
}

template < class T, unsigned N, class MUTEX, unsigned DEBUG_LEVEL >
void * tsFreeList < T, N, MUTEX, DEBUG_LEVEL >::allocate ( size_t size )
{
    if ( DEBUG_LEVEL > 1 ) {
        fprintf ( stderr, "creating a new %s of size %u\n", 
            typeid ( T ).name (), sizeof ( T ) );
    }

    if ( size != sizeof ( T ) || N == 0u || tsFreeListDebugBypass ) {
        void *p = ::operator new ( size );
        if ( tsFreeListDebugBypass ) {
            memset ( p, 0xaa, size );
        }
        return p;
    }

    epicsGuard < MUTEX > guard ( this->mutex );

    tsFreeListItem < T, DEBUG_LEVEL > *p = this->pFreeList;
    if ( p ) {
        this->pFreeList = p->pNext;
    }
    else {
        p = this->allocateFromNewChunk ();
    }

    return static_cast < void * > ( p );
}

template < class T, unsigned N, class MUTEX, unsigned DEBUG_LEVEL >
tsFreeListItem < T, DEBUG_LEVEL > * 
    tsFreeList < T, N, MUTEX, DEBUG_LEVEL >::allocateFromNewChunk ()
{
    tsFreeListChunk < T, N, DEBUG_LEVEL > *pChunk = 
        new tsFreeListChunk < T, N, DEBUG_LEVEL >;

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

template < class T, unsigned N, class MUTEX, unsigned DEBUG_LEVEL >
void tsFreeList < T, N, MUTEX, DEBUG_LEVEL >::release 
                                    ( void *pCadaver, size_t size )
{
    if ( DEBUG_LEVEL > 1 ) {
        fprintf ( stderr, "releasing a %s of size %u\n", 
            typeid ( T ).name (), sizeof ( T ) );
    }
    if ( size != sizeof ( T ) || N == 0u || tsFreeListDebugBypass ) {
        if ( tsFreeListDebugBypass ) {
            memset ( pCadaver, 0xdd, size );
        }
        ::operator delete ( pCadaver );
    }
    else if ( pCadaver ) {
        epicsGuard < MUTEX > guard ( this->mutex );
        tsFreeListItem < T, DEBUG_LEVEL > *p = 
            static_cast < tsFreeListItem < T, DEBUG_LEVEL > *> ( pCadaver );
        p->pNext = this->pFreeList;
        this->pFreeList = p;
    }
}

#endif // tsFreeList_h
