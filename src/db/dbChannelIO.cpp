
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

#include <limits.h>

#include "tsFreeList.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsSingleton.h"
#include "db_access.h"

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbCAC.h"
#include "dbChannelIO.h"
#include "dbPutNotifyBlocker.h"

epicsSingleton < tsFreeList < dbChannelIO > > dbChannelIO::pFreeList;

dbChannelIO::dbChannelIO ( cacChannelNotify &notify, 
    const dbAddr &addrIn, dbServiceIO &serviceIO ) :
    cacChannel ( notify ), serviceIO ( serviceIO ), 
    addr ( addrIn )
{
}

void dbChannelIO::initiateConnect ()
{
    this->notify ().connectNotify ();
}

dbChannelIO::~dbChannelIO ()
{
    this->serviceIO.destroyAllIO ( *this );
}

cacChannel::ioStatus dbChannelIO::read ( unsigned type, 
     unsigned long count, cacReadNotify &notify, ioid * ) 
{
    this->serviceIO.callReadNotify ( this->addr, 
        type, count, notify );
    return iosSynch;
}

void dbChannelIO::write ( unsigned type, unsigned long count, const void *pValue )
{
    int status;
    if ( count > LONG_MAX ) {
        throw outOfBounds();
    }
    status = db_put_field ( &this->addr, type, pValue, static_cast <long> (count) );
    if ( status ) {
        throw -1; 
    }
}

cacChannel::ioStatus dbChannelIO::write ( unsigned type, unsigned long count, 
                        const void *pValue, cacWriteNotify &notify, ioid *pId ) 
{
    if ( count > LONG_MAX ) {
        throw outOfBounds();
    }

    this->serviceIO.initiatePutNotify ( *this, this->addr, type, count, pValue, notify, pId );

    return iosAsynch;
}

void dbChannelIO::subscribe ( unsigned type, unsigned long count, 
    unsigned mask, cacStateNotify &notify, ioid *pId ) 
{   
    if ( type > INT_MAX ) {
        throw cacChannel::badType();
    }
    if ( count > INT_MAX ) {
        throw cacChannel::outOfBounds();
    }

    new dbSubscriptionIO ( this->serviceIO, *this, 
        this->addr, notify, type, count, mask, pId );
}

void dbChannelIO::ioCancel ( const ioid & id )
{
    this->serviceIO.ioCancel ( *this, id );
}

void dbChannelIO::ioShow ( const ioid &id, unsigned level ) const
{
    this->serviceIO.ioShow ( id, level );
}

void dbChannelIO::show ( unsigned level ) const
{
    printf ("channel at %p attached to local database record %s\n", 
        static_cast <const void *> ( this ), this->addr.precord->name );

    if ( level > 0u ) {
        printf ( "\ttype %s, element count %li, field at %p\n",
            dbf_type_to_text ( this->addr.dbr_field_type ), this->addr.no_elements,
            this->addr.pfield );
    }
    if ( level > 1u ) {
        this->serviceIO.show ( level - 2u );
        this->serviceIO.showAllIO ( *this, level - 2u );
    }
}



