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
 * Auther Jeff Hill
 */

#include <stdlib.h>

#include "epicsMutex.h"
#include "dbDefs.h"

#include "cadef.h" // this can be eliminated when the callbacks use the new interface
#include "db_access.h" // should be eliminated here in the future

#define epicsExportSharedSymbols

#include "db_access_routines.h"
#include "dbCAC.h"

#include "epicsAssert.h"

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
    epicsGuard < epicsMutex > & guard, struct dbChannel * dbch,
	unsigned type, unsigned long count, cacReadNotify & notify )
{
    guard.assertIdenticalMutex ( _mutex );

    if ( type > INT_MAX ) {
        notify.exception ( guard, ECA_BADTYPE,
            "type code out of range (high side)",
            type, count );
        return;
    }

    const long maxcount = dbChannelElements(dbch);

    if ( maxcount < 0 ) {
        notify.exception ( guard, ECA_BADCOUNT,
            "database has negetive element count",
            type, count);
        return;

    } else if ( count > (unsigned long)maxcount ) {
        notify.exception ( guard, ECA_BADCOUNT,
            "element count out of range (high side)",
            type, count);
        return;
    }

    long realcount = (count==0)?maxcount:count;
    unsigned long size = dbr_size_n ( type, realcount );

    privateAutoDestroyPtr ptr ( _allocator, size );
    int status;
    {
        epicsGuardRelease < epicsMutex > unguard ( guard );
        if ( count==0 )
            status = dbChannel_get_count ( dbch, (int)type, ptr.get(), &realcount, 0);
        else
            status = dbChannel_get ( dbch, (int)type, ptr.get (), realcount, 0 );
    }
    if ( status ) {
        notify.exception ( guard, ECA_GETFAIL,
            "db_get_field() completed unsuccessfuly",
            type, count );
    }
    else {
        notify.completion (
            guard, type, realcount, ptr.get () );
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
        assert(_pReadNotifyCache->size == _readNotifyCacheSize);
        ::free(_pReadNotifyCache);
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
        assert(pAlloc->size == _readNotifyCacheSize);
        _pReadNotifyCache = pAlloc->pNext;
    }
    else {
        pAlloc = (cacheElem_t*)calloc(1, sizeof(cacheElem_t)+_readNotifyCacheSize);
        if(!pAlloc) throw std::bad_alloc();
        pAlloc->size = _readNotifyCacheSize;
    }
    return pAlloc->buf;
}

void dbContextReadNotifyCacheAllocator::free ( char * pFree )
{
    cacheElem_t * pAlloc = (cacheElem_t*)(pFree - offsetof(cacheElem_t, buf));
    if (pAlloc->size == _readNotifyCacheSize) {
        pAlloc->pNext = _pReadNotifyCache;
        _pReadNotifyCache = pAlloc;
    } else {
        ::free(pAlloc);
    }
}

void dbContextReadNotifyCacheAllocator::show ( unsigned level ) const
{
    printf ( "dbContextReadNotifyCacheAlocator\n" );
    if ( level > 0 ) {
        size_t count =0;
        cacheElem_t * pNext = _pReadNotifyCache;
        while ( pNext ) {
            assert(pNext->size == _readNotifyCacheSize);
            pNext = _pReadNotifyCache->pNext;
            count++;
        }
        printf ( "\tcount %lu and size %lu\n",
            static_cast < unsigned long > ( count ),
            _readNotifyCacheSize );
    }
}


