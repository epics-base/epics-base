
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
#include "osiMutex.h"

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbCAC.h"
#include "dbChannelIOIL.h"
#include "dbNotifyBlockerIL.h"

tsFreeList < dbChannelIO > dbChannelIO::freeList;

dbChannelIO::dbChannelIO ( cacChannel &chan, const dbAddr &addrIn, dbServiceIO &serviceIO ) :
    cacLocalChannelIO ( chan ), serviceIO ( serviceIO ), pGetCallbackCache ( 0 ), 
    pBlocker (0), getCallbackCacheSize ( 0ul ), addr ( addrIn )
{
    chan.attachIO ( *this );
    this->connectNotify ();
}

dbChannelIO::~dbChannelIO ()
{
    // this must go in the derived class's destructor because
    // this calls virtual functions in the cacChannelIO base
    this->ioReleaseNotify ();

    this->lock ();

    /*
     * remove any subscriptions attached to this channel
     */
    tsDLIterBD <dbSubscriptionIO> iter = this->eventq.first ();
    while ( iter.valid () ) {
        tsDLIterBD <dbSubscriptionIO> next = iter.itemAfter ();
        iter->destroy ();
        iter = next;
    }

    if ( this->pBlocker ) {
        this->pBlocker->destroy ();
    }

    if ( this->pGetCallbackCache ) {
        delete [] this->pGetCallbackCache;
    }

    this->unlock ();
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

    this->lock ();
    if ( this->getCallbackCacheSize < size) {
        if ( this->pGetCallbackCache ) {
            delete [] this->pGetCallbackCache;
        }
        this->pGetCallbackCache = new char [size];
        if ( ! this->pGetCallbackCache ) {
            this->getCallbackCacheSize = 0ul;
            this->unlock ();
            return ECA_ALLOCMEM;
        }
        this->getCallbackCacheSize = size;
    }
    int status = db_get_field ( &this->addr, static_cast <int> ( type ), 
                    this->pGetCallbackCache, static_cast <int> ( count ), 0);
    if ( status ) {
        notify.exceptionNotify ( ECA_GETFAIL, "db_get_field () completed unsuccessfuly" );
    }
    else { 
        notify.completionNotify ( type, count, this->pGetCallbackCache );
    }
    this->unlock ();
    notify.destroy ();
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
        this->lock ();
        if ( ! this->pBlocker ) {
            this->pBlocker = new dbPutNotifyBlocker ( *this );
            if ( ! this->pBlocker ) {
                this->unlock ();
                return ECA_ALLOCMEM;
            }
        }
        this->unlock ();
    }

    // must release the lock here so that this can block
    // for put notify completion without monopolizing the lock
    int status = this->pBlocker->initiatePutNotify ( notify, 
        this->addr, type, count, pValue );

    return status;
}

int dbChannelIO::subscribe ( unsigned type, unsigned long count, 
                            unsigned mask, cacNotify &notify ) 
{
    dbSubscriptionIO *pIO = new dbSubscriptionIO ( *this, notify, type, count );
    if ( ! pIO ) {
        return ECA_ALLOCMEM;
    }

    int status = pIO->begin ( this->addr, mask );
    if ( status != ECA_NORMAL ) {
        pIO->destroy ();
    }
    return status;
}

void dbChannelIO::lockOutstandingIO () const
{
}

void dbChannelIO::unlockOutstandingIO () const
{
}

