
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

#include "tsFreeList.h"
#include "osiMutex.h"
#include "cadef.h"
#include "cacIO.h"

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbCAC.h"
#include "dbLock.h"
#include "dbCommon.h"

extern "C" unsigned short dbDBRnewToDBRold[DBR_ENUM+1];

tsFreeList < dbChannelIO > dbChannelIO::freeList;

dbChannelIO::dbChannelIO ( cacChannel &chan, const dbAddr &addrIn, dbServiceIO &serviceIO ) :
    cacChannelIO ( chan ), serviceIO ( serviceIO ), pGetCallbackCache ( 0 ), 
    getCallbackCacheSize ( 0ul ), addr ( addrIn )
{
    this->connectNotify ();
}

dbChannelIO::~dbChannelIO ()
{
    if ( this->pGetCallbackCache ) {
        delete [] this->pGetCallbackCache;
    }
}

void dbChannelIO::destroy () 
{
    delete this;
}

void * dbChannelIO::operator new ( size_t size )
{
    return dbChannelIO::freeList.allocate ( size );
}

void dbChannelIO::operator delete ( void *pCadaver, size_t size )
{
    dbChannelIO::freeList.release ( pCadaver, size );
}

const char *dbChannelIO::pName () const 
{
    return addr.precord->name;
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

    dbScanLock ( this->addr.precord );
    if ( this->getCallbackCacheSize < size) {
        if ( this->pGetCallbackCache ) {
            delete [] this->pGetCallbackCache;
        }
        this->pGetCallbackCache = new char [size];
        if ( ! this->pGetCallbackCache ) {
            this->getCallbackCacheSize = 0ul;
            dbScanUnlock ( this->addr.precord );
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
    notify.destroy ();
    dbScanUnlock ( this->addr.precord );
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
    dbPutNotifyIO *pIO;
    if ( count > LONG_MAX ) {
        return ECA_BADCOUNT;
    }
    pIO = new dbPutNotifyIO ( notify );
    if ( ! pIO ) {
        return ECA_ALLOCMEM;
    }
    int status = pIO->initiate ( this->addr, type, count, pValue );
    if ( status != ECA_NORMAL ) {
        pIO->destroy ();
    }
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

short dbChannelIO::nativeType () const 
{
    return dbDBRnewToDBRold[this->addr.field_type];
}

unsigned long dbChannelIO::nativeElementCount () const 
{
    if ( this->addr.no_elements >= 0u ) {
        return static_cast < unsigned long > ( this->addr.no_elements );
    }
    else {
        return 0u;
    }
}

void dbChannelIO::subscriptionUpdate ( unsigned type, unsigned long count, 
        const struct db_field_log *pfl, cacNotifyIO &notify )
{
    this->serviceIO.subscriptionUpdate ( this->addr, type, count, pfl, notify );
}

dbEventSubscription dbChannelIO::subscribe ( dbSubscriptionIO &subscr, unsigned mask )
{
    return this->serviceIO.subscribe ( this->addr, subscr, mask );
}
