
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
    pBlocker ( 0 ), addr ( addrIn )
{
}

void dbChannelIO::initiateConnect ()
{
    this->notify ().connectNotify ( *this );
}

dbChannelIO::~dbChannelIO ()
{
    while ( dbSubscriptionIO *pIO = this->eventq.get () ) {
        pIO->destroy ();
    }

    if ( this->pBlocker ) {
        this->pBlocker->destroy ();
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
    this->serviceIO.callReadNotify ( this->addr, type, count, 0, *this, notify );
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
    if ( status ) {
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
        dbAutoScanLock ( *this );
        if ( ! this->pBlocker ) {
            this->pBlocker = new dbPutNotifyBlocker ( *this );
            if ( ! this->pBlocker ) {
                return ECA_ALLOCMEM;
            }
        }
    }

    return this->pBlocker->initiatePutNotify ( notify, 
                        this->addr, type, count, pValue );
}

int dbChannelIO::subscribe ( unsigned type, unsigned long count, 
    unsigned mask, cacNotify &notify, cacNotifyIO *&pReturnIO ) 
{   
    int status;
    dbSubscriptionIO *pIO = new dbSubscriptionIO ( *this, notify, type, count );
    if ( pIO ) {
        status = pIO->begin ( mask );
        if ( status == ECA_NORMAL ) {
            dbAutoScanLock locker ( *this );
            this->eventq.add ( *pIO );
            pReturnIO = pIO;
        }
        else {
            pIO->destroy ();
        }
    }
    else {
        status = ECA_ALLOCMEM;
    }
    return status;
}

void dbChannelIO::show ( unsigned level ) const
{
    dbAutoScanLock locker ( *this );
    printf ("channel at %p attached to local database record %s\n", 
        static_cast <const void *> ( this ), this->addr.precord->name );

    if ( level > 0u ) {
        printf ( "\ttype %s, element count %li, field at %p\n",
            dbf_type_to_text ( this->addr.dbr_field_type ), this->addr.no_elements,
            this->addr.pfield );
    }
    if ( level > 1u ) {
        this->serviceIO.show ( level - 2u );
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
