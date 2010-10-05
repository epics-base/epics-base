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
 * $Revision-Id$
 * Auther Jeff Hill
 */

#include "epicsMutex.h"

#include "cadef.h" // this can be eliminated when the callbacks use the new interface
#include "db_access.h" // should be eliminated here in the future

#define epicsExportSharedSymbols

#include "db_access_routines.h"
#include "dbCAC.h"

dbContextReadNotifyCache::dbContextReadNotifyCache ( epicsMutex & mutexIn ) :
    _mutex ( mutexIn )
{
}

class privateAutoDestroyPtr {
public:
    privateAutoDestroyPtr ( 
        dbContextReadNotifyCacheAllocator & allocator, unsigned long size ) :
        _allocator ( allocator ), _p ( allocator.alloc ( size ) ) {}
    ~privateAutoDestroyPtr () { _allocator.free ( _p ); }
	char * get () const { return _p; }              
private:
    dbContextReadNotifyCacheAllocator & _allocator;
    char * _p;
	privateAutoDestroyPtr ( const privateAutoDestroyPtr & );
	privateAutoDestroyPtr & operator = ( const privateAutoDestroyPtr & );
};

// extra effort taken here to not hold the lock when calling the callback
void dbContextReadNotifyCache::callReadNotify ( 
    epicsGuard < epicsMutex > & guard, struct dbAddr & addr, 
	unsigned type, unsigned long count, cacReadNotify & notify )
{
    guard.assertIdenticalMutex ( _mutex );

    if ( type > INT_MAX ) {
        notify.exception ( guard, ECA_BADTYPE, 
            "type code out of range (high side)", 
            type, count );
        return;
    }

    if ( addr.no_elements < 0 ) {
        notify.exception ( guard, ECA_BADCOUNT, 
            "database has negetive element count",
            type, count);
        return;
    }

    if ( count > static_cast < unsigned > ( addr.no_elements ) ) {
        notify.exception ( guard, ECA_BADCOUNT, 
            "element count out of range (high side)",
            type, count);
        return;
    }

    unsigned long size = dbr_size_n ( type, count );
    privateAutoDestroyPtr ptr ( _allocator, size );
    int status;
    {
        epicsGuardRelease < epicsMutex > unguard ( guard );
        status = db_get_field ( &addr, static_cast <int> ( type ), 
                    ptr.get (), static_cast <int> ( count ), 0 );
    }
    if ( status ) {
        notify.exception ( guard, ECA_GETFAIL, 
            "db_get_field() completed unsuccessfuly",
            type, count );
    }
    else { 
        notify.completion ( 
            guard, type, count, ptr.get () );
    }
}

void dbContextReadNotifyCache::show ( 
    epicsGuard < epicsMutex > & guard, unsigned level ) const
{
    guard.assertIdenticalMutex ( _mutex );

    printf ( "dbContextReadNotifyCache\n" );
    if ( level > 0 ) {
        this->_allocator.show ( level - 1 );
    }
}

dbContextReadNotifyCacheAllocator::dbContextReadNotifyCacheAllocator () :
    _readNotifyCacheSize ( 0 ), _pReadNotifyCache ( 0 )
{
}

dbContextReadNotifyCacheAllocator::~dbContextReadNotifyCacheAllocator ()
{
    this->reclaimAllCacheEntries ();
}

void dbContextReadNotifyCacheAllocator::reclaimAllCacheEntries ()
{

    while ( _pReadNotifyCache ) {
        cacheElem_t * pNext = _pReadNotifyCache->pNext;
        delete [] _pReadNotifyCache;
        _pReadNotifyCache = pNext;
    }
}

char * dbContextReadNotifyCacheAllocator::alloc ( unsigned long size )
{
    if ( size > _readNotifyCacheSize ) {
        this->reclaimAllCacheEntries ();
        _readNotifyCacheSize = size;
    }

    cacheElem_t * pAlloc = _pReadNotifyCache;
    if ( pAlloc ) {
        _pReadNotifyCache = pAlloc->pNext;
    }
    else {
        size_t nElem = _readNotifyCacheSize / sizeof ( cacheElem_t );
        pAlloc = new cacheElem_t [ nElem + 1 ];
    }
    return reinterpret_cast < char * > ( pAlloc );
}

void dbContextReadNotifyCacheAllocator::free ( char * pFree )
{
    cacheElem_t * pAlloc = reinterpret_cast < cacheElem_t * > ( pFree );
    pAlloc->pNext = _pReadNotifyCache;
    _pReadNotifyCache = pAlloc;
}

void dbContextReadNotifyCacheAllocator::show ( unsigned level ) const
{
    printf ( "dbContextReadNotifyCacheAlocator\n" );
    if ( level > 0 ) {
        size_t count =0;
        cacheElem_t * pNext = _pReadNotifyCache;
        while ( pNext ) {
            pNext = _pReadNotifyCache->pNext;
            count++;
        }
        printf ( "\tcount %lu and size %lu\n", 
            static_cast < unsigned long > ( count ), 
            _readNotifyCacheSize );
    }
}


