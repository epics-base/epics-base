/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef tsFreeList_h
#define tsFreeList_h

/*
 *  Author: Jeff Hill
 */

//
// TODO: this should allow free list chaining so that a free 
// list (in a different lock domain) is used to provide the
// tsFreeListChunk.
//

//
// To allow your class to be allocated off of a free list
// using the new operator:
// 
// 1) add the following static, private free list data members 
// to your class
//
// static tsFreeList < class classXYZ > freeList;
//
// 2) add the following member functions to your class
// 
// inline void * classXYZ::operator new ( size_t size )
// {
//    return freeList.allocate ( size );
// }
//
// inline void classXYZ::operator delete ( void *pCadaver, size_t size )
// {
//    freeList.release ( pCadaver, size );
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

#ifdef EPICS_FREELIST_DEBUG
#   define tsFreeListDebugBypass 1
#   define tsFreeListMemSetNew(P,SIZE) memset ( (P), 0xaa, (SIZE) )
#   define tsFreeListMemSetDelete(P,SIZE)  memset ( (P), 0xdd, (SIZE) )
#else
#   define tsFreeListDebugBypass 0
#   define tsFreeListMemSetNew(P,SIZE)
#   define tsFreeListMemSetDelete(P,SIZE)
#endif

#include <new>
#include "string.h"

#include "compilerDependencies.h"
#include "epicsMutex.h"
#include "epicsGuard.h"

// ms visual studio 6.0 and before incorrectly
// warn about a missing delete operator if only the
// newly preferred delete operator with a size argument 
// is present 
#if defined ( _MSC_VER ) && _MSC_VER <= 1200 
#   pragma warning ( disable : 4291 )  
#endif

template < class T > union tsFreeListItem;
template < class T, unsigned N> struct tsFreeListChunk;

template < class T, unsigned N = 0x400, 
    class MUTEX = epicsMutex >
class tsFreeList {
public:
    tsFreeList ();
    ~tsFreeList ();
    void * allocate ( size_t size );
    void release ( void * p );
    void release ( void * p, size_t size );
private:
    MUTEX mutex;
    tsFreeListItem < T > * pFreeList;
    tsFreeListChunk < T, N > * pChunkList;
    void * allocateFromNewChunk ();
};

template < class T >
union tsFreeListItem {
public:
    char pad [ sizeof ( T ) ];
    tsFreeListItem < T > * pNext;
};

template < class T, unsigned N = 0x400 >
struct tsFreeListChunk {
    tsFreeListItem < T > items [N];
    tsFreeListChunk < T, N > * pNext;
};

template < class T, unsigned N, class MUTEX >
inline tsFreeList < T, N, MUTEX > :: tsFreeList () : 
    pFreeList ( 0 ), pChunkList ( 0 ) {}

template < class T, unsigned N, class MUTEX >
tsFreeList < T, N, MUTEX > :: ~tsFreeList ()
{
    while ( tsFreeListChunk < T, N > *pChunk = this->pChunkList ) {
        this->pChunkList = this->pChunkList->pNext;
        delete pChunk;
    }
}

template < class T, unsigned N, class MUTEX >
void * tsFreeList < T, N, MUTEX >::allocate ( size_t size ) 
{
    if ( size != sizeof ( T ) || N == 0u || tsFreeListDebugBypass ) {
        void * p = ::operator new ( size );
        tsFreeListMemSetNew ( p, size );
        return p;
    }

    epicsGuard < MUTEX > guard ( this->mutex );

    tsFreeListItem < T > * p = this->pFreeList;
    if ( p ) {
        this->pFreeList = p->pNext;
        return static_cast < void * > ( p );
    }
    return this->allocateFromNewChunk ();
}

template < class T, unsigned N, class MUTEX >
void * tsFreeList < T, N, MUTEX >::allocateFromNewChunk () 
{
    tsFreeListChunk < T, N > * pChunk = 
        new tsFreeListChunk < T, N >;

    for ( unsigned i=1u; i < N-1; i++ ) {
        pChunk->items[i].pNext = &pChunk->items[i+1];
    }
    pChunk->items[N-1].pNext = 0;
    if ( N > 1 ) {
        this->pFreeList = &pChunk->items[1u];
    }
    pChunk->pNext = this->pChunkList;
    this->pChunkList = pChunk;

    return static_cast <void *> ( &pChunk->items[0] );
}

template < class T, unsigned N, class MUTEX >
void tsFreeList < T, N, MUTEX >::release ( void * pCadaver, size_t size ) 
{
    if ( size != sizeof ( T ) ) {
        tsFreeListMemSetDelete ( pCadaver, size );
        ::operator delete ( pCadaver );
    }
    else {
        this->release ( pCadaver );
    }
}

template < class T, unsigned N, class MUTEX >
void tsFreeList < T, N, MUTEX >::release ( void * pCadaver )
{
    if ( N == 0u || tsFreeListDebugBypass ) {
        tsFreeListMemSetDelete ( pCadaver, sizeof ( T ) );
        ::operator delete ( pCadaver );
    }
    else if ( pCadaver ) {
        epicsGuard < MUTEX > guard ( this->mutex );
        tsFreeListItem < T > * p = 
            static_cast < tsFreeListItem < T > * > ( pCadaver );
        p->pNext = this->pFreeList;
        this->pFreeList = p;
    }
}

#endif // tsFreeList_h
