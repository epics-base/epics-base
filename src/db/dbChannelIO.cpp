
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include "limits.h"

#include "cadef.h"
#include "cacIO.h"
#include "tsFreeList.h"
#include "epicsMutex.h"

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbCAC.h"
#include "dbChannelIOIL.h"
#include "dbNotifyBlockerIL.h"

tsFreeList < dbChannelIO > dbChannelIO::freeList;
epicsMutex dbChannelIO::freeListMutex;

dbChannelIO::dbChannelIO ( cacChannelNotify &notify, 
    const dbAddr &addrIn, dbServiceIO &serviceIO ) :
    cacChannelIO ( notify ), serviceIO ( serviceIO ), 
    pGetCallbackCache ( 0 ), pBlocker ( 0 ), 
    getCallbackCacheSize ( 0ul ), addr ( addrIn )
{
}

void dbChannelIO::initiateConnect ()
{
    this->notify ().connectNotify ( *this );
}

dbChannelIO::~dbChannelIO ()
{
    dbAutoScanLock ( *this->addr.precord );

    /*
     * remove any subscriptions attached to this channel
     */
    tsDLIterBD < dbSubscriptionIO > iter = this->eventq.firstIter ();
    while ( iter.valid () ) {
        tsDLIterBD <dbSubscriptionIO> next = iter;
        next++;
        iter->cancel ();
        iter = next;
    }

    if ( this->pBlocker ) {
        this->pBlocker->destroy ();
    }

    if ( this->pGetCallbackCache ) {
        delete [] this->pGetCallbackCache;
    }
}

int dbChannelIO::read ( unsigned type, unsigned long count, void *pValue )
{
    if ( type > INT_MAX ) {
        return ECA_BADCOUNT;
    }
    if ( count > INT_MAX ) {
        return ECA_BADCOUNT;
    }
    int status = db_get_field ( &this->addr, static_cast <int> ( type ), 
                    pValue, static_cast <int> ( count ), 0);
    if ( status ) {
        return ECA_GETFAIL;
    }
    else { 
        return ECA_NORMAL;
    }
}

int dbChannelIO::read ( unsigned type, unsigned long count, cacNotify &notify ) 
{
    unsigned long size = dbr_size_n ( type, count );
    if ( type > INT_MAX ) {
        return ECA_BADCOUNT;
    }
    if ( count > INT_MAX ) {
        return ECA_BADCOUNT;
    }

    {
        dbAutoScanLock ( *this->addr.precord );
        if ( this->getCallbackCacheSize < size) {
            if ( this->pGetCallbackCache ) {
                delete [] this->pGetCallbackCache;
            }
            this->pGetCallbackCache = new char [size];
            if ( ! this->pGetCallbackCache ) {
                this->getCallbackCacheSize = 0ul;
                return ECA_ALLOCMEM;
            }
            this->getCallbackCacheSize = size;
        }
        int status = db_get_field ( &this->addr, static_cast <int> ( type ), 
                        this->pGetCallbackCache, static_cast <int> ( count ), 0);
        if ( status ) {
            notify.exceptionNotify ( *this, ECA_GETFAIL, 
                "db_get_field () completed unsuccessfuly" );
        }
        else { 
            notify.completionNotify ( *this, type, count, this->pGetCallbackCache );
        }
    }
    notify.release ();
    return ECA_NORMAL;
}

int dbChannelIO::write ( unsigned type, unsigned long count, const void *pValue )
{
    int status;
    if ( count > LONG_MAX ) {
        return ECA_BADCOUNT;
    }
    status = db_put_field ( &this->addr, type, pValue, static_cast <long> (count) );
    if (status) {
        return ECA_PUTFAIL;
    }
    else {
        return ECA_NORMAL;
    }
}

int dbChannelIO::write ( unsigned type, unsigned long count, 
                        const void *pValue, cacNotify &notify ) 
{
    if ( count > LONG_MAX ) {
        return ECA_BADCOUNT;
    }

    if ( ! this->pBlocker ) {
        dbAutoScanLock ( *this->addr.precord );
        if ( ! this->pBlocker ) {
            this->pBlocker = new dbPutNotifyBlocker ( *this );
            if ( ! this->pBlocker ) {
                return ECA_ALLOCMEM;
            }
        }
    }

    // must release the lock here so that this can block
    // for put notify completion without monopolizing the lock
    int status = this->pBlocker->initiatePutNotify ( notify, 
        this->addr, type, count, pValue );

    return status;
}

int dbChannelIO::subscribe ( unsigned type, unsigned long count, 
    unsigned mask, cacNotify &notify, cacNotifyIO *&pReturnIO ) 
{
    dbSubscriptionIO *pIO = new dbSubscriptionIO ( *this, notify, type, count );
    if ( ! pIO ) {
        return ECA_ALLOCMEM;
    }

    int status = pIO->begin ( this->addr, mask );
    if ( status == ECA_NORMAL ) {
        pReturnIO = pIO;
    }
    else {
        pIO->cancel ();
    }
    return status;
}

void dbChannelIO::show ( unsigned level ) const
{
    dbAutoScanLock ( *this->addr.precord );
    printf ("channel at %p attached to local database record %s\n", 
        static_cast <const void *> ( this ), this->addr.precord->name );

    if ( level > 0u ) {
        printf ( "\ttype %s, element count %li, field at %p\n",
            dbf_type_to_text ( this->addr.dbr_field_type ), this->addr.no_elements,
            this->addr.pfield );
    }
    if ( level > 1u ) {
        this->serviceIO.show ( level - 2u );
        printf ( "\tget callback cache at %p, with size %lu\n",
            this->pGetCallbackCache, this->getCallbackCacheSize );
        tsDLIterConstBD < dbSubscriptionIO > pItem = this->eventq.firstIter ();
        while ( pItem.valid () ) {
            pItem->show ( level - 2u );
            pItem++;
        }
        if ( this->pBlocker ) {
            this->pBlocker->show ( level - 2u );
        }
    }
}
