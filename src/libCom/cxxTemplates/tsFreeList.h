
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
// static tsFreeList < class classXYZ, 1024 > freeList;
// static epicsMutex freeListMutex;
//
// 2) add the following member functions to your class
// 
// inline void * classXYZ::operator new ( size_t size )
// {
//    epicsAutoMutex locker ( classXYZ::freeListMutex );
//    void *p = classXYZ::freeList.allocate ( size );
//    if ( ! p ) {
//        throw std::bad_alloc ();
//    }
//    return p;
// }
//
// inline void classXYZ::operator delete ( void *pCadaver, size_t size )
// {
//    epicsAutoMutex locker ( classXYZ::freeListMutex );
//    classXYZ::freeList.release ( pCadaver, size );
// }
//
// NOTES:
//
// 1) If a tsFreeList instance is used by more than one thread then the
// user must provide mutual exclusion in their new and delete handlers.
//
// 2) If you wish to force use of the new operator, then declare your class's 
// destructor as a protected member function.
//

#include <stdio.h>
#include <typeinfo>
#include "epicsMutex.h"

// these versions of the microsoft compiler incorrectly
// warn about a missing delete operator if only the
// newly preferred delete operator with a size argument 
// is present - I expect that they will fix this in the
// next version
#if defined ( _MSC_VER ) && _MSC_VER <= 1200
#   pragma warning ( disable : 4291 )  
#endif

template < class T, unsigned DEBUG_LEVEL >
union tsFreeListItem {
public:
    tsFreeListItem < T, DEBUG_LEVEL > *pNext;
    char pad[ sizeof ( T ) ];
};

template < class T, unsigned N = 0x400, unsigned DEBUG_LEVEL = 0u >
struct tsFreeListChunk {
    tsFreeListChunk < T, N, DEBUG_LEVEL > *pNext;
    tsFreeListItem < T, DEBUG_LEVEL > items [N];
};

template < class T, unsigned N = 0x400, unsigned DEBUG_LEVEL = 0u >
class tsFreeList {
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
inline tsFreeList < T, N, DEBUG_LEVEL > :: tsFreeList () : 
    pFreeList ( 0 ), pChunkList ( 0 ) {}

template < class T, unsigned N, unsigned DEBUG_LEVEL >
tsFreeList < T, N, DEBUG_LEVEL > :: ~tsFreeList ()
{
    tsFreeListChunk < T, N, DEBUG_LEVEL > *pChunk;
    unsigned nChunks;

    if ( DEBUG_LEVEL > 0u ) {
        nChunks = 0u;
    }

    while ( ( pChunk = this->pChunkList ) ) {
        this->pChunkList = this->pChunkList->pNext;
        delete pChunk;
        if ( DEBUG_LEVEL > 0u ) {
            nChunks++;
        }
    }

    if ( DEBUG_LEVEL > 0u ) {
        fprintf ( stderr, "free list destructor for class %s returned %u objects to pool\n", 
            typeid ( T ).name (), N * nChunks );
    }
}

template < class T, unsigned N, unsigned DEBUG_LEVEL >
inline void * tsFreeList < T, N, DEBUG_LEVEL >::allocate ( size_t size )
{
#   ifdef EPICS_FREELIST_DEBUG
        return ::operator new ( size, std::nothrow );
#   else
        tsFreeListItem < T, DEBUG_LEVEL > *p;

        if ( DEBUG_LEVEL > 9 ) {
            fprintf ( stderr, "creating a new %s of size %u\n", 
                typeid ( T ).name (), sizeof ( T ) );
        }

        if ( size != sizeof ( T ) || N == 0u ) {
            return ::operator new ( size, std::nothrow );
        }

        p = this->pFreeList;
        if ( p ) {
            this->pFreeList = p->pNext;
        }
        else {
            p = this->allocateFromNewChunk ();
        }

        return static_cast < void * > ( p );
#   endif
}

template < class T, unsigned N, unsigned DEBUG_LEVEL >
tsFreeListItem < T, DEBUG_LEVEL > * tsFreeList < T, N, DEBUG_LEVEL >::allocateFromNewChunk ()
{
    //if ( DEBUG_LEVEL > 0 ) {
    //    fprintf ( stderr, "allocating a %s of size %u\n", 
    //        typeid ( tsFreeListChunk < T, N, DEBUG_LEVEL > ).name (), 
    //        sizeof ( tsFreeListChunk < T, N, DEBUG_LEVEL > ) );
    //}

    tsFreeListChunk < T, N, DEBUG_LEVEL > *pChunk = 
        new ( std::nothrow ) ( tsFreeListChunk < T, N, DEBUG_LEVEL > );
    if ( ! pChunk ) {
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
#   ifdef EPICS_FREELIST_DEBUG
        memset ( pCadaver, 0xdd, size );
        ::operator delete ( pCadaver );
#   else
        if ( DEBUG_LEVEL > 9 ) {
            fprintf ( stderr, "releasing a %s of size %u\n", 
                typeid ( T ).name (), sizeof ( T ) );
        }
        if ( size != sizeof ( T ) || N == 0u ) {
            ::operator delete ( pCadaver );
        }
        else if ( pCadaver ) {
            tsFreeListItem < T, DEBUG_LEVEL > *p = 
                static_cast < tsFreeListItem < T, DEBUG_LEVEL > *> ( pCadaver );
            p->pNext = this->pFreeList;
            this->pFreeList = p;
        }
#   endif
}

#endif // tsFreeList_h
