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
 * $Id$
 * Auther Jeff Hill
 */

#include "epicsMutex.h"
#include "tsFreeList.h"

#include "cadef.h" // this can be eliminated when the callbacks use the new interface
#include "db_access.h" // should be eliminated here in the future

#define epicsExportSharedSymbols

#include "db_access_routines.h"
#include "dbCAC.h"

dbContextReadNotifyCache::dbContextReadNotifyCache ( epicsMutex & mutexIn ) :
    readNotifyCacheSize ( 0 ), mutex ( mutexIn ), pReadNotifyCache ( 0 )
{
}

dbContextReadNotifyCache::~dbContextReadNotifyCache ()
{
	delete this->pReadNotifyCache;
}

void dbContextReadNotifyCache::show ( 
    epicsGuard < epicsMutex > & guard, unsigned level ) const
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( level > 0 ) {
        printf ( "\tget call back cache location %p, and its size %lu\n", 
            static_cast <void *> ( this->pReadNotifyCache ), 
            this->readNotifyCacheSize );
    }
}

// extra effort taken here to not hold the lock when caslling the callback
void dbContextReadNotifyCache::callReadNotify ( 
    epicsGuard < epicsMutex > & guard, struct dbAddr & addr, 
	unsigned type, unsigned long count, cacReadNotify & notify )
{
    guard.assertIdenticalMutex ( this->mutex );

    unsigned long size = dbr_size_n ( type, count );

    if ( type > INT_MAX ) {
        notify.exception ( guard, ECA_BADTYPE, 
            "type code out of range (high side)", 
            type, count );
        return;
    }

    if ( count > static_cast < unsigned > ( INT_MAX ) || 
		count > static_cast < unsigned > ( addr.no_elements ) ) {
        notify.exception ( guard, ECA_BADCOUNT, 
            "element count out of range (high side)",
            type, count);
        return;
    }

    if ( this->readNotifyCacheSize < size) {
        char * pTmp = new char [size];
        if ( ! pTmp ) {
            notify.exception ( guard, ECA_ALLOCMEM, 
                "unable to allocate callback cache",
                type, count );
            return;
        }
        delete [] this->pReadNotifyCache;
        this->pReadNotifyCache = pTmp;
        this->readNotifyCacheSize = size;
    }
    int status;
    {
        epicsGuardRelease < epicsMutex > unguard ( guard );
        status = db_get_field ( &addr, static_cast <int> ( type ), 
                    this->pReadNotifyCache, static_cast <int> ( count ), 0 );
    }
    if ( status ) {
        notify.exception ( guard, ECA_GETFAIL, 
            "db_get_field() completed unsuccessfuly",
            type, count);
    }
    else { 
        notify.completion ( guard, type, count, this->pReadNotifyCache );
    }
}
